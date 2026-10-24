#pragma once
#include <optional>
#include <span>
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

  struct BarrierBatch {
    std::vector<VkImageMemoryBarrier2> images;
    std::vector<VkBufferMemoryBarrier2> buffers;

    void clear();
    void record(VkCommandBuffer cmd) const;
  };

  struct Rendering {
    std::vector<VkRenderingAttachmentInfo> colors;
    std::optional<VkRenderingAttachmentInfo> depth;
    VkRenderingInfo info;

    void clear();
    void begin(VkCommandBuffer cmd) const;
  };

  class Graph {
  public:
    struct PassTimings {
      std::string_view name;
      uint64_t begin;
      uint64_t end;
    };

    struct FrameTimings {
      uint64_t begin;
      uint64_t end;
      std::span<const PassTimings> passes;
    };

    ~Graph();

    void set_image_end_state(ResourceID resource, const PhysicalState& state);
    void set_buffer_end_state(ResourceID resource, const PhysicalState& state);

    [[nodiscard]] GraphicsPassBuilder add_graphics_pass(std::string name = "Unnamed graphics pass");
    [[nodiscard]] ComputePassBuilder add_compute_pass(std::string name = "Unnamed compute pass");

    [[nodiscard]] ResourceID create_image(const ImageCreateInfo& info, std::string name = "Unnamed transient image");
    [[nodiscard]] ResourceID create_buffer(const BufferCreateInfo& info, std::string name = "Unnamed transient buffer");

    bool compile();

    void execute(VkCommandBuffer cmd, uint32_t frame_index);

    [[nodiscard]] FrameTimings get_timings() const;

  private:
    friend Context;
    template<typename T>
    friend class PassBuilder;
    friend class GraphicsPassBuilder;

    explicit Graph(Context* context);

    struct AttachmentResolveInfo {
      ResourceID resource;
      VkResolveModeFlags mode;
      VkImageSubresourceRange subresource;
    };

    struct RenderingAttachmentInfo {
      ResourceID resource;
      VkImageSubresourceRange subresource;
      VkImageViewType view_type;
      VkImageLayout layout;
      VkAttachmentLoadOp load_op;
      VkAttachmentStoreOp store_op;
      VkClearValue clear_value;
      std::optional<AttachmentResolveInfo> resolve;
    };

    struct RenderingInfo {
      std::vector<RenderingAttachmentInfo> color_atts;
      std::optional<RenderingAttachmentInfo> depth_att;
      RenderInfo render_info;
    };

    struct ImageBarrierInfo {
      ResourceID resource;
      PhysicalState dst_state{};
      VkImageSubresourceRange subresource_range{};
    };

    struct BufferBarrierInfo {
      ResourceID resource;
      PhysicalState dst_state{};
      VkDeviceSize size{};
      VkDeviceSize offset{};
    };

    struct DependencyInfo {
      std::vector<ImageBarrierInfo> image_barriers;
      std::vector<BufferBarrierInfo> buffer_barriers;
    };

    struct CompiledPass {
      DependencyInfo deps;
      std::optional<RenderingInfo> render;
      std::string name;
      std::function<void(VkCommandBuffer cmd)> func;
    };

    using ResolvedImage = std::pair<const Image&, PhysicalImage&>;

    using TransientDesc = std::variant<ImageCreateInfo, BufferCreateInfo>;
    struct TransientInfo {
      TransientDesc desc;
      std::string name;
    };

    struct QueryPool {
      VkQueryPool pool = VK_NULL_HANDLE;
      uint32_t capacity = 0;
      uint32_t count = 0;
    };

    flat_hash_map<ResourceID, ResourceDependencies> resource_deps_;

    std::vector<Pass> passes_;

    std::vector<uint32_t> sorted_pass_ids_;
    std::vector<CompiledPass> compiled_passes_;

    BarrierBatch barriers_;
    Rendering rendering_;

    flat_hash_map<ResourceID, PhysicalState> end_image_states_;
    flat_hash_map<ResourceID, PhysicalState> end_buffer_states_;

    std::vector<TransientInfo> transient_infos_;

    std::vector<QueryPool> query_pools_;
    std::vector<uint64_t> query_results_;
    std::vector<PassTimings> pass_timings_;
    float timestamp_period_ = 0.0f;

    [[nodiscard]] ResolvedImage resolve_image(ResourceID id) const;
    [[nodiscard]] PhysicalBuffer& resolve_buffer(ResourceID id) const;

    void create_pass_barriers(const DependencyInfo& deps);
    void create_end_barriers();

    VkRenderingAttachmentInfo create_attachment(const RenderingAttachmentInfo& att, VkExtent2D& extent) const;
    void create_rendering(const RenderingInfo& info);

    static void transition_image(std::vector<VkImageMemoryBarrier2>& barriers, PhysicalImage& image,
                                 const PhysicalState& state, const VkImageSubresourceRange& subresource_range);

    static void transition_buffer(std::vector<VkBufferMemoryBarrier2>& barriers, PhysicalBuffer& buffer,
                                  const PhysicalState& state, VkDeviceSize offset, VkDeviceSize size);

    void begin_debug_label(VkCommandBuffer cmd, const std::string& name) const;

    void get_timestamps(const QueryPool& pool);
    void reset_timestamps(QueryPool& pool, VkCommandBuffer cmd, uint32_t count) const;

    Context* context_;

    static VkImageAspectFlags get_aspect_for_format(VkFormat format);
  };
} // namespace fwrk
