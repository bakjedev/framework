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

  using ViewKey = std::tuple<VkImageAspectFlags, uint32_t, uint32_t, uint32_t, uint32_t, VkImageViewType>;

  struct ViewKeyHasher {
    size_t operator()(const ViewKey& key) const
    {
      size_t seed = 0;
      hash_combine(seed, std::get<0>(key));
      hash_combine(seed, std::get<1>(key));
      hash_combine(seed, std::get<2>(key));
      hash_combine(seed, std::get<3>(key));
      hash_combine(seed, std::get<4>(key));
      hash_combine(seed, std::get<5>(key));
      return seed;
    }
  };

  class Context {
  public:
    explicit Context(VkDevice device) : device_(device) {}
    ~Context();

    [[nodiscard]] ResourceID import_image(const ImageResource& image, VkImage raw, std::string name = "Unnamed image");

    [[nodiscard]] ResourceID import_buffer(const BufferResource& buffer, VkBuffer raw,
                                           std::string name = "Unnamed buffer");

    template<ImageInterface I>
    [[nodiscard]] ResourceID import_image(const I& image, const ImageState& state, std::string name = "Unnamed image");

    template<BufferInterface I>
    [[nodiscard]] ResourceID import_buffer(const I& buffer, const BufferState& state,
                                           std::string name = "Unnamed buffer");

    void update_image(ResourceID resource, const ImageResource& image, VkImage raw);
    void update_buffer(ResourceID resource, const BufferResource& buffer, VkBuffer raw);

    template<ImageInterface I>
    void update_image(ResourceID resource, const I& image, const ImageState& state);

    template<BufferInterface I>
    void update_buffer(ResourceID resource, const I& buffer, const BufferState& state);

    [[nodiscard]] ResourceID create_alias(ResourceID resource = {}, std::string name = "Unnamed alias");
    void update_alias(ResourceID alias, ResourceID resource);

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

    std::vector<flat_hash_map<ViewKey, VkImageView, ViewKeyHasher>> views_cache_{};

    VkImageView get_image_view(const VkImageSubresourceRange& subresource, VkImageViewType view_type,
                               const Resource& resource);
    void destroy_views(uint32_t slot);

    template<ImageInterface I>
    static ImageResource construct_image(const I& image, const ImageState& state);
  };
} // namespace fwrk

#include "context.tpp"
