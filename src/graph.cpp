#include "graph.hpp"
#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include "context.hpp"
#include "types/pass.hpp"

void fwrk::Graph::set_image_end_state(const ResourceID resource, const ImageState& state)
{
  end_image_states_[resource] = state;
}

void fwrk::Graph::set_buffer_end_state(const ResourceID resource, const BufferState& state)
{
  end_buffer_states_[resource] = state;
}

fwrk::GraphicsPassBuilder fwrk::Graph::add_graphics_pass(std::string name)
{
  const auto id = passes_.size();
  passes_.emplace_back().name = std::move(name);
  return GraphicsPassBuilder{&passes_.back(), this, id};
}

bool fwrk::Graph::compile()
{
  // -------------------
  // Reset compiled data
  // -------------------
  sorted_pass_ids_.clear();
  end_dep_info_ = {};
  compiled_passes_.clear();

  size_t pass_count = passes_.size();

  // --------------
  // you like DAGs?
  // --------------
  std::vector<std::vector<uint32_t>> incoming{pass_count};
  std::vector<std::vector<uint32_t>> outcoming{pass_count};

  auto add_edge = [&incoming, &outcoming](const uint32_t from, const uint32_t to) {
    outcoming[from].push_back(to);
    incoming[to].push_back(from);
  };

  for (auto& [_, info]: resource_deps_) {
    std::ranges::sort(info.write_passes);
    std::ranges::sort(info.read_passes);
    const auto write_count = static_cast<uint32_t>(info.write_passes.size());
    const auto read_count = static_cast<uint32_t>(info.read_passes.size());

    std::optional<uint32_t> previous_write;
    std::vector<uint32_t> previous_readers;

    uint32_t write_idx = 0;
    uint32_t read_idx = 0;
    while (write_idx < write_count || read_idx < read_count) {
      // if we still have a write and a read and they happen in the same pass
      if (write_idx < write_count && read_idx < read_count &&
          info.write_passes[write_idx] == info.read_passes[read_idx]) {
        uint32_t write_pass = info.write_passes[write_idx];
        if (previous_write) {
          add_edge(*previous_write, write_pass);
        }
        for (uint32_t read: previous_readers) {
          add_edge(read, write_pass);
        }
        previous_write = write_pass;
        previous_readers.clear();
        write_idx++;
        read_idx++;
        // if we still have a write and either we have no more reads or the write comes earlier than the read
      } else if (write_idx < write_count &&
                 (read_idx >= read_count || info.write_passes[write_idx] < info.read_passes[read_idx])) {
        uint32_t write_pass = info.write_passes[write_idx];
        if (previous_write) {
          add_edge(*previous_write, write_pass);
        }
        for (uint32_t read: previous_readers) {
          add_edge(read, write_pass);
        }
        previous_write = write_pass;
        previous_readers.clear();
        write_idx++;
        // if we still have a read but no more writes
      } else {
        uint32_t read_pass = info.read_passes[read_idx];

        auto it = info.read_deps.find(read_pass);
        if (it != info.read_deps.end()) {
          uint32_t other = it->second;
          assert(std::ranges::binary_search(info.write_passes, other));
          add_edge(other, read_pass);
          for (uint32_t write: info.write_passes) {
            if (write > other && write < read_pass) { // loop over all other writers between this read and the explicit
                                                      // dep write and add a reverse edge
              add_edge(read_pass, write);
            }
          }
        } else if (previous_write) {
          add_edge(*previous_write, read_pass);
        }

        previous_readers.push_back(read_pass);
        read_idx++;
      }
    }
  }
  for (auto& in: incoming) {
    std::ranges::sort(in);
    auto [first, last] = std::ranges::unique(in);
    in.erase(first, last);
  }
  for (auto& out: outcoming) {
    std::ranges::sort(out);
    auto [first, last] = std::ranges::unique(out);
    out.erase(first, last);
  }

  // ----------------
  // Topological sort
  // ----------------
  {
    sorted_pass_ids_.reserve(pass_count);

    std::vector<uint32_t> in_deg(pass_count);
    for (uint32_t i = 0; i < pass_count; i++) {
      in_deg[i] = static_cast<uint32_t>(incoming[i].size());
    }

    // find all nodes with no incoming edges
    std::deque<uint32_t> root_nodes;
    for (uint32_t i = 0; i < incoming.size(); i++) {
      if (incoming[i].empty()) {
        root_nodes.push_back(i);
      }
    }

    // sort
    while (!root_nodes.empty()) {
      auto node = root_nodes.front();
      root_nodes.pop_front();
      sorted_pass_ids_.push_back(node);

      for (auto it: outcoming[node]) {
        in_deg[it]--;

        // recurse
        if (in_deg[it] == 0) {
          root_nodes.push_back(it);
        }
      }
    }
  }

  assert(sorted_pass_ids_.size() == passes_.size() && "You created a cycle");

  // ----------------------------------------
  // Create pass barriers and rendering infos
  // ----------------------------------------
  compiled_passes_.resize(pass_count);

  for (const uint32_t pass_id: sorted_pass_ids_) {
    Pass& pass = passes_[pass_id];
    auto& [dependencies, rendering, name, func] = compiled_passes_[pass_id];
    auto& [dep_info, image_barriers, buffer_barriers] = dependencies;
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.dependencyFlags = 0u;

    name = std::move(pass.name);
    func = std::move(pass.func);

    uint32_t max_width = 0, max_height = 0;

    // image memory barriers
    for (const ImageAccess& image_access: pass.images) {
      const ImageState new_state{
          .access = image_access.access, .stage = image_access.stages, .layout = image_access.layout};
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
        barrier.subresourceRange.baseArrayLayer = image_access.layer;
        barrier.subresourceRange.baseMipLevel = image_access.level;
        barrier.subresourceRange.layerCount = image.layer_count;
        barrier.subresourceRange.levelCount = image.level_count;
        image.state = new_state;
      }

      // rendering attachment
      if (image_access.attachment) {
        if (!rendering.has_value()) {
          rendering.emplace();
        }
        auto& rendering_info = *rendering;
        const auto& [load_op, store_op, clear_value, is_depth, resolve, resolve_mode] = *image_access.attachment;
        VkRenderingAttachmentInfo attachment_info{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .pNext = nullptr,
                                                  .imageView = context_->get_image_view(image_access, resource),
                                                  .imageLayout = image.state.layout,
                                                  .resolveMode = VK_RESOLVE_MODE_NONE,
                                                  .resolveImageView = VK_NULL_HANDLE,
                                                  .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                  .loadOp = load_op,
                                                  .storeOp = store_op,
                                                  .clearValue = {}};
        if (is_depth) {
          attachment_info.clearValue.depthStencil.depth = clear_value.r;
          attachment_info.clearValue.depthStencil.stencil = static_cast<uint32_t>(clear_value.r);
          rendering_info.depth_info = attachment_info;
        } else {
          attachment_info.clearValue.color.float32[0] = clear_value.r;
          attachment_info.clearValue.color.float32[1] = clear_value.g;
          attachment_info.clearValue.color.float32[2] = clear_value.b;
          attachment_info.clearValue.color.float32[3] = clear_value.a;
          rendering_info.attachment_infos.push_back(attachment_info);
        }

        if (resolve) {
          const Resource& resolve_resource = context_->resources_.at(*resolve->id);
          attachment_info.resolveImageView = context_->get_image_view(image_access, resolve_resource);
          attachment_info.resolveMode = static_cast<VkResolveModeFlagBits>(resolve_mode);
          attachment_info.resolveImageLayout =
              is_depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        max_width = std::max(max_width, image.x);
        max_height = std::max(max_height, image.y);
      }
    }
    dep_info.imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size());
    dep_info.pImageMemoryBarriers = image_barriers.data();

    // rendering info
    if (rendering) {
      auto& [rendering_info, attachment_infos, depth_info] = *rendering;
      rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
      rendering_info.layerCount = 1;
      rendering_info.renderArea.extent = pass.render_area
                                             ? VkExtent2D{pass.render_area->width, pass.render_area->height}
                                             : VkExtent2D{max_width, max_height};
      rendering_info.colorAttachmentCount = static_cast<uint32_t>(attachment_infos.size());
      rendering_info.pColorAttachments = attachment_infos.data();
      if (depth_info) {
        rendering_info.pDepthAttachment = &*depth_info;
      }
    }

    // buffer memory barriers
    for (const BufferAccess& buffer_access: pass.buffers) {
      const BufferState new_state{.access = buffer_access.access, .stage = buffer_access.stages};
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
        barrier.size = buffer_access.size;
        barrier.offset = buffer_access.offset;
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

void fwrk::Graph::execute(VkCommandBuffer cmd)
{
  if (!cmd) return;
  for (const uint32_t pass_id: sorted_pass_ids_) {
    auto& compiled_pass = compiled_passes_[pass_id];
    vkCmdPipelineBarrier2(cmd, &compiled_pass.deps.dep_info);
    if (compiled_pass.render) vkCmdBeginRendering(cmd, &compiled_pass.render->rendering_info);
    compiled_pass.func(cmd);
    if (compiled_pass.render) vkCmdEndRendering(cmd);
  }
  vkCmdPipelineBarrier2(cmd, &end_dep_info_.dep_info);

  // -------------------
  // Reset supplied data
  // -------------------
  passes_.clear();
  resource_deps_.clear();
  end_image_states_.clear();
  end_buffer_states_.clear();
}
