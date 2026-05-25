#include "pass_graph.hpp"
#include <algorithm>
#include <iostream>
#include "types/pass.hpp"

passgraph::ResourceID passgraph::Graph::import_image(const ImageResource& image, VkImage raw, std::string name)
{
  if (raw == VK_NULL_HANDLE) {
    // return ResourceID{};
  }

  const auto slot_id = images_.size();
  images_.push_back(image);

  const auto raw_id = raw_images_.size();
  raw_images_.push_back(raw);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Image, slot_id, raw_id, std::move(name));

  return ResourceID{id};
}

passgraph::ResourceID passgraph::Graph::import_buffer(const BufferResource& buffer, VkBuffer raw, std::string name)
{
  if (raw == VK_NULL_HANDLE) {
    // return ResourceID{};
  }

  const auto slot_id = buffers_.size();
  buffers_.push_back(buffer);

  const auto raw_id = raw_buffers_.size();
  raw_buffers_.push_back(raw);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Buffer, slot_id, raw_id, std::move(name));

  return ResourceID{id};
}

bool passgraph::Graph::update_image_raw(const ResourceID resource, VkImage raw)
{
  if (!resource.id.has_value()) {
    return false;
  }
  const uint32_t id = *resource.id;
  if (id >= resources_.size()) {
    return false;
  }

  raw_images_[resources_[id].raw] = raw; // sorta unsafe
  return true;
}

bool passgraph::Graph::update_buffer_raw(const ResourceID resource, VkBuffer raw)
{
  if (!resource.id.has_value()) {
    return false;
  }
  const uint32_t id = *resource.id;
  if (id >= resources_.size()) {
    return false;
  }

  raw_buffers_[resources_[id].raw] = raw; // sorta unsafe
  return true;
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
  std::vector<std::pair<std::unordered_set<uint32_t>, std::unordered_set<uint32_t>>> dag{passes_.size() /*2*/};
  // first = incoming, second = outgoing

  // dag[0].second.insert(1);
  // dag[1].first.insert(0);


  for (const auto& resource: resources_) {
    std::vector<uint32_t> sorted_writes{resource.write_passes.begin(), resource.write_passes.end()};
    std::ranges::sort(sorted_writes);
    const auto write_count = static_cast<uint32_t>(sorted_writes.size());

    std::vector<uint32_t> sorted_reads{resource.read_passes.begin(), resource.read_passes.end()};
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
        const bool rar = previous_read && current_read; // don't care

        if (raw || waw || war) {
          dag[current_access].first.insert(*previous_access);
          dag[*previous_access].second.insert(current_access);
          std::cout << "Inserted edge\n";
        }

        if (raw) {
          std::cout << "RAW ";
        }
        if (waw) {
          std::cout << "WAW ";
        }
        if (war) {
          std::cout << "WAR ";
        }
        if (rar) {
          std::cout << "RAR ";
        }
        std::cout << "\n";
      }

      previous_access = current_access;
      previous_write = current_write;
      previous_read = current_read;
    }
  }

  // ----------------
  // Topological sort
  // ----------------
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

  for (const auto& sorted_pass: sorted_passes) {
    std::cout << sorted_pass << "\n";
  }

  sorted_pass_ids_ = std::move(sorted_passes);
  pass_dep_infos_.clear();
  pass_dep_infos_.resize(passes_.size());

  for (const uint32_t pass_id: sorted_pass_ids_) {
    Pass& pass = passes_[pass_id];
    auto& [dep_info, image_barriers, buffer_barriers] = pass_dep_infos_[pass_id];

    for (const ImageAccess& image_access: pass.images) {
      const uint32_t resource_id = *image_access.resource.id; // unsafe
      const Resource& resource = resources_[resource_id];

      const ImageState new_state{
          .access = image_access.access, .stage = image_access.stage, .layout = image_access.layout};
      ImageState& old_state = images_[resource.slot].state;

      if (new_state != old_state) {
        VkImageMemoryBarrier2& barrier = image_barriers.emplace_back(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, nullptr);
        barrier.srcStageMask = old_state.stage;
        barrier.srcAccessMask = old_state.access;
        barrier.dstStageMask = new_state.stage;
        barrier.dstAccessMask = new_state.access;
        barrier.oldLayout = old_state.layout;
        barrier.newLayout = new_state.layout;
        barrier.image = raw_images_[resource.raw];
        old_state = new_state;
      }
    }
    dep_info.imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size());
    dep_info.pImageMemoryBarriers = image_barriers.data();

    for (const BufferAccess& buffer_access: pass.buffers) {
      const uint32_t resource_id = *buffer_access.resource.id; // unsafe
      const Resource& resource = resources_[resource_id];

      const BufferState new_state{.access = buffer_access.access, .stage = buffer_access.stage};
      BufferState& old_state = buffers_[resource.slot].state;

      if (new_state != old_state) {
        VkBufferMemoryBarrier2& barrier =
            buffer_barriers.emplace_back(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, nullptr);
        barrier.srcStageMask = old_state.stage;
        barrier.srcAccessMask = old_state.access;
        barrier.dstStageMask = new_state.stage;
        barrier.dstAccessMask = new_state.access;
        barrier.buffer = raw_buffers_[resource.raw];
        old_state = new_state;
      }
    }
    dep_info.bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size());
    dep_info.pBufferMemoryBarriers = buffer_barriers.data();
  }
  return true;
}

void passgraph::Graph::execute(VkCommandBuffer cmd) const
{
  for (const uint32_t pass_id: sorted_pass_ids_) {
    auto& pass = passes_[pass_id];
    [[maybe_unused]] const auto& dep_info = pass_dep_infos_[pass_id];
    if (cmd) vkCmdPipelineBarrier2(cmd, &dep_info.dep_info);
    pass.func(cmd);
  }
}
