#pragma once
#include <functional>
#include "types/pass.hpp"
#include "types/resource.hpp"

namespace passgraph {
  class Graph;

  enum class LoadOp { Load = 0, Clear = 1, DontCare = 2 };
  enum class StoreOp { Store = 0, DontCare = 1 };

  struct ResourceAccess {
    ResourceID id;
    std::optional<uint32_t> pass = std::nullopt;
  };

  struct AttachmentInfo {
    ResourceAccess resource;
    LoadOp load_op = LoadOp::DontCare;
    StoreOp store_op = StoreOp::DontCare;
    ClearValue clear_value{};
  };


  template<typename T>
  class PassBuilder {
  public:
    explicit PassBuilder(Pass* pass, Graph* graph, const size_t id) :
        pass_(pass), graph_(graph), id_(static_cast<uint32_t>(id))
    {
    }

    T& set_image_read(const ResourceAccess& resource, VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE);
    T& set_uniform_buffer_read(const ResourceAccess& resource, VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE);
    T& set_storage_buffer_read(const ResourceAccess& resource, VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE);
    T& set_storage_buffer_write(const ResourceAccess& resource,
                                VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE);
    T& set_storage_image_read(const ResourceAccess& resource, VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE);
    T& set_storage_image_write(const ResourceAccess& resource, VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE);

    T& set_execute(std::function<void(VkCommandBuffer)> func);
    [[nodiscard]] uint32_t id() const { return id_; }

  protected:
    Pass* pass_;
    Graph* graph_;
    uint32_t id_;
    std::unordered_set<ResourceID> accessed_;

    void set_buffer_read(const ResourceAccess& resource, VkAccessFlags2 access, VkPipelineStageFlags2 stages);
    void set_stages_fallback(VkPipelineStageFlags2& stages) const;
    void set_possible_read(const ResourceAccess& resource, ResourceDependencies& deps, VkAccessFlags2& access,
                           VkAccessFlags2 access_flags) const;
    bool try_access(ResourceID resource);
  };

  class GraphicsPassBuilder : public PassBuilder<GraphicsPassBuilder> {
  public:
    explicit GraphicsPassBuilder(Pass* pass, Graph* graph, const size_t id) : PassBuilder(pass, graph, id) {}

    GraphicsPassBuilder& set_color_attachment(const AttachmentInfo& info);
    GraphicsPassBuilder& set_depth_attachment(const AttachmentInfo& info);
    GraphicsPassBuilder& set_resolve_attachment(ResourceID resolve_resource, ResourceID color_resource,
                                                VkResolveModeFlags mode);

    GraphicsPassBuilder& set_vertex_buffer_input(const ResourceAccess& resource);
    GraphicsPassBuilder& set_index_buffer_input(const ResourceAccess& resource);
    GraphicsPassBuilder& set_indirect_buffer_input(const ResourceAccess& resource);

    GraphicsPassBuilder& set_render_area(RenderArea area);
  };
} // namespace passgraph
