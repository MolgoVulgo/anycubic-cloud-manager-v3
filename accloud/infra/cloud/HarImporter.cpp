#include "infra/cloud/HarImporter.h"
#include "infra/logging/JsonlLogger.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <initializer_list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace accloud::cloud {
namespace {

struct Candidate {
  std::map<std::string, std::string> tokens;
  int score = 0;
};

[[nodiscard]] std::string lower(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

[[nodiscard]] std::string upper(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  }
  return out;
}

[[nodiscard]] std::string trim(std::string_view value) {
  std::size_t left = 0;
  while (left < value.size() &&
         std::isspace(static_cast<unsigned char>(value[left])) != 0) {
    ++left;
  }
  std::size_t right = value.size();
  while (right > left &&
         std::isspace(static_cast<unsigned char>(value[right - 1])) != 0) {
    --right;
  }
  return std::string(value.substr(left, right - left));
}

[[nodiscard]] bool containsCaseInsensitive(std::string_view haystack,
                                           std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  std::string hay = lower(haystack);
  std::string need = lower(needle);
  return hay.find(need) != std::string::npos;
}

[[nodiscard]] bool startsWithCaseInsensitive(std::string_view value,
                                             std::string_view prefix) {
  if (value.size() < prefix.size()) {
    return false;
  }
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(value[i])) !=
        std::tolower(static_cast<unsigned char>(prefix[i]))) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::string normalizeLookupKey(std::string_view key) {
  std::string out;
  out.reserve(key.size());
  for (char c : key) {
    if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
  }
  return out;
}

[[nodiscard]] std::optional<std::string> stripBearerPrefix(std::string_view value) {
  std::string trimmed = trim(value);
  if (trimmed.empty()) {
    return std::nullopt;
  }
  if (startsWithCaseInsensitive(trimmed, "bearer ")) {
    std::string token = trim(std::string_view(trimmed).substr(7));
    if (token.empty()) {
      return std::nullopt;
    }
    return token;
  }
  return trimmed;
}

void mergeMissing(std::map<std::string, std::string>& destination,
                  const std::map<std::string, std::string>& source) {
  for (const auto& [key, value] : source) {
    if (value.empty()) {
      continue;
    }
    if (destination.find(key) == destination.end()) {
      destination[key] = value;
    }
  }
}

[[nodiscard]] std::optional<std::string> firstNonEmpty(
    const std::map<std::string, std::string>& map,
    std::initializer_list<std::string_view> keys) {
  for (const std::string_view key : keys) {
    auto it = map.find(std::string(key));
    if (it != map.end()) {
      const std::string value = trim(it->second);
      if (!value.empty()) {
        return value;
      }
    }
  }
  return std::nullopt;
}

SessionData normalizeSessionTokens(const std::map<std::string, std::string>& raw) {
  SessionData session;

  std::string accessToken;
  if (auto value = firstNonEmpty(raw, {"access_token", "accesstoken"})) {
    accessToken = *value;
  }

  std::string authorization;
  if (auto value = firstNonEmpty(raw, {"Authorization", "authorization"})) {
    authorization = *value;
  }

  std::string token;
  if (auto value = firstNonEmpty(raw, {"token", "X-Access-Token", "X-Auth-Token"})) {
    token = *value;
  }

  std::string idToken;
  if (auto value = firstNonEmpty(raw, {"id_token", "idtoken"})) {
    idToken = *value;
  }

  if (accessToken.empty() && !token.empty()) {
    accessToken = token;
  }
  if (!accessToken.empty()) {
    accessToken = stripBearerPrefix(accessToken).value_or("");
  }

  if (accessToken.empty() && !authorization.empty()) {
    accessToken = stripBearerPrefix(authorization).value_or("");
  }

  if (idToken.empty() && !accessToken.empty()) {
    idToken = accessToken;
  }
  if (token.empty() && !accessToken.empty()) {
    token = accessToken;
  }
  if (!token.empty()) {
    token = stripBearerPrefix(token).value_or("");
  }

  if (!accessToken.empty()) {
    session.tokens["access_token"] = accessToken;
  }
  if (!idToken.empty()) {
    session.tokens["id_token"] = idToken;
  }
  if (!token.empty()) {
    session.tokens["token"] = token;
  }

  return session;
}

