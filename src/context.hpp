#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "graph.hpp"
#include "types/resource.hpp"

namespace fwrk {
  class Context {
  public:
    explicit Context(VkDevice device, VkPhysicalDevice physical_device, const uint32_t frames_in_flight) :
        device_(device), physical_device_(physical_device), frames_in_flight_(frames_in_flight)
    {
    }
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

    [[nodiscard]] Graph& graph() { return graph_; }

  private:
    friend Graph;
    std::vector<Resource> resources_;

    std::vector<PhysicalImage> images_;
    std::vector<PhysicalBuffer> buffers_;
    std::vector<ResourceID> proxies_;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    uint32_t frames_in_flight_ = 0;
    uint32_t current_frame_ = 0;

    Graph graph_{this};

    [[nodiscard]] VkImageView get_image_view(const ViewKey& key, const Resource& resource);
    void destroy_views(PhysicalImage& image) const;

    [[nodiscard]] PhysicalImage& get_physical_image(uint64_t id, ResourceType type);
    [[nodiscard]] PhysicalBuffer& get_physical_buffer(uint64_t id, ResourceType type);
  };
} // namespace fwrk

#include "context.tpp"
