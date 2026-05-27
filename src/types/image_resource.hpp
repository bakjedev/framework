#pragma once
#include <vulkan/vulkan.h>

namespace passgraph {
  struct ImageState {
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
    VkImageLayout layout;

    static const ImageState Undefined;

    bool operator==(const ImageState& other) const = default;
  };
  constexpr ImageState ImageState::Undefined{VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED};

  struct ImageResource {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    VkFormat format;
    VkImageUsageFlags usage;
    VkImageAspectFlags aspect;

    ImageState state;
  };

} // namespace passgraph
