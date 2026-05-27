#pragma once
#include <vulkan/vulkan.h>

namespace passgraph {
  struct BufferState {
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;

    static const BufferState Undefined;

    bool operator==(const BufferState& other) const = default;
  };
  constexpr BufferState BufferState::Undefined{VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE};

  struct BufferResource {
    VkDeviceSize size;
    VkBufferUsageFlags usage;

    BufferState state;
  };
} // namespace passgraph