// ---------------------------------------------------------------------------
// nlohmann::json helpers
// ---------------------------------------------------------------------------

[[nodiscard]] std::string scalarToString(const nlohmann::json& v) {
  if (v.is_string())         return v.get<std::string>();
  if (v.is_boolean())        return v.get<bool>() ? "true" : "false";
  if (v.is_number_integer()) return std::to_string(v.get<long long>());
  if (v.is_number_float()) {
    const double d = v.get<double>();
    const double rounded = std::round(d);
    if (std::fabs(d - rounded) < 1e-9) {
      return std::to_string(static_cast<long long>(rounded));
    }
    std::ostringstream ss;
    ss << d;
    return ss.str();
  }
  return {};
}

void flattenScalarFields(const nlohmann::json& v, std::map<std::string, std::string>& flat) {
  if (v.is_object()) {
    for (const auto& [key, child] : v.items()) {
      if (child.is_string() || child.is_boolean() || child.is_number()) {
        const std::string norm = normalizeLookupKey(key);
        if (!norm.empty() && !flat.count(norm)) {
          flat[norm] = scalarToString(child);
        }
      }
      flattenScalarFields(child, flat);
    }
  } else if (v.is_array()) {
    for (const auto& child : v) {
      flattenScalarFields(child, flat);
    }
  }
}

std::map<std::string, std::string> extractResponseTokens(const nlohmann::json& responseBody) {
  std::map<std::string, std::string> flat;
  flattenScalarFields(responseBody, flat);

  std::map<std::string, std::string> raw;
  if (auto value = firstNonEmpty(flat, {"accesstoken"}))                    raw["access_token"]  = *value;
  if (auto value = firstNonEmpty(flat, {"token"}))                          raw["token"]         = *value;
  if (auto value = firstNonEmpty(flat, {"idtoken"}))                        raw["id_token"]      = *value;
  if (auto value = firstNonEmpty(flat, {"refreshtoken"}))                   raw["refresh_token"] = *value;
  if (auto value = firstNonEmpty(flat, {"tokentype"}))                      raw["token_type"]    = *value;
  if (auto value = firstNonEmpty(flat, {"authorization", "auth", "bearer"})) raw["Authorization"] = *value;

  if (auto expires = firstNonEmpty(flat, {"expiresin"})) {
    try {
      const long parsed = std::stol(*expires);
      if (parsed > 0) {
        raw["expires_in"] = std::to_string(parsed);
      }
    } catch (...) {
      // Ignore invalid expires_in values.
    }
  }

  if (raw.find("access_token") == raw.end()) {
    auto tokenIt = raw.find("token");
    if (tokenIt != raw.end()) {
      raw["access_token"] = tokenIt->second;
    }
  }

  if (auto accessIt = raw.find("access_token"); accessIt != raw.end()) {
    accessIt->second = stripBearerPrefix(accessIt->second).value_or("");
  }

  if (auto authIt = raw.find("Authorization"); authIt != raw.end()) {
    if (auto stripped = stripBearerPrefix(authIt->second)) {
      if (raw.find("access_token") == raw.end()) {
        raw["access_token"] = *stripped;
      }
      authIt->second = "Bearer " + *stripped;
    }
  }

  if (raw.find("Authorization") == raw.end()) {
    auto accessIt = raw.find("access_token");
    if (accessIt != raw.end() && !accessIt->second.empty()) {
      const auto tokenType = firstNonEmpty(raw, {"token_type"}).value_or("Bearer");
      raw["Authorization"] = tokenType + " " + accessIt->second;
    }
  }

  return raw;
}

