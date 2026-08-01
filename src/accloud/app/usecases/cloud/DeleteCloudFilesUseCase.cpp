#include "DeleteCloudFilesUseCase.h"

#include <utility>

namespace accloud::usecases::cloud {

bool DeleteCloudFilesUseCase::start(std::vector<CloudFileDeleteItem> files) {
    if (m_running) {
        return false;
    }

    reset();
    m_files.reserve(files.size());
    for (auto& file : files) {
        if (file.fileId.empty()) {
            continue;
        }
        m_files.push_back(std::move(file));
    }
    if (m_files.empty()) {
        return false;
    }

    m_running = true;
    advance();
    return true;
}

void DeleteCloudFilesUseCase::cancel() {
    if (m_running) {
        m_cancelRequested = true;
    }
}

bool DeleteCloudFilesUseCase::running() const noexcept {
    return m_running;
}

bool DeleteCloudFilesUseCase::cancelRequested() const noexcept {
    return m_cancelRequested;
}

std::optional<CloudFileDeleteItem> DeleteCloudFilesUseCase::current() const {
    return m_current;
}

std::size_t DeleteCloudFilesUseCase::completed() const noexcept {
    return m_completed;
}

std::size_t DeleteCloudFilesUseCase::total() const noexcept {
    return m_files.size();
}

std::size_t DeleteCloudFilesUseCase::succeeded() const noexcept {
    return m_succeeded;
}

CloudFilesDeleteSummary DeleteCloudFilesUseCase::summary() const {
    return {
        m_files.size(),
        m_completed,
        m_succeeded,
        m_failures,
        m_cancelRequested,
    };
}

bool DeleteCloudFilesUseCase::handleResult(const std::string& fileId,
                                           bool success,
                                           std::string message) {
    if (!m_running || !m_current.has_value() || fileId != m_current->fileId) {
        return false;
    }

    ++m_completed;
    if (success) {
        ++m_succeeded;
    } else {
        m_failures.push_back({fileId, std::move(message)});
    }

    m_current.reset();
    if (m_cancelRequested || m_nextIndex >= m_files.size()) {
        m_running = false;
        return true;
    }

    advance();
    return true;
}

void DeleteCloudFilesUseCase::reset() {
    m_files.clear();
    m_nextIndex = 0;
    m_completed = 0;
    m_succeeded = 0;
    m_failures.clear();
    m_current.reset();
    m_running = false;
    m_cancelRequested = false;
}

void DeleteCloudFilesUseCase::advance() {
    if (!m_running || m_nextIndex >= m_files.size()) {
        m_current.reset();
        m_running = false;
        return;
    }
    m_current = m_files[m_nextIndex++];
}

} // namespace accloud::usecases::cloud
