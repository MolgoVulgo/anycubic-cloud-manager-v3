#include "TelemetryObservationStore.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

namespace accloud::mqtt::observability {
namespace {

constexpr std::size_t kMaxPayloadChars = 2048;
constexpr std::size_t kMaxObservations = 1024;

std::string nowIsoUtc() {
    const auto now = std::chrono::system_clock::now();
    const auto secTp = std::chrono::time_point_cast<std::chrono::seconds>(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - secTp).count();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms << 'Z';
    return out.str();
}

std::string truncatePayload(const std::string& payload) {
    if (payload.size() <= kMaxPayloadChars) {
        return payload;
    }
    return payload.substr(0, kMaxPayloadChars);
}

} // namespace

TelemetryObservationStore& TelemetryObservationStore::instance() {
    static TelemetryObservationStore store;
    return store;
}

void TelemetryObservationStore::observe(const std::string& signature,
                                        const std::string& topic,
                                        const std::string& printerKey,
                                        const std::string& payload,
                                        const std::string& disposition,
                                        const std::string& reason) {
    const std::string key = signature.empty() ? std::string("unknown|unknown|-") : signature;
    std::scoped_lock lock(m_mutex);

    TelemetryObservation& entry = m_bySignature[key];
    if (entry.signature.empty()) {
        entry.signature = key;
    }
    entry.topic = topic;
    entry.printerKey = printerKey;
    entry.payload = truncatePayload(payload);
    entry.disposition = disposition;
    entry.reason = reason;
    entry.lastSeenIso = nowIsoUtc();
    ++entry.count;

    if (m_bySignature.size() <= kMaxObservations) {
        return;
    }

    auto evictIt = std::min_element(m_bySignature.begin(), m_bySignature.end(), [](const auto& a, const auto& b) {
        if (a.second.count != b.second.count) {
            return a.second.count < b.second.count;
        }
        return a.second.lastSeenIso < b.second.lastSeenIso;
    });
    if (evictIt != m_bySignature.end()) {
        m_bySignature.erase(evictIt);
    }
}

std::optional<TelemetryObservation> TelemetryObservationStore::getBySignature(
    const std::string& signature) const {
    if (signature.empty()) {
        return std::nullopt;
    }
    std::scoped_lock lock(m_mutex);
    const auto it = m_bySignature.find(signature);
    if (it == m_bySignature.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<TelemetryObservation> TelemetryObservationStore::topByCount(std::size_t maxItems) const {
    std::scoped_lock lock(m_mutex);
    std::vector<TelemetryObservation> out;
    out.reserve(m_bySignature.size());
    for (const auto& [_, value] : m_bySignature) {
        out.push_back(value);
    }
    std::sort(out.begin(), out.end(), [](const TelemetryObservation& a, const TelemetryObservation& b) {
        if (a.count != b.count) {
            return a.count > b.count;
        }
        return a.lastSeenIso > b.lastSeenIso;
    });
    if (maxItems > 0 && out.size() > maxItems) {
        out.resize(maxItems);
    }
    return out;
}

std::size_t TelemetryObservationStore::size() const {
    std::scoped_lock lock(m_mutex);
    return m_bySignature.size();
}

void TelemetryObservationStore::clear() {
    std::scoped_lock lock(m_mutex);
    m_bySignature.clear();
}

} // namespace accloud::mqtt::observability
