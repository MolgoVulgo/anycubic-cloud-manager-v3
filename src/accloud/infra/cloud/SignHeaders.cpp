#include "SignHeaders.h"

#ifdef ACCLOUD_WITH_QT

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QtGlobal>
#include <QUuid>

namespace accloud::cloud {
namespace {

constexpr const char* kEnvPublicAppId = "ACCLOUD_PUBLIC_APP_ID";
constexpr const char* kEnvPublicAppSecret = "ACCLOUD_PUBLIC_APP_SECRET";
constexpr const char* kEnvPublicVersion = "ACCLOUD_PUBLIC_VERSION";
constexpr const char* kEnvPublicDeviceType = "ACCLOUD_PUBLIC_DEVICE_TYPE";
constexpr const char* kEnvPublicIsCn = "ACCLOUD_PUBLIC_IS_CN";
constexpr const char* kEnvRegion = "ACCLOUD_REGION";
constexpr const char* kEnvDeviceId = "ACCLOUD_DEVICE_ID";
constexpr const char* kEnvUserAgent = "ACCLOUD_USER_AGENT";
constexpr const char* kEnvClientVersion = "ACCLOUD_CLIENT_VERSION";

// Backward-compat env aliases used by previous C++ revisions.
constexpr const char* kEnvWorkbenchAppId = "ACCLOUD_WORKBENCH_APP_ID";
constexpr const char* kEnvWorkbenchAppSecret = "ACCLOUD_WORKBENCH_APP_SECRET";
constexpr const char* kEnvWorkbenchVersion = "ACCLOUD_WORKBENCH_VERSION";
constexpr const char* kEnvClientRegionLegacy = "ACCLOUD_CLIENT_REGION";
constexpr const char* kEnvClientDeviceIdLegacy = "ACCLOUD_CLIENT_DEVICE_ID";
constexpr const char* kEnvClientUserAgentLegacy = "ACCLOUD_CLIENT_USER_AGENT";

constexpr const char* kDefaultPublicAppId = "f9b3528877c94d5c9c5af32245db46ef";
constexpr const char* kDefaultPublicAppSecret = "0cf75926606049a3937f56b0373b99fb";
constexpr const char* kDefaultPublicVersion = "1.0.0";
constexpr const char* kDefaultPublicDeviceType = "web";
constexpr const char* kDefaultPublicIsCn = "2";
constexpr const char* kDefaultRegion = "global";
constexpr const char* kDefaultDeviceId = "manager-anycubic-cloud-dev";
constexpr const char* kDefaultUserAgent = "manager-anycubic-cloud/0.1.0";
constexpr const char* kDefaultClientVersion = "0.1.0";

QString envFirstNonEmpty(std::initializer_list<const char*> keys, const QString& fallback) {
    for (const char* key : keys) {
        const QString value = qEnvironmentVariable(key).trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return fallback;
}

} // namespace

WorkbenchHeaderStatus applyWorkbenchHeaders(QNetworkRequest& req, const QString& xxToken) {
    const QString appId = envFirstNonEmpty({kEnvPublicAppId, kEnvWorkbenchAppId},
                                           QString::fromLatin1(kDefaultPublicAppId));
    const QString appSecret = envFirstNonEmpty({kEnvPublicAppSecret, kEnvWorkbenchAppSecret},
                                               QString::fromLatin1(kDefaultPublicAppSecret));
    if (appId.isEmpty() || appSecret.isEmpty()) {
        return {false, "Missing Workbench app id/app secret"};
    }

    const QString version = envFirstNonEmpty({kEnvPublicVersion, kEnvWorkbenchVersion},
                                             QString::fromLatin1(kDefaultPublicVersion));
    const QString publicDeviceType = envFirstNonEmpty({kEnvPublicDeviceType},
                                                      QString::fromLatin1(kDefaultPublicDeviceType));
    const QString publicIsCn = envFirstNonEmpty({kEnvPublicIsCn},
                                                QString::fromLatin1(kDefaultPublicIsCn));
    const QString region = envFirstNonEmpty({kEnvRegion, kEnvClientRegionLegacy},
                                            QString::fromLatin1(kDefaultRegion));
    const QString deviceId = envFirstNonEmpty({kEnvDeviceId, kEnvClientDeviceIdLegacy},
                                              QString::fromLatin1(kDefaultDeviceId));
    const QString userAgent = envFirstNonEmpty({kEnvUserAgent, kEnvClientUserAgentLegacy},
                                               QString::fromLatin1(kDefaultUserAgent));
    const QString clientVersion = envFirstNonEmpty({kEnvClientVersion},
                                                   QString::fromLatin1(kDefaultClientVersion));

    const qint64 tsMs   = QDateTime::currentMSecsSinceEpoch();
    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Signature: md5(app_id + timestamp + version + app_secret + nonce + app_id)
    const QString sigInput =
        appId
        + QString::number(tsMs)
        + version
        + appSecret
        + nonce
        + appId;

    const QByteArray sig =
        QCryptographicHash::hash(sigInput.toUtf8(), QCryptographicHash::Md5).toHex();

    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
    req.setRawHeader("X-Client-Version", clientVersion.toUtf8());
    req.setRawHeader("X-Region", region.toUtf8());
    req.setRawHeader("X-Device-Id", deviceId.toUtf8());
    req.setRawHeader("X-Request-Id",   nonce.toLatin1());
    req.setRawHeader("XX-Device-Type", publicDeviceType.toUtf8());
    req.setRawHeader("XX-IS-CN",       publicIsCn.toUtf8());
    req.setRawHeader("XX-Version", version.toUtf8());
    req.setRawHeader("XX-Nonce",       nonce.toLatin1());
    req.setRawHeader("XX-Timestamp",   QByteArray::number(tsMs));
    req.setRawHeader("XX-Signature",   sig);
    if (!xxToken.isEmpty()) {
        req.setRawHeader("XX-Token", xxToken.toLatin1());
    }
    return {true, {}};
}

} // namespace accloud::cloud

#endif // ACCLOUD_WITH_QT
