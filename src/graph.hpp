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
  class Context;

  class Graph {
  public:
    void set_image_end_state(ResourceID resource, const ImageState& state);
    void set_buffer_end_state(ResourceID resource, const BufferState& state);

    [[nodiscard]] PassBuilder add_pass(QueueFlags queue_flags, std::string name = "Unnamed pass");

    bool compile();

    void execute(VkCommandBuffer cmd) const;

  private:
    friend Context;
    friend PassBuilder;
    explicit Graph(Context* context) : context_(context) {}

    struct DependencyInfo {
      VkDependencyInfo dep_info{};
      std::vector<VkImageMemoryBarrier2> image_barriers;
      std::vector<VkBufferMemoryBarrier2> buffer_barriers;
    };

    struct ResourceInfo {
      std::unordered_set<uint32_t> write_passes;
      std::unordered_set<uint32_t> read_passes;
      std::optional<uint32_t> last_writer;
    };

    std::unordered_map<ResourceID, ResourceInfo> resource_infos_;

    std::vector<Pass> passes_;

    std::vector<uint32_t> sorted_pass_ids_;
    std::vector<DependencyInfo> pass_dep_infos_;

    std::unordered_map<ResourceID, ImageState> end_image_states_;
    std::unordered_map<ResourceID, BufferState> end_buffer_states_;
    DependencyInfo end_dep_info_;

    Context* context_;
  };
} // namespace passgraph
