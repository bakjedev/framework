#pragma once
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "pass_builder.hpp"
#include "types/buffer_resource.hpp"
#include "types/image_resource.hpp"
#include "types/pass.hpp"
#include "types/resource.hpp"

namespace fwrk {
  class Context;

  class Graph {
  public:
    void set_image_end_state(ResourceID resource, const ImageState& state);
    void set_buffer_end_state(ResourceID resource, const BufferState& state);

    [[nodiscard]] GraphicsPassBuilder add_graphics_pass(std::string name = "Unnamed graphics pass");
    [[nodiscard]] ComputePassBuilder add_compute_pass(std::string name = "Unnamed compute pass");

    bool compile();

    void execute(VkCommandBuffer cmd);

  private:
    friend Context;
    template<typename T>
    friend class PassBuilder;
    friend class GraphicsPassBuilder;

    explicit Graph(Context* context) : context_(context) {}

    struct DependencyInfo {
      VkDependencyInfo dep_info{};
      std::vector<VkImageMemoryBarrier2> image_barriers;
      std::vector<VkBufferMemoryBarrier2> buffer_barriers;
    };

    struct RenderingInfo {
      VkRenderingInfo rendering_info{};
      std::vector<VkRenderingAttachmentInfo> attachment_infos;
      std::optional<VkRenderingAttachmentInfo> depth_info;
    };

    struct CompiledPass {
      DependencyInfo deps;
      std::optional<RenderingInfo> render;
      std::string name;
      std::function<void(VkCommandBuffer cmd)> func;
    };

    flat_hash_map<ResourceID, ResourceDependencies> resource_deps_;

    std::vector<Pass> passes_;

    std::vector<uint32_t> sorted_pass_ids_;
    std::vector<CompiledPass> compiled_passes_;

    flat_hash_map<ResourceID, ImageState> end_image_states_;
    flat_hash_map<ResourceID, BufferState> end_buffer_states_;
    DependencyInfo end_dep_info_;

    Context* context_;
  };
} // namespace fwrk
