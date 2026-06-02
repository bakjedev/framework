#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vulkan/vulkan.h>
#include "resource.hpp"

namespace passgraph {
  struct ClearValue {
    float r{};
    float g{};
    float b{};
    float a{};
  };

  struct Attachment {
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ClearValue clear_value;
    bool is_depth = false;
    std::optional<ResourceID> resolve = std::nullopt;
    VkResolveModeFlags resolve_mode = VK_RESOLVE_MODE_NONE;
  };

  struct RenderArea {
    uint32_t width{};
    uint32_t height{};
  };

  struct ImageAccess {
    ResourceID resource;
    std::optional<Attachment> attachment = std::nullopt;

    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  struct BufferAccess {
    ResourceID resource;

    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
  };


  struct Pass {
    std::string name;
    std::function<void(VkCommandBuffer cmd)> func;
    std::vector<ImageAccess> images;
    std::vector<BufferAccess> buffers;
    std::optional<RenderArea> render_area;
  };
} // namespace passgraph
