#pragma once
#include <functional>

namespace fwrk {
  template<class T>
  static void hash_combine(size_t& seed, const T& value)
  {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
} // namespace fwrk
