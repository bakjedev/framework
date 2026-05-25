#pragma once
#include <vulkan/vulkan.h>

namespace passgraph {
  struct BufferState {
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;

    bool operator==(const BufferState& other) const = default;
  };

  struct BufferResource {
    VkDeviceSize size;
    VkBufferUsageFlags usage;

    BufferState state;
  };
} // namespace passgraph