std::map<std::string, std::string> extractHeaderTokens(const nlohmann::json& headers) {
  std::map<std::string, std::string> raw;
  if (!headers.is_array()) {
    return raw;
  }

  for (const auto& entry : headers) {
    if (!entry.is_object()) continue;
    if (!entry.contains("name") || !entry["name"].is_string()) continue;
    if (!entry.contains("value") || !entry["value"].is_string()) continue;

    const std::string name        = entry["name"].get<std::string>();
    const std::string headerValue = trim(entry["value"].get<std::string>());
    if (headerValue.empty()) continue;

    const std::string loweredName = lower(name);

    if (loweredName == "authorization") {
      raw["Authorization"] = headerValue;
      if (auto stripped = stripBearerPrefix(headerValue)) {
        raw["access_token"] = *stripped;
      }
      continue;
    }

    if (loweredName == "x-access-token") {
      raw["X-Access-Token"] = headerValue;
      if (raw.find("token") == raw.end()) {
        raw["token"] = headerValue;
      }
      continue;
    }

    if (loweredName == "x-auth-token") {
      raw["X-Auth-Token"] = headerValue;
      if (raw.find("token") == raw.end()) {
        raw["token"] = headerValue;
      }
      continue;
    }

    if (loweredName.rfind("x-", 0) == 0 && loweredName.find("token") != std::string::npos) {
      if (raw.find("token") == raw.end()) {
        raw["token"] = headerValue;
      }
      continue;
    }

    const std::string normalized = normalizeLookupKey(loweredName);
    if (normalized == "accesstoken")  raw["access_token"] = headerValue;
    else if (normalized == "idtoken") raw["id_token"]     = headerValue;
    else if (normalized == "token")   raw["token"]        = headerValue;
  }

  return raw;
}

// ---------------------------------------------------------------------------
// URL helpers (no JSON dependency)
// ---------------------------------------------------------------------------

[[nodiscard]] char hexDigit(int value) {
  if (value >= 0 && value <= 9) {
    return static_cast<char>('0' + value);
  }
  return static_cast<char>('A' + (value - 10));
}

[[nodiscard]] int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

