#pragma once
#include <vulkan/vulkan.h>

namespace fwrk {
  struct PhysicalState {
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stages;
    VkImageLayout layout;

    constexpr PhysicalState() = default;

    constexpr PhysicalState(const VkAccessFlags2 access_, const VkPipelineStageFlags2 stages_,
                            const VkImageLayout layout_) : access(access_), stages(stages_), layout(layout_)
    {
    }

    constexpr PhysicalState(const VkAccessFlags2 access_, const VkPipelineStageFlags2 stages_) :
        access(access_), stages(stages_), layout(VK_IMAGE_LAYOUT_UNDEFINED)
    {
    }

    static const PhysicalState Undefined;
    bool operator==(const PhysicalState& other) const = default;
  };
  constexpr PhysicalState PhysicalState::Undefined{VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE,
                                                   VK_IMAGE_LAYOUT_UNDEFINED};
} // namespace fwrk
