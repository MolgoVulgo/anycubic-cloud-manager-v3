#include "infra/cloud/thumbnail/ThumbnailValidation.h"

#include <QBuffer>
#include <QByteArray>
#include <QImage>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    const QByteArray placeholder(97, '\0');
    const auto tooSmall = accloud::cloud::thumbnail::validateThumbnailBytes(placeholder);
    require(!tooSmall.valid, "97-byte placeholder must be invalid");
    require(tooSmall.tooSmall, "97-byte placeholder must be classified as too small");

    const QByteArray invalid(128, 'x');
    const auto notImage = accloud::cloud::thumbnail::validateThumbnailBytes(invalid);
    require(!notImage.valid, "non-image payload must be invalid");
    require(!notImage.tooSmall, "128-byte payload must not be classified as too small");

    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(qRgba(20, 40, 60, 255));
    QByteArray png;
    QBuffer buffer(&png);
    require(buffer.open(QIODevice::WriteOnly), "PNG buffer must open");
    require(image.save(&buffer, "PNG"), "PNG image must serialize");
    const auto valid = accloud::cloud::thumbnail::validateThumbnailBytes(png);
    require(valid.valid, "decodable PNG must be valid");
    require(!valid.tooSmall, "valid PNG must not be classified as too small");

    std::cout << "thumbnail validation tests passed\n";
    return 0;
}
