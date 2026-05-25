#pragma once
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "pass_builder.hpp"
#include "types/buffer_resource.hpp"
#include "types/image_resource.hpp"
#include "types/pass.hpp"
#include "types/resource.hpp"

namespace passgraph {
  class Graph {
  public:
    [[nodiscard]] ResourceID import_image(const ImageResource& image, VkImage raw, std::string name = "Unnamed image");

    [[nodiscard]] ResourceID import_buffer(const BufferResource& buffer, VkBuffer raw,
                                           std::string name = "Unnamed buffer");

    bool update_image_raw(ResourceID resource, VkImage raw);
    bool update_buffer_raw(ResourceID resource, VkBuffer raw);

    [[nodiscard]] PassBuilder add_pass(QueueFlags queue_flags, std::string name = "Unnamed pass");

    [[nodiscard]] bool compile();

    void execute(VkCommandBuffer cmd) const;

  private:
    friend class PassBuilder;

    struct DependencyInfo {
      VkDependencyInfo dep_info;
      std::vector<VkImageMemoryBarrier2> image_barriers;
      std::vector<VkBufferMemoryBarrier2> buffer_barriers;
    };

    std::vector<Resource> resources_;

    std::vector<ImageResource> images_;
    std::vector<BufferResource> buffers_;

    std::vector<VkImage> raw_images_;
    std::vector<VkBuffer> raw_buffers_;

    std::vector<Pass> passes_;

    std::vector<uint32_t> sorted_pass_ids_;
    std::vector<DependencyInfo> pass_dep_infos_;
  };
} // namespace passgraph
