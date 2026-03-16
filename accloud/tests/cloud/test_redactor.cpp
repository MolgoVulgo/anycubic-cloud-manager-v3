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

} // namespace

int main() {
    bool ok = true;
    ok = test_sensitive_key_detection_and_redaction() && ok;
    ok = test_message_redaction() && ok;
    if (!ok) {
        return 1;
    }
    std::cout << "Redactor tests passed\n";
    return 0;
}

