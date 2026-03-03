#include "infra/cloud/HarImporter.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace {

using accloud::cloud::HarImportOptions;
using accloud::cloud::HarImportResult;
using accloud::cloud::LoadSessionResult;

bool expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    return false;
  }
  return true;
}

bool expectToken(const HarImportResult& result, const char* key, const char* expected) {
  const auto it = result.session.tokens.find(key);
  if (it == result.session.tokens.end()) {
    std::cerr << "FAILED: token key missing: " << key << '\n';
    return false;
  }
  if (it->second != expected) {
    std::cerr << "FAILED: token mismatch for " << key << " expected=" << expected
              << " actual=" << it->second << '\n';
    return false;
  }
  return true;
}

std::filesystem::path makeTempDir(const char* suffix) {
  const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  std::filesystem::path path = std::filesystem::temp_directory_path() /
                               ("accloud-har-test-" + std::string(suffix) + "-" +
                                std::to_string(stamp));
  std::filesystem::create_directories(path);
  return path;
}

bool test_response_body_priority_and_header_merge() {
  const std::string har = R"json(
{
  "log": {
    "entries": [
      {
        "request": {
          "url": "https://api.anycubic.com/v1/auth/login",
          "method": "POST",
          "headers": [
            {"name": "Authorization", "value": "Bearer header_access"},
            {"name": "X-Access-Token", "value": "header_token"}
          ]
        },
        "response": {
          "status": 200,
          "content": {
            "text": "{\"data\":{\"access_token\":\"response_access\",\"id_token\":\"response_id\"}}"
          }
        }
      }
    ]
  }
}
)json";

  HarImportOptions options;
  options.mergeWithExistingSession = false;
  options.persistSession = false;
  const HarImportResult result = accloud::cloud::importHarText(har, options);

  return expect(result.ok, result.message) &&
         expectToken(result, "access_token", "response_access") &&
         expectToken(result, "id_token", "response_id") &&
         expectToken(result, "token", "header_token") &&
         expect(result.session.tokens.find("Authorization") == result.session.tokens.end(),
                "Authorization should not be stored in session tokens");
}

bool test_base64_response_body() {
  const std::string har = R"json(
{
  "log": {
    "entries": [
      {
        "request": {
          "url": "https://api.anycubic.com/v1/token/refresh",
          "method": "POST",
          "headers": []
        },
        "response": {
          "status": 201,
          "content": {
            "encoding": "base64",
            "text": "eyJ0b2tlbiI6ImI2NF90b2tlbiJ9"
          }
        }
      }
    ]
  }
}
)json";

  HarImportOptions options;
  options.mergeWithExistingSession = false;
  options.persistSession = false;
  const HarImportResult result = accloud::cloud::importHarText(har, options);

  return expect(result.ok, result.message) &&
         expectToken(result, "access_token", "b64_token") &&
         expect(result.session.tokens.find("Authorization") == result.session.tokens.end(),
                "Authorization should not be stored in session tokens");
}

bool test_query_fallback_and_session_roundtrip() {
  const std::string har = R"json(
{
  "log": {
    "entries": [
      {
        "request": {
          "url": "https://workshop.anycubic.com/v1/cloud/list?access_token=query_access&id_token=query_id",
          "method": "GET",
          "headers": []
        },
        "response": {
          "status": 204,
          "content": {
            "text": ""
          }
        }
      }
    ]
  }
}
)json";

  const std::filesystem::path tempDir = makeTempDir("session");
  const std::filesystem::path sessionPath = tempDir / "session.json";

  HarImportOptions options;
  options.mergeWithExistingSession = false;
  options.persistSession = true;
  options.sessionPathOverride = sessionPath;

  const HarImportResult imported = accloud::cloud::importHarText(har, options);
  if (!expect(imported.ok, imported.message) ||
      !expect(std::filesystem::exists(sessionPath), "session.json should be written")) {
    std::filesystem::remove_all(tempDir);
    return false;
  }

  const LoadSessionResult loaded = accloud::cloud::loadSessionFile(sessionPath);
  bool ok = expect(loaded.ok, loaded.message);
  if (ok) {
    auto accessIt = loaded.session.tokens.find("access_token");
    ok = expect(accessIt != loaded.session.tokens.end(),
                "access_token should exist in persisted content") &&
         expect(accessIt->second == "query_access",
                "access_token should match persisted content") &&
         expect(loaded.session.tokens.find("Authorization") == loaded.session.tokens.end(),
                "Authorization should not be stored in persisted session");
  }

#if defined(__unix__) || defined(__APPLE__)
  struct stat st {};
  if (stat(sessionPath.c_str(), &st) == 0) {
    ok = ok && expect((st.st_mode & 0777) == 0600, "session.json permissions should be 0600");
  } else {
    ok = false;
    std::cerr << "FAILED: unable to stat session file\n";
  }
#endif

  std::filesystem::remove_all(tempDir);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok = test_response_body_priority_and_header_merge() && ok;
  ok = test_base64_response_body() && ok;
  ok = test_query_fallback_and_session_roundtrip() && ok;

  if (!ok) {
    return 1;
  }

  std::cout << "HAR tests passed\n";
  return 0;
}
