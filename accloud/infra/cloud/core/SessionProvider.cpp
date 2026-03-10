#include "SessionProvider.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cctype>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#ifdef ACCLOUD_WITH_QT
#include "infra/cloud/core/EndpointRegistry.h"
#include "infra/cloud/core/HttpClient.h"
#include "infra/cloud/core/ResponseEnvelopeParser.h"
#include "infra/cloud/core/WorkbenchRequestBuilder.h"
#endif

namespace accloud::cloud::core {
namespace {

std::string firstNonEmptyToken(const cloud::SessionData& session,
                               std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const auto it = session.tokens.find(std::string(key));
        if (it != session.tokens.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return {};
}

std::string envOrFallback(const char* envKey, const std::string& fallback) {
    const char* value = std::getenv(envKey);
    if (value != nullptr && *value != '\0') {
        return value;
    }
    return fallback;
}

std::optional<std::string> firstNonEmptyValue(
    const nlohmann::json& object,
    const std::initializer_list<const char*>& keys) {
    if (!object.is_object()) {
        return std::nullopt;
    }
    for (const char* key : keys) {
        if (!object.contains(key) || object[key].is_null()) {
            continue;
        }
        if (object[key].is_string()) {
            const std::string value = object[key].get<std::string>();
            if (!value.empty()) {
                return value;
            }
            continue;
        }
        if (object[key].is_number_integer()) {
            return std::to_string(object[key].get<long long>());
        }
    }
    return std::nullopt;
}

std::string normalizeLookupKey(std::string_view key) {
    std::string out;
    out.reserve(key.size());
    for (char c : key) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return out;
}

void flattenScalarFields(const nlohmann::json& value, std::map<std::string, std::string>& out) {
    if (value.is_object()) {
        for (const auto& [key, child] : value.items()) {
            if (child.is_string()) {
                const std::string str = child.get<std::string>();
                if (!str.empty()) {
                    out.emplace(key, str);
                    const std::string normalized = normalizeLookupKey(key);
                    if (!normalized.empty()) {
                        out.emplace(normalized, str);
                    }
                }
            } else if (child.is_number_integer()) {
                const std::string number = std::to_string(child.get<long long>());
                out.emplace(key, number);
                const std::string normalized = normalizeLookupKey(key);
                if (!normalized.empty()) {
                    out.emplace(normalized, number);
                }
            }
            flattenScalarFields(child, out);
        }
    } else if (value.is_array()) {
        for (const auto& child : value) {
            flattenScalarFields(child, out);
        }
    }
}

std::optional<std::string> firstNonEmptyFlat(
    const std::map<std::string, std::string>& flat,
    const std::initializer_list<const char*>& keys) {
    for (const char* key : keys) {
        auto it = flat.find(key);
        if (it != flat.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return std::nullopt;
}

struct RemoteSessionMetadata {
    std::string email;
    std::string userId;
    std::string modeAuth;
};

#ifdef ACCLOUD_WITH_QT
std::optional<RemoteSessionMetadata> fetchRemoteSessionMetadata(const std::string& accessToken,
                                                                const std::string& xxToken) {
    if (accessToken.empty()) {
        return std::nullopt;
    }
    const auto endpoint = EndpointRegistry::instance().find(EndpointId::AuthCheckSession);
    if (!endpoint.has_value()) {
        return std::nullopt;
    }

    const WorkbenchRequestBuilder builder;
    const auto request = builder.build(*endpoint, accessToken, xxToken, {}, QByteArray("{}"));
    if (!request.has_value()) {
        return std::nullopt;
    }

    const HttpClient client;
    const HttpResponse response = client.execute(*request);
    if (!response.ok) {
        return std::nullopt;
    }

    const ResponseEnvelopeParser parser;
    const EnvelopeParseResult envelope = parser.parse(response.body);
    if (!envelope.jsonValid || !envelope.envelopePresent || !envelope.success) {
        return std::nullopt;
    }

    std::map<std::string, std::string> flat;
    flattenScalarFields(envelope.data, flat);
    RemoteSessionMetadata meta;
    if (auto value = firstNonEmptyFlat(flat, {"email", "user_email", "useremail", "mail"});
        value.has_value()) {
        meta.email = *value;
    }
    if (auto value = firstNonEmptyFlat(flat, {"user_id", "uid", "id", "userId", "userid"});
        value.has_value()) {
        meta.userId = *value;
    }
    if (auto value = firstNonEmptyFlat(flat, {"mode_auth", "auth_mode", "modeAuth", "modeauth", "authmode"});
        value.has_value()) {
        meta.modeAuth = *value;
    }
    if (meta.email.empty() && meta.userId.empty() && meta.modeAuth.empty()) {
        return std::nullopt;
    }
    return meta;
}
#endif

std::optional<std::string> decodeBase64Url(std::string value) {
    for (char& c : value) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while ((value.size() % 4) != 0) {
        value.push_back('=');
    }

    static constexpr std::array<int, 256> kTable = [] {
        std::array<int, 256> table{};
        table.fill(-1);
        const std::string alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (std::size_t i = 0; i < alphabet.size(); ++i) {
            table[static_cast<unsigned char>(alphabet[i])] = static_cast<int>(i);
        }
        return table;
    }();

    std::string out;
    int val = 0;
    int bits = -8;
    for (char ch : value) {
        if (ch == '=') {
            break;
        }
        const int decoded = kTable[static_cast<unsigned char>(ch)];
        if (decoded < 0) {
            return std::nullopt;
        }
        val = (val << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

nlohmann::json decodeJwtPayload(const std::string& token) {
    const std::size_t firstDot = token.find('.');
    if (firstDot == std::string::npos) {
        return {};
    }
    const std::size_t secondDot = token.find('.', firstDot + 1);
    if (secondDot == std::string::npos || secondDot <= firstDot + 1) {
        return {};
    }

    const std::string payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
    const auto decoded = decodeBase64Url(payloadB64);
    if (!decoded.has_value()) {
        return {};
    }
    nlohmann::json payload = nlohmann::json::parse(*decoded, nullptr, false);
    if (payload.is_discarded() || !payload.is_object()) {
        return {};
    }
    return payload;
}

} // namespace

cloud::LoadSessionResult SessionProvider::loadCurrentSession(
    const std::optional<std::filesystem::path>& pathOverride) const {
    return cloud::loadSessionFile(pathOverride);
}

std::optional<std::string> SessionProvider::requireAccessToken(const cloud::SessionData& session) const {
    const auto accessTokenIt = session.tokens.find("access_token");
    if (accessTokenIt == session.tokens.end() || accessTokenIt->second.empty()) {
        return std::nullopt;
    }
    return accessTokenIt->second;
}

std::string SessionProvider::optionalXxToken(const cloud::SessionData& session) const {
    const auto tokenIt = session.tokens.find("token");
    if (tokenIt != session.tokens.end() && !tokenIt->second.empty()) {
        return tokenIt->second;
    }
    return {};
}

std::string SessionProvider::optionalMqttAuthToken(const cloud::SessionData& session) const {
    const auto authTokenIt = session.tokens.find("auth_token");
    if (authTokenIt != session.tokens.end() && !authTokenIt->second.empty()) {
        return authTokenIt->second;
    }
    // Compatibility fallback for older session dumps where auth_token is absent.
    const auto tokenIt = session.tokens.find("token");
    if (tokenIt != session.tokens.end() && !tokenIt->second.empty()) {
        return tokenIt->second;
    }
    return {};
}

SessionContextResult SessionProvider::loadRequestContext(
    const std::optional<std::filesystem::path>& pathOverride) const {
    cloud::LoadSessionResult loaded = loadCurrentSession(pathOverride);
    if (!loaded.ok) {
        return {false, loaded.message, {}};
    }

    const std::optional<std::string> accessToken = requireAccessToken(loaded.session);
    if (!accessToken.has_value()) {
        return {false, "session_missing_access_token", {}};
    }

    CloudRequestContext context;
    context.accessToken = *accessToken;
    context.xxToken = optionalXxToken(loaded.session);
    context.mqttAuthToken = envOrFallback("ACCLOUD_MQTT_AUTH_TOKEN", optionalMqttAuthToken(loaded.session));
    context.email = envOrFallback("ACCLOUD_MQTT_EMAIL",
                                  firstNonEmptyToken(loaded.session, {"email", "user_email"}));
    context.userId = envOrFallback("ACCLOUD_MQTT_USER_ID",
                                   firstNonEmptyToken(loaded.session, {"user_id", "uid"}));
    context.modeAuth = envOrFallback("ACCLOUD_MQTT_MODE_AUTH",
                                     firstNonEmptyToken(loaded.session, {"mode_auth", "auth_mode"}));

    // Local fallback from JWT claims when session metadata is incomplete.
    if (context.email.empty() || context.userId.empty() || context.modeAuth.empty()) {
        auto applyPayloadFields = [&](const nlohmann::json& payload) {
            if (context.email.empty()) {
                if (auto value = firstNonEmptyValue(payload, {"email", "user_email"}); value.has_value()) {
                    context.email = *value;
                }
            }
            if (context.userId.empty()) {
                if (auto value = firstNonEmptyValue(payload, {"user_id", "uid", "userid"}); value.has_value()) {
                    context.userId = *value;
                }
            }
            if (context.modeAuth.empty()) {
                if (auto value = firstNonEmptyValue(payload, {"mode_auth", "auth_mode"}); value.has_value()) {
                    context.modeAuth = *value;
                }
            }
        };

        if (!context.xxToken.empty()) {
            applyPayloadFields(decodeJwtPayload(context.xxToken));
        }
        if ((context.email.empty() || context.userId.empty() || context.modeAuth.empty())
            && !context.accessToken.empty()) {
            applyPayloadFields(decodeJwtPayload(context.accessToken));
        }
    }

#ifdef ACCLOUD_WITH_QT
    // Network fallback only for still-missing metadata.
    if (context.email.empty() || context.userId.empty() || context.modeAuth.empty()) {
        const auto remote = fetchRemoteSessionMetadata(context.accessToken, context.xxToken);
        if (remote.has_value()) {
            if (context.email.empty() && !remote->email.empty()) {
                context.email = remote->email;
                loaded.session.tokens["email"] = remote->email;
            }
            if (context.userId.empty() && !remote->userId.empty()) {
                context.userId = remote->userId;
                loaded.session.tokens["user_id"] = remote->userId;
            }
            if (context.modeAuth.empty() && !remote->modeAuth.empty()) {
                context.modeAuth = remote->modeAuth;
                loaded.session.tokens["mode_auth"] = remote->modeAuth;
            }

            // Best effort persistence for next startup.
            if (pathOverride.has_value()) {
                (void)cloud::saveSessionFile(loaded.session, *pathOverride);
            } else {
                (void)cloud::saveSessionFile(loaded.session);
            }
        }
    }
#endif

    context.sessionPath = loaded.path;
    return {true, "ok", std::move(context)};
}

} // namespace accloud::cloud::core