[[nodiscard]] std::string urlDecode(std::string_view value) {
  std::string out;
  out.reserve(value.size());

  for (std::size_t i = 0; i < value.size(); ++i) {
    const char c = value[i];
    if (c == '+') {
      out.push_back(' ');
      continue;
    }
    if (c == '%' && i + 2 < value.size()) {
      int h1 = hexValue(value[i + 1]);
      int h2 = hexValue(value[i + 2]);
      if (h1 >= 0 && h2 >= 0) {
        out.push_back(static_cast<char>((h1 << 4) | h2));
        i += 2;
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

std::map<std::string, std::string> extractQueryTokens(std::string_view url) {
  std::map<std::string, std::string> raw;

  std::size_t qpos = url.find('?');
  if (qpos == std::string_view::npos || qpos + 1 >= url.size()) {
    return raw;
  }
  std::size_t end = url.find('#', qpos + 1);
  if (end == std::string_view::npos) {
    end = url.size();
  }
  const std::string_view query = url.substr(qpos + 1, end - qpos - 1);

  std::size_t cursor = 0;
  while (cursor < query.size()) {
    std::size_t amp = query.find('&', cursor);
    if (amp == std::string_view::npos) {
      amp = query.size();
    }
    std::string_view part = query.substr(cursor, amp - cursor);
    cursor = amp + 1;
    if (part.empty()) {
      continue;
    }

    std::size_t eq = part.find('=');
    const std::string key =
        urlDecode(part.substr(0, eq == std::string_view::npos ? part.size() : eq));
    const std::string value =
        (eq == std::string_view::npos) ? "" : urlDecode(part.substr(eq + 1));

    const std::string normalized = normalizeLookupKey(key);
    if (normalized == "accesstoken")       raw["access_token"]  = value;
    else if (normalized == "idtoken")      raw["id_token"]      = value;
    else if (normalized == "refreshtoken") raw["refresh_token"] = value;
    else if (normalized == "tokentype")    raw["token_type"]    = value;
    else if (normalized == "token")        raw["token"]         = value;
    else if (normalized == "authorization") raw["Authorization"] = value;
  }

  return raw;
}

// ---------------------------------------------------------------------------
// Scoring helpers
// ---------------------------------------------------------------------------

[[nodiscard]] bool isAuthEndpoint(std::string_view url) {
  const std::array<std::string_view, 4> markers = {
      "/login", "/auth", "/token", "/refresh",
  };
  const std::string lowered = lower(url);
  for (std::string_view marker : markers) {
    if (lowered.find(std::string(marker)) != std::string::npos) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] int scoreEntry(std::string_view method, int status, bool authEndpoint) {
  int score = 0;
  if (status == 200 || status == 201) score += 100;
  if (authEndpoint)                    score += 10;
  if (upper(method) == "POST")         score += 1;
  return score;
}

[[nodiscard]] std::optional<Candidate> bestCandidate(const std::vector<Candidate>& candidates) {
  if (candidates.empty()) {
    return std::nullopt;
  }
  return *std::max_element(
      candidates.begin(), candidates.end(),
      [](const Candidate& left, const Candidate& right) { return left.score < right.score; });
}

// ---------------------------------------------------------------------------
// Base64 decoder (no JSON dependency)
// ---------------------------------------------------------------------------

[[nodiscard]] std::optional<std::string> decodeBase64(std::string_view input) {
  constexpr std::string_view kAlphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::array<int, 256> table{};
  table.fill(-1);
  for (std::size_t i = 0; i < kAlphabet.size(); ++i) {
    table[static_cast<unsigned char>(kAlphabet[i])] = static_cast<int>(i);
  }

  std::string output;
  int val = 0;
  int valBits = -8;

  for (char c : input) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isspace(u) != 0) continue;
    if (c == '=') break;
    const int decoded = table[u];
    if (decoded < 0) return std::nullopt;
    val = (val << 6) + decoded;
    valBits += 6;
    if (valBits >= 0) {
      output.push_back(static_cast<char>((val >> valBits) & 0xFF));
      valBits -= 8;
    }
  }

  return output;
}

// ---------------------------------------------------------------------------
// HAR entry helpers
// ---------------------------------------------------------------------------

[[nodiscard]] std::optional<std::string> responseContentText(const nlohmann::json& entry) {
  if (!entry.contains("response") || !entry["response"].is_object()) {
    return std::nullopt;
  }
  const auto& response = entry["response"];
  if (!response.contains("content") || !response["content"].is_object()) {
    return std::nullopt;
  }
  const auto& content = response["content"];
  if (!content.contains("text") || !content["text"].is_string()) {
    return std::nullopt;
  }
  std::string body = content["text"].get<std::string>();
  if (body.empty()) {
    return std::nullopt;
  }

  if (content.contains("encoding") && content["encoding"].is_string() &&
      lower(content["encoding"].get<std::string>()) == "base64") {
    auto decoded = decodeBase64(body);
    if (!decoded.has_value()) {
      return std::nullopt;
    }
    return decoded;
  }
  return body;
}

[[nodiscard]] bool readFileToString(const std::filesystem::path& path, std::string& out,
                                    std::string& error) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open()) {
    error = "Unable to open file: " + path.string();
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  out = buffer.str();
  return true;
}

// ---------------------------------------------------------------------------
// Session JSON serialisation
// ---------------------------------------------------------------------------

[[nodiscard]] std::string nowAsDayMonthYear() {
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_r(&now, &tm);

  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1,
                tm.tm_year + 1900);
  return buffer;
}

[[nodiscard]] std::string buildSessionJson(const SessionData& session) {
  constexpr std::array<std::string_view, 3> kCanonicalOrder = {
      "access_token", "id_token", "token",
  };

  nlohmann::ordered_json j;
  j["last_update"] = nowAsDayMonthYear();
  nlohmann::ordered_json tokens = nlohmann::ordered_json::object();
  for (std::string_view key : kCanonicalOrder) {
    auto it = session.tokens.find(std::string(key));
    if (it != session.tokens.end()) {
      tokens[std::string(key)] = it->second;
    }
  }
  j["tokens"] = tokens;
  return j.dump(2) + "\n";
}

// ---------------------------------------------------------------------------
// Session JSON parsing
// ---------------------------------------------------------------------------

[[nodiscard]] bool parseSessionText(const std::string& content, SessionData& session,
                                    std::string& error) {
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(content);
  } catch (const nlohmann::json::parse_error& e) {
    error = e.what();
    return false;
  }

  if (!root.is_object()) {
    error = "session.json root must be an object";
    return false;
  }

  std::map<std::string, std::string> raw;

  if (root.contains("tokens") && root["tokens"].is_object()) {
    for (const auto& [key, value] : root["tokens"].items()) {
      if (value.is_string() || value.is_boolean() || value.is_number()) {
        raw[key] = scalarToString(value);
      }
    }
  }

  if (root.contains("headers") && root["headers"].is_object()) {
    for (const auto& [key, value] : root["headers"].items()) {
      if (value.is_string() || value.is_boolean() || value.is_number()) {
        raw[key] = scalarToString(value);
        if (key == "X-Access-Token" || key == "X-Auth-Token") {
          raw["token"] = scalarToString(value);
        }
      }
    }
  }

  for (const auto& key : {"Authorization", "access_token", "id_token", "token"}) {
    if (root.contains(key)) {
      const auto& val = root[key];
      if (val.is_string() || val.is_boolean() || val.is_number()) {
        raw[key] = scalarToString(val);
      }
    }
  }

  session = normalizeSessionTokens(raw);
  return !session.empty();
}

