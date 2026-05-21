#pragma once

namespace accloud::debug {

#if defined(ACCLOUD_DEBUG)
inline constexpr bool kEnabled = true;
#else
inline constexpr bool kEnabled = false;
#endif

} // namespace accloud::debug

