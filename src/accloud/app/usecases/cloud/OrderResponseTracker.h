#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace accloud::usecases::cloud {

enum class CorrelationClass {
    PrintStart,
    PrintPause,
    PrintResume,
    PrintStop,
    ListLocalFiles,
    ListUdiskFiles,
    DeleteLocalFile,
    DeleteUdiskFile,
    PeripheralQuery,
};

enum class CorrelationOutcome {
    Pending,
    Success,
    Failure,
    Timeout,
    AmbiguousFallback,
    Uncorrelated,
};

struct TrackerOpenRequest {
    std::string printerId;
    CorrelationClass correlationClass{CorrelationClass::PrintStart};
    std::string msgid;
    std::chrono::milliseconds timeout{std::chrono::seconds(30)};
};

struct TrackerOpenResult {
    bool ok{false};
    std::string ticket;
    std::string code;
    std::string message;
};

struct TrackerResolveResult {
    CorrelationOutcome outcome{CorrelationOutcome::Uncorrelated};
    std::string ticket;
    std::string message;
};

struct TrackedOrderSnapshot {
    std::string ticket;
    std::string printerId;
    CorrelationClass correlationClass{CorrelationClass::PrintStart};
    std::string msgid;
    CorrelationOutcome outcome{CorrelationOutcome::Pending};
    std::string reason;
};

class OrderResponseTracker {
public:
    static OrderResponseTracker& instance();

    TrackerOpenResult open(const TrackerOpenRequest& request);
    TrackerResolveResult resolveByMsgid(const std::string& msgid, bool success, const std::string& reason);
    TrackerResolveResult resolveByFallback(const std::string& printerId,
                                           CorrelationClass correlationClass,
                                           bool success,
                                           const std::string& reason);

    std::size_t expireTimeouts();
    std::optional<TrackedOrderSnapshot> findByTicket(const std::string& ticket) const;
    std::size_t pendingCount() const;
    void clear();

    static std::string correlationClassToString(CorrelationClass value);
    static std::string outcomeToString(CorrelationOutcome value);

private:
    struct PendingOrder {
        std::string ticket;
        std::string printerId;
        CorrelationClass correlationClass{CorrelationClass::PrintStart};
        std::string msgid;
        std::chrono::steady_clock::time_point deadline;
        CorrelationOutcome outcome{CorrelationOutcome::Pending};
        std::string reason;
    };

    static std::string makeFallbackKey(const std::string& printerId, CorrelationClass correlationClass);
    static std::string makeTicket();
    TrackerResolveResult finalize(PendingOrder& order, CorrelationOutcome outcome, const std::string& reason);

    mutable std::mutex m_mutex;
    std::map<std::string, PendingOrder> m_byTicket;
    std::map<std::string, std::string> m_msgidToTicket;
    std::multimap<std::string, std::string> m_fallbackToTicket;
};

} // namespace accloud::usecases::cloud

