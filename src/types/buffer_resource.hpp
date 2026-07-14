#pragma once
#include <vulkan/vulkan.h>

#include "physical_state.hpp"

namespace fwrk {
  struct Buffer {
    VkDeviceSize size;
  };

  struct PhysicalBuffer {
    VkBuffer handle;
    PhysicalState state;
  };

  template<typename T>
  concept BufferInterface = requires(const T& buf) {
    { buf.size() } -> std::same_as<VkDeviceSize>;
    { buf.buffer() } -> std::same_as<VkBuffer>;
  };

  struct BufferImportInfo {
    VkDeviceSize size;
    PhysicalState state;
  };

  struct BufferCreateInfo {
    VkDeviceSize size;
  };
} // namespace fwrk
