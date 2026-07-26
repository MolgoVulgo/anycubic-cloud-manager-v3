#include "MqttTlsPathResolver.h"

#include <system_error>
#include <vector>

namespace accloud::mqtt::core {
namespace {

constexpr int kMaxParentDepth = 12;

std::filesystem::path findFromCandidate(std::filesystem::path candidate) {
    std::error_code ec;
    if (candidate.empty()) {
        return {};
    }
    if (!std::filesystem::is_directory(candidate, ec)) {
        candidate = candidate.parent_path();
    }

    for (int depth = 0; depth < kMaxParentDepth && !candidate.empty(); ++depth) {
        const std::filesystem::path tlsDir = candidate / "resources" / "mqtt" / "tls";
        ec.clear();
        if (std::filesystem::is_directory(tlsDir, ec)) {
            const std::filesystem::path canonical = std::filesystem::weakly_canonical(tlsDir, ec);
            return ec ? tlsDir.lexically_normal() : canonical;
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }
    return {};
}

std::vector<std::filesystem::path> rootCandidates() {
    std::vector<std::filesystem::path> candidates;
#ifdef ACCLOUD_REPO_ROOT_PATH
    candidates.emplace_back(ACCLOUD_REPO_ROOT_PATH);
#endif

    std::error_code ec;
    const std::filesystem::path current = std::filesystem::current_path(ec);
    if (!ec) {
        candidates.push_back(current);
    }

    std::filesystem::path sourcePath(__FILE__);
    if (sourcePath.is_relative() && !current.empty()) {
        sourcePath = current / sourcePath;
    }
    candidates.push_back(sourcePath.parent_path());
    return candidates;
}

} // namespace

std::filesystem::path MqttTlsPathResolver::resolveDevTlsDirectory() {
    const auto candidates = rootCandidates();
    for (const auto& candidate : candidates) {
        const std::filesystem::path resolved = findFromCandidate(candidate);
        if (!resolved.empty()) {
            return resolved;
        }
    }

#ifdef ACCLOUD_REPO_ROOT_PATH
    return (std::filesystem::path(ACCLOUD_REPO_ROOT_PATH) / "resources" / "mqtt" / "tls")
        .lexically_normal();
#else
    return (std::filesystem::path("resources") / "mqtt" / "tls").lexically_normal();
#endif
}

std::filesystem::path MqttTlsPathResolver::resolveDevTlsFile(const char* preferredFilename,
                                                             const char* legacyFilename) {
    const std::filesystem::path tlsDir = resolveDevTlsDirectory();
    const std::filesystem::path preferred = tlsDir / preferredFilename;
    if (std::filesystem::exists(preferred)) {
        return preferred;
    }
    const std::filesystem::path legacy = tlsDir / legacyFilename;
    if (std::filesystem::exists(legacy)) {
        return legacy;
    }
    return preferred;
}

} // namespace accloud::mqtt::core
