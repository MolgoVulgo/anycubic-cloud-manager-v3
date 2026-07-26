#include "infra/logging/Redactor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace accloud::logging {
namespace {

constexpr std::array<std::string_view, 11> kSensitiveHints = {
    "token",      "authorization", "cookie", "signature", "password", "secret",
    "credential", "session",       "api_key", "apikey",   "bearer",
};

char toLowerAscii(char c) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

std::string lowercase(std::string_view value) {
  std::string out(value);
  std::transform(out.begin(), out.end(), out.begin(), toLowerAscii);
  return out;
}

bool containsInsensitive(std::string_view value, std::string_view needle) {
  if (needle.empty() || value.size() < needle.size()) {
    return false;
  }
  const std::string loweredValue = lowercase(value);
  const std::string loweredNeedle = lowercase(needle);
  return loweredValue.find(loweredNeedle) != std::string::npos;
}

std::string redactSpan(std::string_view value) {
  if (value.empty()) {
    return "<redacted>";
  }
  if (value.size() <= 8) {
    return "<redacted>";
  }
  std::string out;
  out.reserve(value.size());
  out.append(value.substr(0, 3));
  out.append("...");
  out.append(value.substr(value.size() - 2));
  out.append(" (redacted)");
  return out;
}

std::size_t findInsensitive(std::string_view haystack, std::string_view needle, std::size_t start) {
  if (needle.empty() || start >= haystack.size()) {
    return std::string::npos;
  }
  const std::string loweredHaystack = lowercase(haystack);
  const std::string loweredNeedle = lowercase(needle);
  return loweredHaystack.find(loweredNeedle, start);
}

void redactBearerTokens(std::string& message) {
  constexpr std::string_view kPrefix = "Bearer ";
  std::size_t cursor = 0;
  while (true) {
    const std::size_t markerPos = findInsensitive(message, kPrefix, cursor);
    if (markerPos == std::string::npos) {
      return;
    }
    const std::size_t valueStart = markerPos + kPrefix.size();
    std::size_t valueEnd = valueStart;
    while (valueEnd < message.size()) {
      const char c = message[valueEnd];
      if (std::isspace(static_cast<unsigned char>(c)) != 0 || c == ',' || c == ';' || c == '"' ||
          c == '\'') {
        break;
      }
      ++valueEnd;
    }
    message.replace(valueStart, valueEnd - valueStart, "<redacted>");
    cursor = valueStart + 10;
  }
}

void redactQueryParamValue(std::string& message, std::string_view key) {
  const std::string pattern = std::string(key) + "=";
  std::size_t cursor = 0;
  while (true) {
    const std::size_t markerPos = findInsensitive(message, pattern, cursor);
    if (markerPos == std::string::npos) {
      return;
    }
    const std::size_t valueStart = markerPos + pattern.size();
    std::size_t valueEnd = valueStart;
    while (valueEnd < message.size()) {
      const char c = message[valueEnd];
      if (c == '&' || std::isspace(static_cast<unsigned char>(c)) != 0 || c == '"' || c == ',') {
        break;
      }
      ++valueEnd;
    }
    message.replace(valueStart, valueEnd - valueStart, "<redacted>");
    cursor = valueStart + 10;
  }
}

} // namespace

bool isSensitiveKey(std::string_view key) {
  if (key.empty()) {
    return false;
  }
  for (std::string_view hint : kSensitiveHints) {
    if (containsInsensitive(key, hint)) {
      return true;
    }
  }
  return false;
}

std::string redactValueForKey(std::string_view key, std::string_view value) {
  if (!isSensitiveKey(key)) {
    return std::string(value);
  }
  return redactSpan(value);
}

std::string safeUrlForLogs(std::string_view value) {
  std::string out(value);

  const auto first = out.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = out.find_last_not_of(" \t\r\n");
  out = out.substr(first, last - first + 1);

  const std::size_t sensitiveSuffix = out.find_first_of("?#");
  if (sensitiveSuffix != std::string::npos) {
    out.resize(sensitiveSuffix);
  }

  std::size_t authorityStart = std::string::npos;
  const std::size_t scheme = out.find("://");
  if (scheme != std::string::npos) {
    authorityStart = scheme + 3;
  } else if (out.rfind("//", 0) == 0) {
    authorityStart = 2;
  }

  if (authorityStart != std::string::npos) {
    std::size_t authorityEnd = out.find('/', authorityStart);
    if (authorityEnd == std::string::npos) {
      authorityEnd = out.size();
    }
    const std::size_t userInfoEnd = out.rfind('@', authorityEnd);
    if (userInfoEnd != std::string::npos && userInfoEnd >= authorityStart) {
      out.erase(authorityStart, userInfoEnd - authorityStart + 1);
    }
  }

  return out;
}

std::string redactMessage(std::string_view message) {
  std::string out(message);
  redactBearerTokens(out);
  redactQueryParamValue(out, "access_token");
  redactQueryParamValue(out, "id_token");
  redactQueryParamValue(out, "refresh_token");
  redactQueryParamValue(out, "signature");
  redactQueryParamValue(out, "token");
  return out;
}

} // namespace accloud::logging
