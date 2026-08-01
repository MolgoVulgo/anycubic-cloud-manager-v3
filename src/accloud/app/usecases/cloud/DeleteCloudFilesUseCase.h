#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace accloud::usecases::cloud {

struct CloudFileDeleteItem {
    std::string fileId;
    std::string fileName;
};

struct CloudFileDeleteFailure {
    std::string fileId;
    std::string message;
};

struct CloudFilesDeleteSummary {
    std::size_t requested{0};
    std::size_t completed{0};
    std::size_t succeeded{0};
    std::vector<CloudFileDeleteFailure> failures;
    bool cancelled{false};
};

class DeleteCloudFilesUseCase {
public:
    [[nodiscard]] bool start(std::vector<CloudFileDeleteItem> files);
    void cancel();

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool cancelRequested() const noexcept;
    [[nodiscard]] std::optional<CloudFileDeleteItem> current() const;
    [[nodiscard]] std::size_t completed() const noexcept;
    [[nodiscard]] std::size_t total() const noexcept;
    [[nodiscard]] std::size_t succeeded() const noexcept;
    [[nodiscard]] CloudFilesDeleteSummary summary() const;

    // Returns false for stale/unrelated completions and leaves the workflow unchanged.
    [[nodiscard]] bool handleResult(const std::string& fileId,
                                    bool success,
                                    std::string message = {});

private:
    void reset();
    void advance();

    std::vector<CloudFileDeleteItem> m_files;
    std::size_t m_nextIndex{0};
    std::size_t m_completed{0};
    std::size_t m_succeeded{0};
    std::vector<CloudFileDeleteFailure> m_failures;
    std::optional<CloudFileDeleteItem> m_current;
    bool m_running{false};
    bool m_cancelRequested{false};
};

} // namespace accloud::usecases::cloud
