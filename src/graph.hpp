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

namespace passgraph {
  class Context;

  class Graph {
  public:
    void set_image_end_state(ResourceID resource, const ImageState& state);
    void set_buffer_end_state(ResourceID resource, const BufferState& state);

    [[nodiscard]] GraphicsPassBuilder add_graphics_pass(std::string name = "Unnamed pass");

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

    struct ResourceInfo {
      std::unordered_set<uint32_t> write_passes;
      std::unordered_set<uint32_t> read_passes;
      std::unordered_map<uint32_t, uint32_t> read_deps;
    };

    std::unordered_map<ResourceID, ResourceInfo> resource_infos_;

    std::vector<Pass> passes_;

    std::vector<uint32_t> sorted_pass_ids_;
    std::vector<CompiledPass> compiled_passes_;

    std::unordered_map<ResourceID, ImageState> end_image_states_;
    std::unordered_map<ResourceID, BufferState> end_buffer_states_;
    DependencyInfo end_dep_info_;

    Context* context_;
  };
} // namespace passgraph
