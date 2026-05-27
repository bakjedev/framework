#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "graph.hpp"
#include "types/resource.hpp"

namespace passgraph {
  class Context {
  public:
    [[nodiscard]] ResourceID import_image(const ImageResource& image, VkImage raw, VkImageView view,
                                          std::string name = "Unnamed image");

    [[nodiscard]] ResourceID import_buffer(const BufferResource& buffer, VkBuffer raw,
                                           std::string name = "Unnamed buffer");

    [[nodiscard]] Graph create_graph() { return Graph{this}; }


  private:
    friend Graph;
    std::vector<Resource> resources_;

    std::vector<ImageResource> images_;
    std::vector<BufferResource> buffers_;

    std::vector<VkImage> raw_images_;
    std::vector<VkImageView> raw_image_views_;
    std::vector<VkBuffer> raw_buffers_;
  };
} // namespace passgraph
