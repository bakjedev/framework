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

  struct ResolveInfo {
    ResourceID resource;
    VkResolveModeFlags mode = VK_RESOLVE_MODE_AVERAGE_BIT;
    uint32_t base_level = 0;
    uint32_t base_layer = 0;
  };

  struct AttachmentInfo {
    ResourceAccess resource;
    LoadOp load_op = LoadOp::DontCare;
    StoreOp store_op = StoreOp::DontCare;
    ClearValue clear_value{};
    uint32_t base_level = 0;
    uint32_t base_layer = 0;
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;
    std::optional<ResolveInfo> resolve = std::nullopt;
  };

  struct ImageInfo {
    ResourceAccess resource;
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    uint32_t base_level = 0;
    uint32_t level_count = VK_REMAINING_MIP_LEVELS;
    uint32_t base_layer = 0;
    uint32_t layer_count = VK_REMAINING_ARRAY_LAYERS;
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;
  };

  struct BufferInfo {
    ResourceAccess resource;
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkDeviceSize size = VK_WHOLE_SIZE;
    VkDeviceSize offset = 0;
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
    T& set_image_transfer_src(const ImageInfo& info);
    T& set_image_transfer_dst(const ImageInfo& info);
    T& set_buffer_transfer_src(const BufferInfo& info);
    T& set_buffer_transfer_dst(const BufferInfo& info);

    T& set_execute(std::function<void(VkCommandBuffer)> func);
    [[nodiscard]] uint32_t id() const { return id_; }

  protected:
    Pass* pass_;
    Graph* graph_;
    uint32_t id_;

    void set_buffer_read(const BufferInfo& info, VkAccessFlags2 access) const;
    void set_stages_fallback(VkPipelineStageFlags2& stages) const;
    void set_possible_read(const ResourceAccess& resource, ResourceDependencies& deps, VkAccessFlags2& access,
                           VkAccessFlags2 access_flags) const;

    [[nodiscard]] bool try_access_image_internal(ResourceID resource, VkImageAspectFlags aspect, uint32_t base_level,
                                                 uint32_t level_count, uint32_t base_layer, uint32_t layer_count) const;
    [[nodiscard]] bool try_access_image(const AttachmentInfo& info, bool resolve = false) const;
    [[nodiscard]] bool try_access_image(const ImageInfo& info) const;
    static bool ranges_overlap(uint32_t x, uint32_t x_count, uint32_t y, uint32_t y_count);

    [[nodiscard]] bool try_access_buffer(ResourceID resource, VkDeviceSize offset, VkDeviceSize size) const;
    static bool ranges_overlap(VkDeviceSize x, VkDeviceSize x_size, VkDeviceSize y, VkDeviceSize y_size);


    void set_possible_explicit_read(std::optional<uint32_t> pass, ResourceDependencies& deps) const;
  };

  class GraphicsPassBuilder : public PassBuilder<GraphicsPassBuilder> {
  public:
    explicit GraphicsPassBuilder(Pass* pass, Graph* graph, const size_t id) : PassBuilder(pass, graph, id) {}

    GraphicsPassBuilder& set_color_attachment(const AttachmentInfo& info);
    GraphicsPassBuilder& set_depth_attachment(const AttachmentInfo& info);

    GraphicsPassBuilder& set_vertex_buffer_input(const BufferInfo& info);
    GraphicsPassBuilder& set_index_buffer_input(const BufferInfo& info);
    GraphicsPassBuilder& set_indirect_buffer_input(const BufferInfo& info);

    GraphicsPassBuilder& set_render_area(VkExtent2D area);
    GraphicsPassBuilder& set_render_view(uint32_t layer_count, uint32_t view_mask = 0);
  };

  class ComputePassBuilder : public PassBuilder<ComputePassBuilder> {
  public:
    explicit ComputePassBuilder(Pass* pass, Graph* graph, const size_t id) : PassBuilder(pass, graph, id) {}
  };
} // namespace fwrk
