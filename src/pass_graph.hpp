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
    [[nodiscard]] ResourceID import_image(std::string name, const ImageResource& image, VkImage raw);

    [[nodiscard]] ResourceID import_buffer(std::string name, const BufferResource& buffer, VkBuffer raw);

    [[nodiscard]] PassBuilder add_pass(std::string name, QueueFlags queue_flags);

    [[nodiscard]] bool compile();

    void execute(VkCommandBuffer cmd) const;

  private:
    friend class PassBuilder;

    std::vector<Resource> resources_;

    std::vector<ImageResource> images_;
    std::vector<BufferResource> buffers_;

    std::vector<VkImage> raw_images_;
    std::vector<VkBuffer> raw_buffers_;

    std::vector<Pass> passes_;

    std::vector<uint32_t> sorted_pass_ids_;
    std::vector<VkDependencyInfo> pass_dep_infos_;
  };
} // namespace passgraph
