#pragma once
#include <functional>
#include <unordered_set>
#include "types/pass.hpp"
#include "types/resource.hpp"

namespace fwrk {
  class Graph;

  enum class LoadOp : uint8_t { Load = 0, Clear = 1, DontCare = 2 };
  enum class StoreOp : uint8_t { Store = 0, DontCare = 1 };

  struct ResourceAccess {
    ResourceID id;
    std::optional<uint32_t> pass = std::nullopt;
  };

  struct AttachmentInfo {
    ResourceAccess resource;
    LoadOp load_op = LoadOp::DontCare;
    StoreOp store_op = StoreOp::DontCare;
    ClearValue clear_value{};
    uint32_t layer = 0;
    uint32_t level = 0;
  };

  struct ImageInfo {
    ResourceAccess resource;
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    uint32_t layer = 0;
    uint32_t level = 0;
  };

  struct BufferInfo {
    ResourceAccess resource;
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkDeviceSize size = VK_WHOLE_SIZE;
    VkDeviceSize offset = 0;
  };

  struct ResolveInfo {
    ResourceID resource;
    ResourceID color;
    VkResolveModeFlags mode = VK_RESOLVE_MODE_AVERAGE_BIT;
  };

  template<typename T>
  class PassBuilder {
  public:
    explicit PassBuilder(Pass* pass, Graph* graph, const size_t id) :
        pass_(pass), graph_(graph), id_(static_cast<uint32_t>(id))
    {
    }

    T& set_image_read(const ImageInfo& info);
    T& set_uniform_buffer_read(const BufferInfo& info);
    T& set_storage_buffer_read(const BufferInfo& info);
    T& set_storage_buffer_write(const BufferInfo& info);
    T& set_storage_image_read(const ImageInfo& info);
    T& set_storage_image_write(const ImageInfo& info);

    T& set_execute(std::function<void(VkCommandBuffer)> func);
    [[nodiscard]] uint32_t id() const { return id_; }

  protected:
    Pass* pass_;
    Graph* graph_;
    uint32_t id_;
    std::unordered_set<ResourceID> accessed_;

    void set_buffer_read(const BufferInfo& info, VkAccessFlags2 access);
    void set_stages_fallback(VkPipelineStageFlags2& stages) const;
    void set_possible_read(const ResourceAccess& resource, ResourceDependencies& deps, VkAccessFlags2& access,
                           VkAccessFlags2 access_flags) const;
    bool try_access(ResourceID resource);
    void set_possible_explicit_read(std::optional<uint32_t> pass, ResourceDependencies& deps) const;
  };

  class GraphicsPassBuilder : public PassBuilder<GraphicsPassBuilder> {
  public:
    explicit GraphicsPassBuilder(Pass* pass, Graph* graph, const size_t id) : PassBuilder(pass, graph, id) {}

    GraphicsPassBuilder& set_color_attachment(const AttachmentInfo& info);
    GraphicsPassBuilder& set_depth_attachment(const AttachmentInfo& info);
    GraphicsPassBuilder& set_resolve_attachment(const ResolveInfo& info);

    GraphicsPassBuilder& set_vertex_buffer_input(const BufferInfo& info);
    GraphicsPassBuilder& set_index_buffer_input(const BufferInfo& info);
    GraphicsPassBuilder& set_indirect_buffer_input(const BufferInfo& info);

    GraphicsPassBuilder& set_render_area(RenderArea area);
  };

  class ComputePassBuilder : public PassBuilder<ComputePassBuilder> {
  public:
    explicit ComputePassBuilder(Pass* pass, Graph* graph, const size_t id) : PassBuilder(pass, graph, id) {}
  };
} // namespace fwrk
