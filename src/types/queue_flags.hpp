#pragma once
#include <cstdint>

namespace passgraph {
  using QueueFlagsUnderlying = uint8_t;
  enum class QueueFlags : QueueFlagsUnderlying {
    None = 0,
    Graphics = 1 << 0,
    Compute = 1 << 1,
  };

  constexpr QueueFlags operator|(QueueFlags lhs, QueueFlags rhs)
  {
    return static_cast<QueueFlags>(static_cast<QueueFlagsUnderlying>(lhs) | static_cast<QueueFlagsUnderlying>(rhs));
  }

  constexpr QueueFlags operator&(QueueFlags lhs, QueueFlags rhs)
  {
    return static_cast<QueueFlags>(static_cast<QueueFlagsUnderlying>(lhs) & static_cast<QueueFlagsUnderlying>(rhs));
  }
} // namespace passgraph