// ---------------------------------------------------------------------------
// Unix strict session file write (0600)
// ---------------------------------------------------------------------------

[[nodiscard]] bool writeStrictSessionFile(const std::filesystem::path& path,
                                          const std::string& payload,
                                          std::string& error) {
  const std::string pathString = path.string();
  const int fd = ::open(pathString.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    error = "Unable to open session file for write: " + pathString;
    return false;
  }

  std::size_t cursor = 0;
  while (cursor < payload.size()) {
    const ssize_t wrote = ::write(fd, payload.data() + cursor, payload.size() - cursor);
    if (wrote <= 0) {
      ::close(fd);
      error = "Unable to write session file: " + pathString;
      return false;
    }
    cursor += static_cast<std::size_t>(wrote);
  }

  if (::fchmod(fd, 0600) != 0) {
    ::close(fd);
    error = "Unable to apply 0600 permissions on: " + pathString;
    return false;
  }

  if (::close(fd) != 0) {
    error = "Unable to close session file: " + pathString;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// HAR parsing
// ---------------------------------------------------------------------------

[[nodiscard]] bool parseHarToSession(const std::string& harText, const std::string& hostContains,
                                     SessionData& outSession, std::size_t& visited,
                                     std::size_t& accepted, std::string& error) {
  visited = 0;
  accepted = 0;

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(harText);
  } catch (const nlohmann::json::parse_error& e) {
    error = e.what();
    return false;
  }

  if (!root.contains("log") || !root["log"].is_object()) {
    error = "HAR root does not contain log object";
    return false;
  }
  const auto& log = root["log"];
  if (!log.contains("entries") || !log["entries"].is_array()) {
    error = "HAR log.entries is missing or invalid";
    return false;
  }

  const std::string hostNeedle = lower(hostContains);
  std::vector<Candidate> responseCandidates;
  std::vector<Candidate> headerCandidates;
  std::vector<Candidate> queryCandidates;

  for (const auto& entry : log["entries"]) {
    ++visited;
    if (!entry.is_object()) continue;
    if (!entry.contains("request") || !entry["request"].is_object()) continue;

    const auto& request = entry["request"];
    if (!request.contains("url") || !request["url"].is_string()) continue;
    const std::string url = request["url"].get<std::string>();
    if (url.empty()) continue;
    if (!hostNeedle.empty() && !containsCaseInsensitive(url, hostNeedle)) continue;
    ++accepted;

    const std::string method = request.value("method", std::string("GET"));
    int status = 0;
    if (entry.contains("response") && entry["response"].is_object()) {
      const auto& resp = entry["response"];
      if (resp.contains("status") && resp["status"].is_number()) {
        status = resp["status"].get<int>();
      }
    }
    const bool authLikely = isAuthEndpoint(url);
    const int score = scoreEntry(method, status, authLikely);

    std::map<std::string, std::string> queryTokens = extractQueryTokens(url);
    if (!queryTokens.empty()) {
      queryCandidates.push_back(Candidate{std::move(queryTokens), score});
    }

    if (request.contains("headers")) {
      std::map<std::string, std::string> headerTokens = extractHeaderTokens(request["headers"]);
      if (!headerTokens.empty()) {
        headerCandidates.push_back(Candidate{std::move(headerTokens), score});
      }
    }

    if (auto content = responseContentText(entry); content.has_value()) {
      try {
        nlohmann::json body = nlohmann::json::parse(*content);
        if (body.is_object()) {
          std::map<std::string, std::string> responseTokens = extractResponseTokens(body);
          if (!responseTokens.empty()) {
            responseCandidates.push_back(Candidate{std::move(responseTokens), score});
          }
        }
      } catch (const nlohmann::json::parse_error&) {
        // Response body is not JSON — skip silently.
      }
    }
  }

  std::map<std::string, std::string> merged;
  if (auto best = bestCandidate(responseCandidates); best.has_value()) merged = best->tokens;
  if (auto best = bestCandidate(headerCandidates);   best.has_value()) mergeMissing(merged, best->tokens);
  if (auto best = bestCandidate(queryCandidates);    best.has_value()) mergeMissing(merged, best->tokens);

  outSession = normalizeSessionTokens(merged);
  if (outSession.empty()) {
    error = "No usable token fields found in HAR entries";
    return false;
  }
  return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::filesystem::path resolveSessionPath(
    const std::optional<std::filesystem::path>& pathOverride) {
  if (pathOverride.has_value()) {
    return *pathOverride;
  }

  if (const char* env = std::getenv("ACCLOUD_SESSION_PATH");
      env != nullptr && *env != '\0') {
    return std::filesystem::path(env);
  }
  return std::filesystem::path("session.json");
}

LoadSessionResult loadSessionFile(
    const std::optional<std::filesystem::path>& pathOverride) {
  LoadSessionResult result;
  result.path = resolveSessionPath(pathOverride);
  logging::debug("cloud", "har_importer", "session_load_start", "Loading session file",
                 {{"path", result.path.string()}});

  if (!std::filesystem::exists(result.path)) {
    result.message = "Session file does not exist: " + result.path.string();
    logging::warn("cloud", "har_importer", "session_load_missing", result.message,
                  {{"path", result.path.string()}});
    return result;
  }

  std::string content;
  std::string error;
  if (!readFileToString(result.path, content, error)) {
    result.message = error;
    logging::error("cloud", "har_importer", "session_load_failed", "Unable to read session file",
                   {{"path", result.path.string()}, {"reason", error}});
    return result;
  }

  SessionData session;
  if (!parseSessionText(content, session, error)) {
    result.message = error;
    logging::error("cloud", "har_importer", "session_parse_failed",
                   "Unable to parse session JSON", {{"path", result.path.string()}, {"reason", error}});
    return result;
  }

  result.ok = true;
  result.session = std::move(session);
  result.message = "Session loaded";
  logging::info("cloud", "har_importer", "session_load_success", result.message,
                {{"path", result.path.string()},
                 {"token_count", std::to_string(result.session.tokens.size())}});
  return result;
}

SaveSessionResult saveSessionFile(
    const SessionData& session,
    const std::optional<std::filesystem::path>& pathOverride) {
  SaveSessionResult result;
  result.path = resolveSessionPath(pathOverride);
  logging::debug("cloud", "har_importer", "session_save_start", "Saving session file",
                 {{"path", result.path.string()}});

  SessionData normalized = normalizeSessionTokens(session.tokens);
  if (normalized.empty()) {
    result.message = "Refusing to save an empty session";
    logging::warn("cloud", "har_importer", "session_save_rejected", result.message,
                  {{"path", result.path.string()}});
    return result;
  }

  if (result.path.has_parent_path() && !result.path.parent_path().empty()) {
    std::error_code ec;
    std::filesystem::create_directories(result.path.parent_path(), ec);
    if (ec) {
      result.message = "Unable to create session directory: " + result.path.parent_path().string();
      logging::error("cloud", "har_importer", "session_dir_create_failed", result.message,
                     {{"path", result.path.parent_path().string()}});
      return result;
    }
  }

  std::string payload = buildSessionJson(normalized);
  std::string error;
  if (!writeStrictSessionFile(result.path, payload, error)) {
    result.message = error;
    logging::error("cloud", "har_importer", "session_save_failed", "Unable to persist session file",
                   {{"path", result.path.string()}, {"reason", error}});
    return result;
  }

  result.ok = true;
  result.message = "Session saved";
  logging::info("cloud", "har_importer", "session_save_success", result.message,
                {{"path", result.path.string()},
                 {"token_count", std::to_string(normalized.tokens.size())}});
  return result;
}

HarImportResult importHarText(const std::string& harText,
                              const HarImportOptions& options) {
  HarImportResult result;
  SessionData imported;

  logging::info("cloud", "har_importer", "har_import_text_start", "Parsing HAR text",
                {{"host_filter", options.hostContains}});

  std::string parseError;
  if (!parseHarToSession(harText, options.hostContains, imported, result.entriesVisited,
                         result.entriesAccepted, parseError)) {
    result.message = parseError;
    logging::error("cloud", "har_importer", "har_parse_failed", "HAR parsing failed",
                   {{"reason", parseError}});
    return result;
  }

  SessionData merged = imported;
  std::string warning;
  const std::filesystem::path sessionPath = resolveSessionPath(options.sessionPathOverride);
  if (options.mergeWithExistingSession && std::filesystem::exists(sessionPath)) {
    LoadSessionResult loaded = loadSessionFile(sessionPath);
    if (loaded.ok) {
      std::map<std::string, std::string> raw = loaded.session.tokens;
      for (const auto& [key, value] : imported.tokens) {
        raw[key] = value;
      }
      merged = normalizeSessionTokens(raw);
    } else {
      warning = loaded.message;
      logging::warn("cloud", "har_importer", "session_merge_warning",
                    "Continuing without merging existing session", {{"warning", warning}});
    }
  }

  if (options.persistSession) {
    SaveSessionResult saved = saveSessionFile(merged, sessionPath);
    if (!saved.ok) {
      result.message = saved.message;
      logging::error("cloud", "har_importer", "har_import_persist_failed",
                     "HAR import failed while persisting session",
                     {{"path", sessionPath.string()}, {"reason", saved.message}});
      return result;
    }
  }

  result.ok = true;
  result.session = std::move(merged);
  std::ostringstream message;
  message << "HAR import succeeded (" << result.entriesAccepted << "/" << result.entriesVisited
          << " entries matched host filter)";
  if (!warning.empty()) {
    message << " - warning: " << warning;
  }
  result.message = message.str();
  logging::info("cloud", "har_importer", "har_import_text_success", "HAR parsing succeeded",
                {{"entries_visited", std::to_string(result.entriesVisited)},
                 {"entries_accepted", std::to_string(result.entriesAccepted)},
                 {"token_count", std::to_string(result.session.tokens.size())},
                 {"persisted", options.persistSession ? "true" : "false"}});
  return result;
}

HarImportResult importHarFile(const std::filesystem::path& harPath,
                              const HarImportOptions& options) {
  HarImportResult result;
  logging::info("cloud", "har_importer", "har_import_file_start", "Importing HAR file",
                {{"path", harPath.string()}, {"host_filter", options.hostContains}});

  std::string harText;
  std::string readError;
  if (!readFileToString(harPath, harText, readError)) {
    result.message = readError;
    logging::error("cloud", "har_importer", "har_file_read_failed", "Unable to read HAR file",
                   {{"path", harPath.string()}, {"reason", readError}});
    return result;
  }

  result = importHarText(harText, options);
  if (!result.ok) {
    result.message = "HAR import failed for " + harPath.string() + ": " + result.message;
    logging::error("cloud", "har_importer", "har_import_file_failed", "HAR import failed",
                   {{"path", harPath.string()}, {"reason", result.message}});
    return result;
  }
  logging::info("cloud", "har_importer", "har_import_file_success", "HAR file imported",
                {{"path", harPath.string()},
                 {"entries_visited", std::to_string(result.entriesVisited)},
                 {"entries_accepted", std::to_string(result.entriesAccepted)}});
  return result;
}

} // namespace accloud::cloud
