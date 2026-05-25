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

    bool update_image(ResourceID resource, VkImage raw, const ImageState& state);
    bool update_buffer(ResourceID resource, VkBuffer raw, const BufferState& state);

    bool set_image_end_state(ResourceID resource, const ImageState& state);
    bool set_buffer_end_state(ResourceID resource, const BufferState& state);

    [[nodiscard]] PassBuilder add_pass(QueueFlags queue_flags, std::string name = "Unnamed pass");

    bool compile();

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

    std::vector<std::optional<ImageState>> end_image_states_;
    std::vector<std::optional<BufferState>> end_buffer_states_;
    DependencyInfo end_dep_info_;
  };
} // namespace passgraph
