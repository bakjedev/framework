#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "graph.hpp"
#include "types/resource.hpp"
#include "util/flat_hash_map.hpp"

namespace fwrk {
  template<class T>
  static void hash_combine(size_t& seed, const T& value)
  {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  class Context {
  public:
    explicit Context(VkDevice device) : device_(device), image_views_cache_(100) {}
    ~Context();

    [[nodiscard]] ResourceID import_image(const ImageResource& image, VkImage raw, std::string name = "Unnamed image");

    template<ImageInterface I>
    [[nodiscard]] ResourceID import_image(const I& image, const ImageState& state, std::string name = "Unnamed image");

    [[nodiscard]] ResourceID import_buffer(const BufferResource& buffer, VkBuffer raw,
                                           std::string name = "Unnamed buffer");

    template<BufferInterface I>
    [[nodiscard]] ResourceID import_buffer(const I& buffer, const BufferState& state,
                                           std::string name = "Unnamed buffer");

    [[nodiscard]] Graph& graph() { return graph_; }


  private:
    friend Graph;
    std::vector<Resource> resources_;

    std::vector<ImageResource> images_;
    std::vector<BufferResource> buffers_;

    std::vector<VkImage> raw_images_;
    std::vector<VkBuffer> raw_buffers_;

    VkDevice device_ = VK_NULL_HANDLE;

    Graph graph_{this};

    using ViewKey = std::tuple<VkImage, VkImageAspectFlags, uint32_t, uint32_t, uint32_t, uint32_t, VkFormat>;
    struct ViewKeyHasher {
      size_t operator()(const ViewKey& key) const;
    };
    flat_hash_map<ViewKey, VkImageView, ViewKeyHasher> image_views_cache_;

    VkImageView get_image_view(const ImageAccess& image_access, const Resource& resource);
  };
} // namespace fwrk

#include "context.tpp"
