#pragma once

#include <string>
#include <string_view>

namespace accloud::logging {

[[nodiscard]] bool isSensitiveKey(std::string_view key);
[[nodiscard]] std::string redactValueForKey(std::string_view key, std::string_view value);
[[nodiscard]] std::string redactMessage(std::string_view message);

} // namespace accloud::logging
