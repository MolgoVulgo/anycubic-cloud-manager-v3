#include "ThumbnailValidation.h"

#ifdef ACCLOUD_WITH_QT

#include <QBuffer>
#include <QImage>
#include <QImageReader>

namespace accloud::cloud::thumbnail {

ThumbnailValidationResult validateThumbnailBytes(const QByteArray& bytes) {
    ThumbnailValidationResult out;
    out.sizeBytes = static_cast<std::size_t>(bytes.size());
    if (out.sizeBytes < kMinimumValidThumbnailBytes) {
        out.tooSmall = true;
        out.error = QStringLiteral("thumbnail_too_small");
        return out;
    }

    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("thumbnail_buffer_open_failed");
        return out;
    }

    QImageReader reader(&buffer);
    out.decodable = reader.canRead();
    if (!out.decodable) {
        out.error = QStringLiteral("thumbnail_not_decodable");
        return out;
    }

    const QImage image = reader.read();
    if (image.isNull()) {
        out.decodable = false;
        out.error = QStringLiteral("thumbnail_decode_failed");
        return out;
    }

    out.valid = true;
    return out;
}

} // namespace accloud::cloud::thumbnail

#endif
