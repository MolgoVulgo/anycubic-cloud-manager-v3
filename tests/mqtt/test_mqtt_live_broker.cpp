#include "infra/cloud/core/SessionProvider.h"
#include "infra/mqtt/core/MqttCredentialProvider.h"
#include "infra/mqtt/core/OpenSslCompat.h"
#include "infra/mqtt/core/TlsMaterialProvider.h"
#include "infra/mqtt/routing/MqttTopicBuilder.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QIODevice>
#include <QTimer>

#include <iostream>
#include <string>
#include <vector>

#ifdef ACCLOUD_WITH_MQTT
#include <QMqttClient>
#include <QMqttSubscription>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#endif

namespace {

std::string md5LowerHex(const std::string& input) {
    return QCryptographicHash::hash(QByteArray::fromStdString(input), QCryptographicHash::Md5)
        .toHex()
        .toStdString();
}

} // namespace

int main(int argc, char** argv) {
    const auto opensslCompat = accloud::mqtt::core::ensureOpenSslSecurityLevelCompat(
        accloud::mqtt::core::shouldEnableMqttOpenSslCompatFromEnv());
    if (!opensslCompat.ok) {
        std::cerr << "[WARN] OpenSSL compatibility profile not applied: "
                  << opensslCompat.code << " | " << opensslCompat.message << '\n';
    }

    QCoreApplication app(argc, argv);

#ifndef ACCLOUD_WITH_MQTT
    std::cout << "[SKIP] Qt MQTT unavailable in this build\n";
    return 0;
#else
    accloud::cloud::core::SessionProvider sessionProvider;
    const auto ctx = sessionProvider.loadRequestContext();
    if (!ctx.ok || ctx.context.email.empty() || ctx.context.userId.empty()
        || ctx.context.mqttAuthToken.empty()) {
        std::cerr << "[FAIL] Missing session context for live MQTT check\n";
        return 2;
    }

    accloud::mqtt::core::TlsMaterialProvider tlsProvider;
    const auto tls = tlsProvider.loadFromEnvironment();
    if (!tls.ok) {
        std::cerr << "[FAIL] TLS material loading failed: " << tls.code << " | " << tls.message << '\n';
        return 2;
    }

    accloud::mqtt::core::MqttCredentialInput input;
    input.brokerHost = "mqtt-universe.anycubic.com";
    input.email = ctx.context.email;
    input.userId = ctx.context.userId;
    input.authToken = ctx.context.mqttAuthToken;
    input.authMode = accloud::mqtt::core::MqttAuthMode::Slicer;

    accloud::mqtt::core::MqttCredentialProvider credentialProvider;
    const auto built = credentialProvider.buildCandidates(input);
    if (!built.ok || built.candidates.empty()) {
        std::cerr << "[FAIL] MQTT credential build failed: " << built.code << " | " << built.message << '\n';
        return 2;
    }
    const auto credentials = built.candidates.front().credentials;

    QMqttClient client;
    client.setHostname(QStringLiteral("mqtt-universe.anycubic.com"));
    client.setPort(8883);
    client.setProtocolVersion(QMqttClient::MQTT_3_1_1);
    client.setKeepAlive(1200);
    client.setCleanSession(true);
    client.setClientId(QString::fromStdString(credentials.clientId));
    client.setUsername(QString::fromStdString(credentials.username));
    client.setPassword(QString::fromStdString(credentials.password));

    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2);
    ssl.setPeerVerifyMode(tls.paths.allowInsecureTls ? QSslSocket::VerifyNone : QSslSocket::VerifyPeer);
    if (tls.paths.allowInsecureTls) {
        ssl.setBackendConfigurationOption(QByteArrayLiteral("CipherString"),
                                          QStringLiteral("ALL:@SECLEVEL=0"));
    }

    QFile caFile(QString::fromStdString(tls.paths.caCertificatePath.string()));
    if (!caFile.open(QIODevice::ReadOnly)) {
        std::cerr << "[FAIL] Unable to open CA cert file\n";
        return 2;
    }
    const QList<QSslCertificate> caCerts = QSslCertificate::fromData(caFile.readAll(), QSsl::Pem);
    if (caCerts.isEmpty()) {
        std::cerr << "[FAIL] CA cert parse failed\n";
        return 2;
    }
    ssl.setCaCertificates(caCerts);

    QFile certFile(QString::fromStdString(tls.paths.clientCertificatePath.string()));
    if (!certFile.open(QIODevice::ReadOnly)) {
        std::cerr << "[FAIL] Unable to open client cert file\n";
        return 2;
    }
    const QList<QSslCertificate> certs = QSslCertificate::fromData(certFile.readAll(), QSsl::Pem);
    if (certs.isEmpty()) {
        std::cerr << "[FAIL] Client cert parse failed\n";
        return 2;
    }
    ssl.setLocalCertificate(certs.front());

    QFile keyFile(QString::fromStdString(tls.paths.clientKeyPath.string()));
    if (!keyFile.open(QIODevice::ReadOnly)) {
        std::cerr << "[FAIL] Unable to open client key file\n";
        return 2;
    }
    const QSslKey privateKey(keyFile.readAll(), QSsl::Rsa, QSsl::Pem);
    if (privateKey.isNull()) {
        std::cerr << "[FAIL] Client key parse failed\n";
        return 2;
    }
    ssl.setPrivateKey(privateKey);

    bool connected = false;
    bool subscribeAck = false;
    int mqttError = 0;
    std::string failureReason;

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&]() {
        if (!connected) {
            failureReason = "connect_timeout";
        } else if (!subscribeAck) {
            failureReason = "subscribe_timeout";
        }
        app.quit();
    });

    QObject::connect(&client, &QMqttClient::errorChanged, &app, [&](QMqttClient::ClientError e) {
        if (e == QMqttClient::NoError) {
            return;
        }
        mqttError = static_cast<int>(e);
        failureReason = "mqtt_client_error";
        app.quit();
    });

    QObject::connect(&client, &QMqttClient::connected, &app, [&]() {
        connected = true;
        const auto userTopics = accloud::mqtt::routing::MqttTopicBuilder::buildUserReportTopics(
            ctx.context.userId,
            md5LowerHex(ctx.context.userId));
        if (userTopics.empty()) {
            subscribeAck = true;
            app.quit();
            return;
        }
        auto* subscription = client.subscribe(QString::fromStdString(userTopics.front()), 0);
        if (subscription == nullptr) {
            failureReason = "subscribe_failed";
            app.quit();
            return;
        }
        QObject::connect(subscription, &QMqttSubscription::stateChanged, &app,
                         [&](QMqttSubscription::SubscriptionState s) {
            if (s == QMqttSubscription::Subscribed) {
                subscribeAck = true;
                app.quit();
            }
         });
    });

    QObject::connect(&client, &QMqttClient::disconnected, &app, [&]() {
        if (!subscribeAck) {
            failureReason = "disconnected_before_subscribed";
            app.quit();
        }
    });

    timeout.start(15000);
    client.connectToHostEncrypted(ssl);
    app.exec();

    client.disconnectFromHost();

    if (connected && subscribeAck) {
        std::cout << "[PASS] Live MQTT broker connection and subscription succeeded\n";
        return 0;
    }

    std::cerr << "[FAIL] Live MQTT check failed: reason=" << failureReason
              << " mqtt_error=" << mqttError << '\n';
    return 1;
#endif
}
