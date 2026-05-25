#pragma once
#include <vulkan/vulkan.h>

namespace passgraph {
  struct ImageState {
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
    VkImageLayout layout;
    
    bool operator==(const ImageState& other) const = default;
  };

  struct ImageResource {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    VkFormat format;
    VkImageUsageFlags usage;

    ImageState state;
  };

} // namespace passgraph
