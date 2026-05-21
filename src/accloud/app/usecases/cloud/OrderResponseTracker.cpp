#include "OrderResponseTracker.h"

#include "infra/mqtt/observability/MqttTelemetry.h"

#include <array>
#include <iterator>
#include <random>
#include <sstream>
#include <vector>

namespace accloud::usecases::cloud {
namespace {

using Clock = std::chrono::steady_clock;

} // namespace

OrderResponseTracker& OrderResponseTracker::instance() {
    static OrderResponseTracker tracker;
    return tracker;
}

TrackerOpenResult OrderResponseTracker::open(const TrackerOpenRequest& request) {
    if (request.printerId.empty()) {
        return {false, {}, "tracker_invalid_printer", "printer_id is required"};
    }
    const auto timeout = request.timeout <= std::chrono::milliseconds(0)
        ? std::chrono::seconds(30)
        : request.timeout;

    std::scoped_lock lock(m_mutex);
    const std::string fallbackKey = makeFallbackKey(request.printerId, request.correlationClass);
    if (request.msgid.empty()) {
        const auto [begin, end] = m_fallbackToTicket.equal_range(fallbackKey);
        const auto count = static_cast<std::size_t>(std::distance(begin, end));
        if (count >= 1) {
            return {false, {}, "tracker_ambiguous_fallback",
                    "Another fallback-correlated command is already in flight for printer/correlation class"};
        }
    } else if (m_msgidToTicket.find(request.msgid) != m_msgidToTicket.end()) {
        return {false, {}, "tracker_duplicate_msgid", "msgid already tracked"};
    }

    PendingOrder order;
    order.ticket = makeTicket();
    order.printerId = request.printerId;
    order.correlationClass = request.correlationClass;
    order.msgid = request.msgid;
    order.deadline = Clock::now() + timeout;
    m_byTicket[order.ticket] = order;
    if (!order.msgid.empty()) {
        m_msgidToTicket[order.msgid] = order.ticket;
    }
    m_fallbackToTicket.emplace(fallbackKey, order.ticket);
    accloud::mqtt::observability::MqttTelemetry::instance().setPendingOrders(m_byTicket.size());
    return {true, order.ticket, "ok", "tracked"};
}

TrackerResolveResult OrderResponseTracker::resolveByMsgid(const std::string& msgid,
                                                          bool success,
                                                          const std::string& reason) {
    if (msgid.empty()) {
        return {CorrelationOutcome::Uncorrelated, {}, "empty_msgid"};
    }

    std::scoped_lock lock(m_mutex);
    auto mapIt = m_msgidToTicket.find(msgid);
    if (mapIt == m_msgidToTicket.end()) {
        return {CorrelationOutcome::Uncorrelated, {}, "msgid_not_found"};
    }
    auto orderIt = m_byTicket.find(mapIt->second);
    if (orderIt == m_byTicket.end()) {
        m_msgidToTicket.erase(mapIt);
        return {CorrelationOutcome::Uncorrelated, {}, "ticket_not_found"};
    }
    return finalize(orderIt->second, success ? CorrelationOutcome::Success : CorrelationOutcome::Failure, reason);
}

TrackerResolveResult OrderResponseTracker::resolveByFallback(const std::string& printerId,
                                                             CorrelationClass correlationClass,
                                                             bool success,
                                                             const std::string& reason) {
    if (printerId.empty()) {
        return {CorrelationOutcome::Uncorrelated, {}, "empty_printer"};
    }
    std::scoped_lock lock(m_mutex);
    const std::string key = makeFallbackKey(printerId, correlationClass);
    const auto [begin, end] = m_fallbackToTicket.equal_range(key);
    std::vector<std::string> tickets;
    for (auto it = begin; it != end; ++it) {
        if (m_byTicket.find(it->second) != m_byTicket.end()) {
            tickets.push_back(it->second);
        }
    }

    if (tickets.empty()) {
        return {CorrelationOutcome::Uncorrelated, {}, "fallback_no_candidate"};
    }
    if (tickets.size() > 1) {
        return {CorrelationOutcome::AmbiguousFallback, {}, "fallback_ambiguous"};
    }

    auto orderIt = m_byTicket.find(tickets.front());
    if (orderIt == m_byTicket.end()) {
        return {CorrelationOutcome::Uncorrelated, {}, "fallback_candidate_not_found"};
    }
    return finalize(orderIt->second, success ? CorrelationOutcome::Success : CorrelationOutcome::Failure, reason);
}

std::size_t OrderResponseTracker::expireTimeouts() {
    std::scoped_lock lock(m_mutex);
    const auto now = Clock::now();
    std::size_t expired = 0;
    std::vector<std::string> timeoutTickets;
    timeoutTickets.reserve(m_byTicket.size());
    for (const auto& [ticket, order] : m_byTicket) {
        if (order.deadline <= now && order.outcome == CorrelationOutcome::Pending) {
            timeoutTickets.push_back(ticket);
        }
    }
    for (const auto& ticket : timeoutTickets) {
        auto it = m_byTicket.find(ticket);
        if (it == m_byTicket.end()) {
            continue;
        }
        finalize(it->second, CorrelationOutcome::Timeout, "timeout");
        ++expired;
    }
    return expired;
}

std::optional<TrackedOrderSnapshot> OrderResponseTracker::findByTicket(const std::string& ticket) const {
    std::scoped_lock lock(m_mutex);
    const auto it = m_byTicket.find(ticket);
    if (it == m_byTicket.end()) {
        return std::nullopt;
    }
    const PendingOrder& o = it->second;
    return TrackedOrderSnapshot{
        o.ticket, o.printerId, o.correlationClass, o.msgid, o.outcome, o.reason,
    };
}

std::size_t OrderResponseTracker::pendingCount() const {
    std::scoped_lock lock(m_mutex);
    return m_byTicket.size();
}

void OrderResponseTracker::clear() {
    std::scoped_lock lock(m_mutex);
    m_byTicket.clear();
    m_msgidToTicket.clear();
    m_fallbackToTicket.clear();
}

std::string OrderResponseTracker::correlationClassToString(CorrelationClass value) {
    switch (value) {
        case CorrelationClass::PrintStart: return "PrintStart";
        case CorrelationClass::PrintPause: return "PrintPause";
        case CorrelationClass::PrintResume: return "PrintResume";
        case CorrelationClass::PrintStop: return "PrintStop";
        case CorrelationClass::ListLocalFiles: return "ListLocalFiles";
        case CorrelationClass::ListUdiskFiles: return "ListUdiskFiles";
        case CorrelationClass::DeleteLocalFile: return "DeleteLocalFile";
        case CorrelationClass::DeleteUdiskFile: return "DeleteUdiskFile";
        case CorrelationClass::PeripheralQuery: return "PeripheralQuery";
    }
    return "PrintStart";
}

std::string OrderResponseTracker::outcomeToString(CorrelationOutcome value) {
    switch (value) {
        case CorrelationOutcome::Pending: return "Pending";
        case CorrelationOutcome::Success: return "Success";
        case CorrelationOutcome::Failure: return "Failure";
        case CorrelationOutcome::Timeout: return "Timeout";
        case CorrelationOutcome::AmbiguousFallback: return "AmbiguousFallback";
        case CorrelationOutcome::Uncorrelated: return "Uncorrelated";
    }
    return "Uncorrelated";
}

std::string OrderResponseTracker::makeFallbackKey(const std::string& printerId,
                                                  CorrelationClass correlationClass) {
    return printerId + "|" + correlationClassToString(correlationClass);
}

std::string OrderResponseTracker::makeTicket() {
    static std::random_device rd;
    static std::mt19937_64 rng(rd());
    static std::uniform_int_distribution<std::uint64_t> dist;
    const auto value = dist(rng);
    std::ostringstream ss;
    ss << "trk_" << std::hex << value;
    return ss.str();
}

TrackerResolveResult OrderResponseTracker::finalize(PendingOrder& order,
                                                    CorrelationOutcome outcome,
                                                    const std::string& reason) {
    order.outcome = outcome;
    order.reason = reason;
    const std::string ticket = order.ticket;
    const std::string msgid = order.msgid;
    const std::string fallbackKey = makeFallbackKey(order.printerId, order.correlationClass);

    if (!msgid.empty()) {
        auto msgIt = m_msgidToTicket.find(msgid);
        if (msgIt != m_msgidToTicket.end() && msgIt->second == ticket) {
            m_msgidToTicket.erase(msgIt);
        }
    }

    auto [begin, end] = m_fallbackToTicket.equal_range(fallbackKey);
    for (auto it = begin; it != end; ) {
        if (it->second == ticket) {
            it = m_fallbackToTicket.erase(it);
        } else {
            ++it;
        }
    }
    m_byTicket.erase(ticket);
    accloud::mqtt::observability::MqttTelemetry::instance().setPendingOrders(m_byTicket.size());
    return {outcome, ticket, reason};
}

} // namespace accloud::usecases::cloud
