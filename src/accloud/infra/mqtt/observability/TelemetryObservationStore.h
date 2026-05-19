#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace accloud::mqtt::observability {

struct TelemetryObservation {
    std::string signature;
    std::string topic;
    std::string printerKey;
    std::string payload;
    std::string disposition;
    std::string reason;
    std::string lastSeenIso;
    std::size_t count{0};
};

class TelemetryObservationStore {
public:
    static TelemetryObservationStore& instance();

    void observe(const std::string& signature,
                 const std::string& topic,
                 const std::string& printerKey,
                 const std::string& payload,
                 const std::string& disposition,
                 const std::string& reason);

    [[nodiscard]] std::optional<TelemetryObservation> getBySignature(
        const std::string& signature) const;
    [[nodiscard]] std::vector<TelemetryObservation> topByCount(std::size_t maxItems) const;
    [[nodiscard]] std::size_t size() const;
    void clear();

private:
    mutable std::mutex m_mutex;
    std::map<std::string, TelemetryObservation> m_bySignature;
};

} // namespace accloud::mqtt::observability
