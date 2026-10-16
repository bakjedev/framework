#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "allocator.hpp"
#include "graph.hpp"
#include "types/resource.hpp"

namespace fwrk {
  struct VulkanInfo {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t frames_in_flight = 0;
  };

  struct DebugMarkerInfo {
    PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin;
    PFN_vkCmdEndDebugUtilsLabelEXT cmd_end;
  };

  struct ContextCreateInfo {
    VulkanInfo vulkan;
    Allocator* allocator = nullptr;
    std::optional<DebugMarkerInfo> debug_markers;
    bool timestamps = false;
  };

  class Context {
  public:
    using CreateInfo = ContextCreateInfo;

    explicit Context(const CreateInfo& info);
    Context(VkDevice, uint32_t, Allocator&&) = delete;
    ~Context();

    [[nodiscard]] ResourceID import_image(const ImageImportInfo& info, VkImage raw, std::string name = "Unnamed image");

    [[nodiscard]] ResourceID import_buffer(const BufferImportInfo& info, VkBuffer raw,
                                           std::string name = "Unnamed buffer");

    template<ImageInterface I>
    [[nodiscard]] ResourceID import_image(const I& image, const PhysicalState& state,
                                          std::string name = "Unnamed image");

    template<BufferInterface I>
    [[nodiscard]] ResourceID import_buffer(const I& buffer, const PhysicalState& state,
                                           std::string name = "Unnamed buffer");

    void update_image(ResourceID resource, const ImageImportInfo& info, VkImage raw);
    void update_buffer(ResourceID resource, const BufferImportInfo& info, VkBuffer raw);

    template<ImageInterface I>
    void update_image(ResourceID resource, const I& image, const PhysicalState& state);

    template<BufferInterface I>
    void update_buffer(ResourceID resource, const I& buffer, const PhysicalState& state);

    [[nodiscard]] ResourceID create_proxy(ResourceID resource = {});
    void update_proxy(ResourceID proxy, ResourceID resource);

    [[nodiscard]] VkImageView acquire_image_view(ResourceID resource, const ViewKey& view_key,
                                                 std::optional<uint32_t> frame_index = std::nullopt);
    [[nodiscard]] std::vector<VkImageView> get_image_views(ResourceID resource);
    [[nodiscard]] std::optional<VkImageView> get_first_image_view(ResourceID resource);
    [[nodiscard]] VkImage get_raw_image(ResourceID resource, std::optional<uint32_t> frame_index = std::nullopt);
    [[nodiscard]] VkBuffer get_raw_buffer(ResourceID resource, std::optional<uint32_t> frame_index = std::nullopt);

    [[nodiscard]] uint32_t current_frame() const { return frame_index_; }

    [[nodiscard]] Graph& graph() { return graph_; }

  private:
    friend Graph;
    std::vector<Resource> resources_;
    std::vector<Resource> transients_;
    std::vector<ResourceID> proxies_;

    std::vector<std::pair<uint64_t, std::vector<Resource>>> deletion_queue_;

    std::vector<PhysicalImage> images_;
    std::vector<PhysicalBuffer> buffers_;

    std::vector<PhysicalImage> transient_images_;
    std::vector<PhysicalBuffer> transient_buffers_;

    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t frames_in_flight_ = 0;
    uint32_t frame_index_ = 0;
    uint64_t frame_ = 0;

    Allocator* alloc_;

    PFN_vkCmdBeginDebugUtilsLabelEXT begin_debug_pfn_ = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT end_debug_pfn_ = nullptr;

    bool timestamps_ = false;

    Graph graph_{this};

    void allocate_transients(std::vector<Graph::TransientInfo> infos);
    void schedule_destroy_transients();
    void destroy_transients(uint32_t frame_index);
    void destroy_transient(const Resource& transient);

    [[nodiscard]] VkImageView get_image_view(const ViewKey& key, const Image& image, PhysicalImage& phys) const;
    void destroy_views(PhysicalImage& image) const;

    [[nodiscard]] ResourceID resolve_proxy(ResourceID resource) const;
    [[nodiscard]] Resource& get_resource(ResourceID id);

    [[nodiscard]] PhysicalImage& get_physical_image(uint64_t id, ResourceType type,
                                                    std::optional<uint32_t> frame_index = std::nullopt);
    [[nodiscard]] PhysicalBuffer& get_physical_buffer(uint64_t id, ResourceType type,
                                                      std::optional<uint32_t> frame_index = std::nullopt);
  };
} // namespace fwrk

#include "context.tpp"
