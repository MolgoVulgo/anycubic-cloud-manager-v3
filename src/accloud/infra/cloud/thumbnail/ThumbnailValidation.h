#pragma once

#include <cstddef>

#ifdef ACCLOUD_WITH_QT
#include <QByteArray>
#include <QString>
#endif

namespace accloud::cloud::thumbnail {

inline constexpr std::size_t kMinimumValidThumbnailBytes = 100;

struct ThumbnailValidationResult {
    bool valid{false};
    bool tooSmall{false};
    bool decodable{false};
    std::size_t sizeBytes{0};
#ifdef ACCLOUD_WITH_QT
    QString error;
#endif
};

#ifdef ACCLOUD_WITH_QT
[[nodiscard]] ThumbnailValidationResult validateThumbnailBytes(const QByteArray& bytes);
#endif

} // namespace accloud::cloud::thumbnail
