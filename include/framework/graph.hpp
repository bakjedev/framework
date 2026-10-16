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

    struct ImageMemoryBarrier {
      ResourceID resource;
      PhysicalState dst_state{};
      VkImageSubresourceRange subresource_range{};
    };

    struct BufferMemoryBarrier {
      ResourceID resource;
      PhysicalState dst_state{};
      VkDeviceSize size{};
      VkDeviceSize offset{};
    };

    struct DependencyInfo {
      std::vector<ImageMemoryBarrier> image_barriers;
      std::vector<BufferMemoryBarrier> buffer_barriers;
    };

    struct CompiledPass {
      DependencyInfo deps;
      std::optional<RenderingInfo> render;
      VkDebugUtilsLabelEXT debug;
      std::string name;
      std::function<void(VkCommandBuffer cmd)> func;
    };

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

    flat_hash_map<ResourceID, PhysicalState> end_image_states_;
    flat_hash_map<ResourceID, PhysicalState> end_buffer_states_;

    std::vector<TransientInfo> transient_infos_;

    std::vector<QueryPool> query_pools_;
    std::vector<uint64_t> query_results_;
    std::vector<PassTimings> pass_timings_;
    float timestamp_period_ = 0.0f;

    void get_timestamps(const QueryPool& pool);
    void reset_timestamps(QueryPool& pool, VkCommandBuffer cmd, uint32_t count) const;

    Context* context_;

    static VkImageAspectFlags get_aspect_for_format(VkFormat format);
  };
} // namespace fwrk
