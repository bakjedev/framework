#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vulkan/vulkan.h>
#include "resource.hpp"

namespace passgraph {
  struct ImageAccess {
    ResourceID resource;
    std::optional<uint32_t> pass;

    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  struct BufferAccess {
    ResourceID id;
    std::optional<uint32_t> pass;

    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
  };

  struct Pass {
    std::string name;
    std::function<void()> func;
    std::vector<ImageAccess> images;
    std::vector<BufferAccess> buffers;
  };
} // namespace passgraph
