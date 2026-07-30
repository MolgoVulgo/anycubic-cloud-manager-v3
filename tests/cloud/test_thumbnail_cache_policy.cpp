#include "app/ThumbnailCachePolicy.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool test_signed_urls_share_one_cache_identity() {
    const QString first = QStringLiteral(
        "https://User@Cloud-Slice-Prod.S3.us-east-2.amazonaws.com/cloud/a.jpg"
        "?X-Amz-Signature=first#fragment");
    const QString second = QStringLiteral(
        "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/a.jpg"
        "?X-Amz-Signature=second");

    return expect(accloud::thumbnail_cache::canonicalSourceIdentity(first)
                      == QStringLiteral(
                          "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/a.jpg"),
                  "canonical identity must remove credentials, query and fragment")
        && expect(accloud::thumbnail_cache::stableCacheKey(first)
                      == accloud::thumbnail_cache::stableCacheKey(second),
                  "rotating signed queries must reuse the same cache key");
}

bool test_different_remote_paths_keep_distinct_keys() {
    const QByteArray first = accloud::thumbnail_cache::stableCacheKey(
        QStringLiteral("https://cdn.example.test/cloud/a.jpg?token=1"));
    const QByteArray second = accloud::thumbnail_cache::stableCacheKey(
        QStringLiteral("https://cdn.example.test/cloud/b.jpg?token=1"));
    return expect(first != second, "different remote paths must not collide");
}

bool test_cached_local_candidate_precedes_remote_download() {
    const QString local = QStringLiteral("file:///tmp/accloud-thumb.png");
    const QString remote = QStringLiteral("https://cdn.example.test/cloud/a.jpg?token=1");
    const QStringList ordered = accloud::thumbnail_cache::orderedCandidates(
        QStringList{remote, local}, remote, remote, false);

    return expect(ordered.size() == 2, "candidate list must stay deduplicated")
        && expect(ordered.at(0) == local,
                  "normal refresh must validate the cached local image first")
        && expect(ordered.at(1) == remote,
                  "remote source must remain available as fallback");
}

bool test_validated_local_candidate_skips_second_validation() {
    const QString local = QStringLiteral("file:///tmp/accloud-thumb.png");
    const QString remote = QStringLiteral("https://cdn.example.test/cloud/a.jpg?token=1");

    return expect(accloud::thumbnail_cache::shouldReuseValidatedLocalCandidate(
                      local, true, false),
                  "a validated unchanged local candidate must be reused")
        && expect(!accloud::thumbnail_cache::shouldReuseValidatedLocalCandidate(
                      local, true, true),
                  "an explicit thumbnail refresh must bypass validation reuse")
        && expect(!accloud::thumbnail_cache::shouldReuseValidatedLocalCandidate(
                      local, false, false),
                  "an unvalidated local candidate must still be checked")
        && expect(!accloud::thumbnail_cache::shouldReuseValidatedLocalCandidate(
                      remote, true, false),
                  "a remote candidate must never use the local-validation shortcut");
}

bool test_explicit_thumbnail_force_keeps_remote_first() {
    const QString local = QStringLiteral("file:///tmp/accloud-thumb.png");
    const QString remote = QStringLiteral("https://cdn.example.test/cloud/a.jpg?token=1");
    const QStringList ordered = accloud::thumbnail_cache::orderedCandidates(
        QStringList{remote, local}, remote, local, true);

    return expect(ordered.size() == 2, "forced candidate list must stay deduplicated")
        && expect(ordered.at(0) == remote,
                  "explicit thumbnail refresh must preserve the remote-first order");
}

} // namespace

int main() {
    if (!test_signed_urls_share_one_cache_identity()
        || !test_different_remote_paths_keep_distinct_keys()
        || !test_cached_local_candidate_precedes_remote_download()
        || !test_validated_local_candidate_skips_second_validation()
        || !test_explicit_thumbnail_force_keeps_remote_first()) {
        return 1;
    }
    std::cout << "thumbnail cache policy tests passed\n";
    return 0;
}
