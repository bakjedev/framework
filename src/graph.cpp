#include "graph.hpp"
#include <algorithm>
#include <iostream>
#include "context.hpp"
#include "types/pass.hpp"


void passgraph::Graph::set_image_end_state(const ResourceID resource, const ImageState& state)
{
  end_image_states_[resource] = state;
}

void passgraph::Graph::set_buffer_end_state(const ResourceID resource, const BufferState& state)
{
  end_buffer_states_[resource] = state;
}

passgraph::PassBuilder passgraph::Graph::add_pass(const QueueFlags queue_flags, std::string name)
{
  const auto id = passes_.size();
  Pass& pass = passes_.emplace_back();
  pass.name = std::move(name);
  pass.queue_flags = queue_flags;
  return PassBuilder{&passes_.back(), this, id};
}

bool passgraph::Graph::compile()
{
  // --------------
  // you like DAGs?
  // --------------
  std::vector<std::pair<std::unordered_set<uint32_t>, std::unordered_set<uint32_t>>> dag{passes_.size()};
  // first = incoming, second = outgoing

  for (const auto& [_, info]: resource_infos_) {
    std::vector<uint32_t> sorted_writes{info.write_passes.begin(), info.write_passes.end()};
    std::ranges::sort(sorted_writes);
    const auto write_count = static_cast<uint32_t>(sorted_writes.size());

    std::vector<uint32_t> sorted_reads{info.read_passes.begin(), info.read_passes.end()};
    std::ranges::sort(sorted_reads);
    const auto read_count = static_cast<uint32_t>(sorted_reads.size());

    std::optional<uint32_t> previous_access;
    bool previous_write = false;
    bool previous_read = false;

    uint32_t write_idx = 0;
    uint32_t read_idx = 0;
    while (write_idx < write_count || read_idx < read_count) {
      uint32_t current_access;
      bool current_write = false;
      bool current_read = false;

      // if we still have a write and a read and they happen in the same pass
      if (write_idx < write_count && read_idx < read_count && sorted_writes[write_idx] == sorted_reads[read_idx]) {
        current_access = sorted_writes[write_idx];
        current_write = true;
        current_read = true;
        write_idx++;
        read_idx++;
        // if we still have a write and either we have no more reads or the write comes earlier than the read
      } else if (write_idx < write_count &&
                 (read_idx >= read_count || sorted_writes[write_idx] < sorted_reads[read_idx])) {
        current_access = sorted_writes[write_idx];
        current_write = true;
        write_idx++;
        // if we still have a read but no more writes
      } else {
        current_access = sorted_reads[read_idx];
        current_read = true;
        read_idx++;
      }

      // if we've accessed before and that was in a different pass
      if (previous_access.has_value() && *previous_access != current_access) {
        const bool raw = previous_write && current_read;
        const bool waw = previous_write && current_write;
        const bool war = previous_read && current_write;
        [[maybe_unused]] const bool rar = previous_read && current_read; // don't care

        if (raw || waw || war) {
          dag[current_access].first.insert(*previous_access);
          dag[*previous_access].second.insert(current_access);
        }
      }

      previous_access = current_access;
      previous_write = current_write;
      previous_read = current_read;
    }
  }

  // ----------------
  // Topological sort
  // ----------------
  {
    std::vector<uint32_t> sorted_passes;
    std::unordered_set<uint32_t> root_nodes;

    // find all nodes with no incoming edges
    for (uint32_t i = 0; i < dag.size(); i++) {
      if (dag[i].first.empty()) {
        root_nodes.insert(i);
      }
    }

    // sort
    while (!root_nodes.empty()) {
      auto node = root_nodes.extract(root_nodes.begin()).value();
      sorted_passes.push_back(node);

      auto& node_out = dag[node].second;
      for (auto out_it = node_out.begin(); out_it != node_out.end();) {
        // remove edge
        auto out_node = *out_it;
        auto& out_node_ins = dag[out_node].first;
        out_node_ins.erase(out_node_ins.find(node)); // unsafe
        out_it = node_out.erase(out_it);

        // recurse
        if (out_node_ins.empty()) {
          root_nodes.insert(out_node);
        }
      }
    }
    sorted_pass_ids_ = std::move(sorted_passes);
  }

  // --------------------
  // Create pass barriers
  // --------------------
  pass_dep_infos_.resize(passes_.size());

  for (const uint32_t pass_id: sorted_pass_ids_) {
    Pass& pass = passes_[pass_id];
    auto& [dep_info, image_barriers, buffer_barriers] = pass_dep_infos_[pass_id];

    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.dependencyFlags = 0u;

    // image memory barriers
    for (const ImageAccess& image_access: pass.images) {
      const ImageState new_state{
          .access = image_access.access, .stage = image_access.stage, .layout = image_access.layout};
      const Resource& resource = context_->resources_.at(*image_access.resource.id); // sorta unsafe
      ImageResource& image = context_->images_.at(resource.slot);

      if (new_state != image.state) {
        VkImageMemoryBarrier2& barrier = image_barriers.emplace_back(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, nullptr);
        barrier.srcStageMask = image.state.stage;
        barrier.srcAccessMask = image.state.access;
        barrier.dstStageMask = new_state.stage;
        barrier.dstAccessMask = new_state.access;
        barrier.oldLayout = image.state.layout;
        barrier.newLayout = new_state.layout;
        barrier.image = context_->raw_images_.at(resource.raw);
        barrier.subresourceRange.aspectMask = image.aspect;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;
        image.state = new_state;
      }
    }
    dep_info.imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size());
    dep_info.pImageMemoryBarriers = image_barriers.data();

    // buffer memory barriers
    for (const BufferAccess& buffer_access: pass.buffers) {
      const BufferState new_state{.access = buffer_access.access, .stage = buffer_access.stage};
      const Resource& resource = context_->resources_.at(*buffer_access.resource.id); // sorta unsafe
      BufferResource& buffer = context_->buffers_.at(resource.slot);

      if (new_state != buffer.state) {
        VkBufferMemoryBarrier2& barrier =
            buffer_barriers.emplace_back(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, nullptr);
        barrier.srcStageMask = buffer.state.stage;
        barrier.srcAccessMask = buffer.state.access;
        barrier.dstStageMask = new_state.stage;
        barrier.dstAccessMask = new_state.access;
        barrier.buffer = context_->raw_buffers_.at(resource.raw);
        buffer.state = new_state;
      }
    }
    dep_info.bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size());
    dep_info.pBufferMemoryBarriers = buffer_barriers.data();
  }

  // -------------------
  // Create end barriers
  // -------------------
  end_dep_info_.dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  end_dep_info_.dep_info.dependencyFlags = 0u;

  for (const auto& [id, state]: end_image_states_) {
    const Resource& resource = context_->resources_[*id.id];
    ImageResource& image = context_->images_[resource.slot];

    end_dep_info_.image_barriers.push_back(
        {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
         .pNext = nullptr,
         .srcStageMask = image.state.stage,
         .srcAccessMask = image.state.access,
         .dstStageMask = state.stage,
         .dstAccessMask = state.access,
         .oldLayout = image.state.layout,
         .newLayout = state.layout,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .image = context_->raw_images_[resource.raw],
         .subresourceRange = {
             .aspectMask = image.aspect, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}});
    image.state = state;
  }

  for (const auto& [id, state]: end_buffer_states_) {
    const Resource& resource = context_->resources_[*id.id];
    BufferResource& buffer = context_->buffers_[resource.slot];

    end_dep_info_.buffer_barriers.push_back({.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                             .pNext = nullptr,
                                             .srcStageMask = buffer.state.stage,
                                             .srcAccessMask = buffer.state.access,
                                             .dstStageMask = state.stage,
                                             .dstAccessMask = state.access,
                                             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                             .buffer = context_->raw_buffers_[resource.raw],
                                             .offset = 0,
                                             .size = buffer.size});
    buffer.state = state;
  }

  end_dep_info_.dep_info.bufferMemoryBarrierCount = static_cast<uint32_t>(end_dep_info_.buffer_barriers.size());
  end_dep_info_.dep_info.pBufferMemoryBarriers = end_dep_info_.buffer_barriers.data();
  end_dep_info_.dep_info.imageMemoryBarrierCount = static_cast<uint32_t>(end_dep_info_.image_barriers.size());
  end_dep_info_.dep_info.pImageMemoryBarriers = end_dep_info_.image_barriers.data();

  return true;
}

void passgraph::Graph::execute(VkCommandBuffer cmd) const
{
  if (!cmd) return;
  for (const uint32_t pass_id: sorted_pass_ids_) {
    auto& pass = passes_[pass_id];
    const auto& dep_info = pass_dep_infos_[pass_id];
    vkCmdPipelineBarrier2(cmd, &dep_info.dep_info);
    // if (pass.rendering) vkCmdBeginRendering(cmd, pass.render_info);
    pass.func(cmd);
    // if (pass.rendering) vkCmdEndRendering(cmd);
  }
  vkCmdPipelineBarrier2(cmd, &end_dep_info_.dep_info);
}
