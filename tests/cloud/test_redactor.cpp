#include "infra/logging/Redactor.h"

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

bool test_sensitive_key_detection_and_redaction() {
    const bool keyToken = accloud::logging::isSensitiveKey("access_token");
    const bool keyPassword = accloud::logging::isSensitiveKey("PASSWORD");
    const bool keySafe = accloud::logging::isSensitiveKey("printer_id");

    const std::string redactedToken =
        accloud::logging::redactValueForKey("access_token", "abcdefghijklmnopqrstuvwxyz");
    const std::string safeValue =
        accloud::logging::redactValueForKey("printer_id", "p-1");

    return expect(keyToken, "access_token should be sensitive")
        && expect(keyPassword, "password should be sensitive")
        && expect(!keySafe, "printer_id should not be sensitive")
        && expect(redactedToken.find("redacted") != std::string::npos,
                  "sensitive value should be redacted")
        && expect(safeValue == "p-1", "non-sensitive value should not be redacted");
}

bool test_message_redaction() {
    const std::string input =
        "Authorization: Bearer abcdefghijklmnop token=secret123 access_token=xyz987 signature=sig999";
    const std::string redacted = accloud::logging::redactMessage(input);
    return expect(redacted.find("Bearer <redacted>") != std::string::npos,
                  "bearer token must be redacted")
        && expect(redacted.find("token=<redacted>") != std::string::npos,
                  "token query param must be redacted")
        && expect(redacted.find("access_token=<redacted>") != std::string::npos,
                  "access_token query param must be redacted")
        && expect(redacted.find("signature=<redacted>") != std::string::npos,
                  "signature query param must be redacted");
}

bool test_safe_url_for_logs() {
    const std::string signedUrl =
        "https://user:password@cdn.example.test/thumb/image.png?X-Amz-Signature=secret&token=abc#preview";
    const std::string safe = accloud::logging::safeUrlForLogs(signedUrl);
    const std::string plain =
        accloud::logging::safeUrlForLogs("https://cdn.example.test/thumb/image.png");

    return expect(safe == "https://cdn.example.test/thumb/image.png",
                  "safe URL should keep scheme, host and path only")
        && expect(safe.find("password") == std::string::npos,
                  "URL user info must be removed")
        && expect(safe.find("Signature") == std::string::npos,
                  "URL query must be removed")
        && expect(safe.find("preview") == std::string::npos,
                  "URL fragment must be removed")
        && expect(plain == "https://cdn.example.test/thumb/image.png",
                  "plain URL should remain unchanged");
}

} // namespace

int main() {
    bool ok = true;
    ok = test_sensitive_key_detection_and_redaction() && ok;
    ok = test_message_redaction() && ok;
    ok = test_safe_url_for_logs() && ok;
    if (!ok) {
        return 1;
    }
    std::cout << "Redactor tests passed\n";
    return 0;
}

