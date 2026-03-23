#pragma once
// =============================================================================
// BimCore/Core.h  —  shared types, logging, and cross-platform utilities
// =============================================================================
#include <string>
#include <limits>

namespace BimCore {

// -----------------------------------------------------------------------------
// Numeric sentinels (replaces magic 1e9f / -1e9f scatter)
// -----------------------------------------------------------------------------
inline constexpr float kFloatMax = std::numeric_limits<float>::max();
inline constexpr float kFloatMin = std::numeric_limits<float>::lowest();

// -----------------------------------------------------------------------------
// Safe config parsing helpers
// -----------------------------------------------------------------------------
inline bool SafeParseFloat(const std::string& s, float& out, float fallback = 0.0f) {
    try { out = std::stof(s); return true; }
    catch (...) { out = fallback; return false; }
}

inline bool SafeParseInt(const std::string& s, int& out, int fallback = 0) {
    try { out = std::stoi(s); return true; }
    catch (...) { out = fallback; return false; }
}

} // namespace BimCore

// -----------------------------------------------------------------------------
// Logging macros — zero overhead when disabled
// Toggle via CMake: option(BIM_ENABLE_DEBUG_LOG ...)
// -----------------------------------------------------------------------------
#ifdef BIM_ENABLE_DEBUG_LOG
  #include <iostream>
  #define BIM_LOG(channel, msg) \
      std::cout << "[" channel "] " << msg << '\n'
  #define BIM_ERR(channel, msg) \
      std::cerr << "[" channel " ERROR] " << msg << '\n'
#else
  #define BIM_LOG(channel, msg) (void)0
  #define BIM_ERR(channel, msg) (void)0
#endif
