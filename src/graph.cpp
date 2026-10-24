#include "framework/graph.hpp"
#include <algorithm>
#include <cassert>
#include <deque>
#include "framework/context.hpp"
#include "framework/types/pass.hpp"

void fwrk::BarrierBatch::clear()
{
  images.clear();
  buffers.clear();
}

void fwrk::BarrierBatch::record(VkCommandBuffer cmd) const
{
  if (images.empty() && buffers.empty()) return;
  const VkDependencyInfo info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                              .pNext = nullptr,
                              .dependencyFlags = 0U,
                              .memoryBarrierCount = 0,
                              .pMemoryBarriers = nullptr,
                              .bufferMemoryBarrierCount = static_cast<uint32_t>(buffers.size()),
                              .pBufferMemoryBarriers = buffers.data(),
                              .imageMemoryBarrierCount = static_cast<uint32_t>(images.size()),
                              .pImageMemoryBarriers = images.data()};
  vkCmdPipelineBarrier2(cmd, &info);
}

void fwrk::Rendering::clear()
{
  colors.clear();
  depth.reset();
}

void fwrk::Rendering::begin(VkCommandBuffer cmd) const { vkCmdBeginRendering(cmd, &info); }

fwrk::Graph::~Graph()
{
  for (const QueryPool& pool: query_pools_)
    if (pool.pool) vkDestroyQueryPool(context_->device_, pool.pool, nullptr);
}

void fwrk::Graph::set_image_end_state(const ResourceID resource, const PhysicalState& state)
{
  end_image_states_[resource] = state;
}

void fwrk::Graph::set_buffer_end_state(const ResourceID resource, const PhysicalState& state)
{
  end_buffer_states_[resource] = state;
}

fwrk::GraphicsPassBuilder fwrk::Graph::add_graphics_pass(std::string name)
{
  const auto id = passes_.size();
  passes_.emplace_back().name = std::move(name);
  return GraphicsPassBuilder{&passes_.back(), this, id};
}

fwrk::ComputePassBuilder fwrk::Graph::add_compute_pass(std::string name)
{
  const auto id = passes_.size();
  passes_.emplace_back().name = std::move(name);
  return ComputePassBuilder{&passes_.back(), this, id};
}

fwrk::ResourceID fwrk::Graph::create_image(const ImageCreateInfo& info, std::string name)
{
  const auto id = transient_infos_.size();
  transient_infos_.emplace_back(info, std::move(name));

  return ResourceID{ResourceType::Transient, id};
}

fwrk::ResourceID fwrk::Graph::create_buffer(const BufferCreateInfo& info, std::string name)
{
  const auto id = transient_infos_.size();
  transient_infos_.emplace_back(info, std::move(name));

  return ResourceID{ResourceType::Transient, id};
}

bool fwrk::Graph::compile()
{
  // -------------------
  // Reset compiled data
  // -------------------
  compiled_passes_.clear();
  sorted_pass_ids_.clear();

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
    info.write_passes.erase(std::ranges::unique(info.write_passes).begin(), info.write_passes.end());
    info.read_passes.erase(std::ranges::unique(info.read_passes).begin(), info.read_passes.end());
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
  compiled_passes_.reserve(pass_count);

  for (const uint32_t pass_id: sorted_pass_ids_) {
    Pass& pass = passes_[pass_id];
    auto& [dependencies, rendering, name, func] = compiled_passes_.emplace_back();
    auto& [image_barriers, buffer_barriers] = dependencies;

    name = std::move(pass.name);
    func = std::move(pass.func);

    // image memory barriers
    for (const ImageAccess& image_access: pass.images) {
      VkImageSubresourceRange subresource{.aspectMask = image_access.aspect,
                                          .baseMipLevel = image_access.base_level,
                                          .levelCount = image_access.level_count,
                                          .baseArrayLayer = image_access.base_layer,
                                          .layerCount = image_access.layer_count};

      image_barriers.emplace_back(image_access.resource,
                                  PhysicalState{image_access.access, image_access.stages, image_access.layout},
                                  subresource);

      // rendering attachment
      if (image_access.attachment) {
        if (!rendering.has_value()) {
          rendering.emplace();
          rendering->render_info = pass.render_info;
        }
        const auto& [load_op, store_op, clear_value, resolve, resolve_mode] = *image_access.attachment;

        RenderingAttachmentInfo info;
        info.resource = image_access.resource;
        info.subresource = subresource;
        info.view_type = image_access.view_type;
        info.layout = image_access.layout;
        info.load_op = load_op;
        info.store_op = store_op;
        if (resolve) {
          const ImageAccess& resolve_access = pass.images.at(*resolve);
          info.resolve.emplace();
          info.resolve->resource = resolve_access.resource;
          info.resolve->subresource.aspectMask = resolve_access.aspect;
          info.resolve->subresource.baseArrayLayer = resolve_access.base_layer;
          info.resolve->subresource.baseMipLevel = resolve_access.base_level;
          info.resolve->subresource.layerCount = resolve_access.layer_count;
          info.resolve->subresource.levelCount = resolve_access.level_count;
          info.resolve->mode = resolve_mode;
        }
        if ((image_access.aspect & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
          info.clear_value =
              VkClearValue{.color = {.float32 = {clear_value.r, clear_value.g, clear_value.b, clear_value.a}}};
          rendering->color_atts.push_back(info);
        } else if ((image_access.aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 ||
                   (image_access.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) {
          info.clear_value =
              VkClearValue{.depthStencil = {.depth = clear_value.r, .stencil = static_cast<uint32_t>(clear_value.r)}};
          rendering->depth_att = info;
        }
      }
    }

    // buffer memory barriers
    for (const BufferAccess& buffer_access: pass.buffers) {
      buffer_barriers.emplace_back(buffer_access.resource, PhysicalState{buffer_access.access, buffer_access.stages},
                                   buffer_access.size, buffer_access.offset);
    }
  }

  for (QueryPool& pool: query_pools_) pool.count = 0;
  pass_timings_.clear();

  context_->schedule_destroy_transients();
  context_->allocate_transients(std::move(transient_infos_));

  // -------------------
  // Reset supplied data
  // -------------------
  passes_.clear();
  resource_deps_.clear();
  transient_infos_.clear();
  return true;
}

void fwrk::Graph::execute(VkCommandBuffer cmd, const uint32_t frame_index)
{
  context_->destroy_transients(frame_index);

  if (!cmd) return;

  QueryPool* query_pool = nullptr;
  if (context_->timestamps_) {
    query_pool = &query_pools_[frame_index];
    get_timestamps(*query_pool);
    reset_timestamps(*query_pool, cmd, static_cast<uint32_t>(compiled_passes_.size() * 2));
  }

  uint32_t query = 0;
  for (const CompiledPass& pass: compiled_passes_) {
    // --------------------
    // Create Pass Barriers
    // --------------------
    barriers_.clear();
    create_pass_barriers(pass.deps);

    // ---------------------
    // Create Rendering Info
    // ---------------------
    if (pass.render) {
      rendering_.clear();
      create_rendering(*pass.render);
    }

    // -----------------------------------------
    // Begin Debug Labels and Starting Timestamp
    // -----------------------------------------
    if (context_->begin_debug_pfn_) begin_debug_label(cmd, pass.name);
    if (context_->timestamps_)
      vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, query_pool->pool, query++);

    // ------------
    // Execute Pass
    // ------------
    barriers_.record(cmd);

    if (pass.render) rendering_.begin(cmd);

    pass.func(cmd);

    if (pass.render) vkCmdEndRendering(cmd);

    // -------------------------------------
    // End Debug Labels and Ending Timestamp
    // -------------------------------------
    if (context_->timestamps_)
      vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, query_pool->pool, query++);
    if (context_->end_debug_pfn_) context_->end_debug_pfn_(cmd);
  }

  // -------------------
  // Create End Barriers
  // -------------------
  barriers_.clear();
  create_end_barriers();

  // --------------------
  // Execute End Barriers
  // --------------------
  barriers_.record(cmd);
}

fwrk::Graph::FrameTimings fwrk::Graph::get_timings() const
{
  uint64_t end = 0;
  for (const PassTimings& timing: pass_timings_) end = std::max(end, timing.end);
  return {.begin = 0, .end = end, .passes = pass_timings_};
}

fwrk::Graph::Graph(Context* context) : context_(context)
{
  // If no valid physical device was given it just won't pre-allocate the color attachments and timestamps won't work.
  if (context_->physical_device_ == VK_NULL_HANDLE) return;

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(context_->physical_device_, &properties);

  rendering_.colors.reserve(properties.limits.maxColorAttachments);

  if (context->timestamps_) {
    query_pools_.resize(context_->frames_in_flight_);
    timestamp_period_ = properties.limits.timestampPeriod;
  }
}

fwrk::Graph::ResolvedImage fwrk::Graph::resolve_image(const ResourceID id) const
{
  const ResourceID resolved = context_->resolve_proxy(id);
  const Resource& resource = context_->get_resource(resolved);
  assert(std::holds_alternative<Image>(resource.desc));
  return {std::get<Image>(resource.desc), context_->get_physical_image(resource.physical_id, resolved.type())};
}

fwrk::PhysicalBuffer& fwrk::Graph::resolve_buffer(const ResourceID id) const
{
  const ResourceID resolved = context_->resolve_proxy(id);
  const Resource& resource = context_->get_resource(resolved);
  assert(std::holds_alternative<Buffer>(resource.desc));
  return context_->get_physical_buffer(resource.physical_id, resolved.type());
}

void fwrk::Graph::create_pass_barriers(const DependencyInfo& deps)
{
  // Images
  for (const auto& img_barr: deps.image_barriers) {
    auto [image, physical] = resolve_image(img_barr.resource);

    VkImageSubresourceRange subresource_range = img_barr.subresource_range;
    if (subresource_range.aspectMask == VK_IMAGE_ASPECT_NONE)
      subresource_range.aspectMask = get_aspect_for_format(image.format);

    if (subresource_range.levelCount == VK_REMAINING_MIP_LEVELS)
      subresource_range.levelCount = image.mip_count - subresource_range.baseMipLevel;
    if (subresource_range.layerCount == VK_REMAINING_ARRAY_LAYERS)
      subresource_range.layerCount = image.layer_count - subresource_range.baseArrayLayer;

    transition_image(barriers_.images, physical, img_barr.dst_state, subresource_range);
  }

  // Buffers
  for (const auto& buf_barr: deps.buffer_barriers) {
    transition_buffer(barriers_.buffers, resolve_buffer(buf_barr.resource), buf_barr.dst_state, buf_barr.offset,
                      buf_barr.size);
  }
}

void fwrk::Graph::create_end_barriers()
{
  // Images
  for (const auto& [id, state]: end_image_states_) {
    const auto& [image, physical] = resolve_image(id);

    VkImageSubresourceRange subresource_range = {.aspectMask = get_aspect_for_format(image.format),
                                                 .baseMipLevel = 0,
                                                 .levelCount = image.mip_count,
                                                 .baseArrayLayer = 0,
                                                 .layerCount = image.layer_count};

    transition_image(barriers_.images, physical, state, subresource_range);
  }

  // Buffers
  for (const auto& [id, state]: end_buffer_states_) {
    transition_buffer(barriers_.buffers, resolve_buffer(id), state, 0, VK_WHOLE_SIZE);
  }
}

VkRenderingAttachmentInfo fwrk::Graph::create_attachment(const RenderingAttachmentInfo& att, VkExtent2D& extent) const
{
  auto [image, physical] = resolve_image(att.resource);

  VkRenderingAttachmentInfo info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = nullptr,
      .imageView = context_->get_image_view({att.subresource, att.view_type}, image, physical),
      .imageLayout = att.layout,
      .resolveMode = VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = att.load_op,
      .storeOp = att.store_op,
      .clearValue = att.clear_value};

  if (att.resolve) {
    auto [image_resolve, physical_resolve] = resolve_image(att.resolve->resource);
    info.resolveMode = static_cast<VkResolveModeFlagBits>(att.resolve->mode);
    info.resolveImageView =
        context_->get_image_view({att.resolve->subresource, att.view_type}, image_resolve, physical_resolve);
    info.resolveImageLayout = att.layout;
  }

  extent.width = std::min(extent.width, std::max(1u, image.size.width >> att.subresource.baseMipLevel));
  extent.height = std::min(extent.height, std::max(1u, image.size.height >> att.subresource.baseMipLevel));

  return info;
}

void fwrk::Graph::create_rendering(const RenderingInfo& info)
{
  VkExtent2D extent{.width = UINT32_MAX, .height = UINT32_MAX};

  for (const auto& att: info.color_atts) rendering_.colors.push_back(create_attachment(att, extent));

  if (info.depth_att) rendering_.depth = create_attachment(*info.depth_att, extent);

  rendering_.info = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                     .pNext = nullptr,
                     .flags = 0U,
                     .renderArea = {.offset = {}, .extent = info.render_info.render_area.value_or(extent)},
                     .layerCount = info.render_info.layer_count,
                     .viewMask = info.render_info.view_mask,
                     .colorAttachmentCount = static_cast<uint32_t>(rendering_.colors.size()),
                     .pColorAttachments = rendering_.colors.data(),
                     .pDepthAttachment = rendering_.depth ? &*rendering_.depth : nullptr,
                     .pStencilAttachment = nullptr};
}

void fwrk::Graph::transition_image(std::vector<VkImageMemoryBarrier2>& barriers, PhysicalImage& image,
                                   const PhysicalState& state, const VkImageSubresourceRange& subresource_range)
{
  constexpr auto make_barrier = [](VkImage handle, const PhysicalState& src, const PhysicalState& dst,
                                   const VkImageSubresourceRange& range) {
    return VkImageMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 .pNext = nullptr,
                                 .srcStageMask = src.stages,
                                 .srcAccessMask = src.access,
                                 .dstStageMask = dst.stages,
                                 .dstAccessMask = dst.access,
                                 .oldLayout = src.layout,
                                 .newLayout = dst.layout,
                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 .image = handle,
                                 .subresourceRange = range};
  };

  const bool full_range = subresource_range.baseMipLevel == 0 && subresource_range.levelCount == image.levels &&
                          subresource_range.baseArrayLayer == 0 && subresource_range.layerCount == image.layers;

  // if all layers/mips are the same layout
  if (image.is_state_uniform()) {
    // if were going over all layers
    if (full_range) {
      if (image.uniform_state != state)
        barriers.push_back(make_barrier(image.handle, image.uniform_state, state, subresource_range));
      image.uniform_state = state;
      return;
    }
    // if not, were going to non-uniform, so first fill the list with the uniform state
    image.states.assign(image.levels * image.layers, image.uniform_state);
  }

  // one barrier per mip
  for (uint32_t mip = subresource_range.baseMipLevel;
       mip < subresource_range.baseMipLevel + subresource_range.levelCount; ++mip) {
    const uint32_t layer_start = subresource_range.baseArrayLayer;
    const uint32_t layer_end = layer_start + subresource_range.layerCount;

    // group layers that need to transition
    uint32_t layer = layer_start;
    while (layer < layer_end) {
      const PhysicalState src = image.state_at(mip, layer);
      uint32_t end = layer + 1;
      while (end < layer_end && image.state_at(mip, end) == src) end++;
      if (src != state) {
        barriers.push_back(make_barrier(image.handle, src, state,
                                        {.aspectMask = subresource_range.aspectMask,
                                         .baseMipLevel = mip,
                                         .levelCount = 1,
                                         .baseArrayLayer = layer,
                                         .layerCount = end - layer}));
      }
      for (uint32_t i = layer; i < end; i++) image.state_at(mip, i) = state;
      layer = end;
    }
  }

  if (full_range) {
    image.uniform_state = state;
    image.states.clear();
  }
}

void fwrk::Graph::transition_buffer(std::vector<VkBufferMemoryBarrier2>& barriers, PhysicalBuffer& buffer,
                                    const PhysicalState& state, const VkDeviceSize offset, const VkDeviceSize size)
{
  if (buffer.state == state) return;
  barriers.push_back({.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                      .pNext = nullptr,
                      .srcStageMask = buffer.state.stages,
                      .srcAccessMask = buffer.state.access,
                      .dstStageMask = state.stages,
                      .dstAccessMask = state.access,
                      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                      .buffer = buffer.handle,
                      .offset = offset,
                      .size = size});
  buffer.state = state;
}

void fwrk::Graph::begin_debug_label(VkCommandBuffer cmd, const std::string& name) const
{
  const VkDebugUtilsLabelEXT debug = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = name.c_str(),
      .color = {1.0f, 1.0f, 1.0f, 1.0f},
  };
  context_->begin_debug_pfn_(cmd, &debug);
}

void fwrk::Graph::get_timestamps(const QueryPool& pool)
{
  if (pool.count == 0) return;
  query_results_.resize(pool.count);

  if (vkGetQueryPoolResults(context_->device_, pool.pool, 0, pool.count, query_results_.size() * sizeof(uint64_t),
                            query_results_.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) != VK_SUCCESS) {
    return;
  }

  const uint64_t start = query_results_[0];

  pass_timings_.resize(pool.count / 2);
  for (size_t i = 0; i < pass_timings_.size(); ++i) {
    pass_timings_[i] = {
        .name = compiled_passes_[i].name,
        .begin = static_cast<uint64_t>(static_cast<float>(query_results_[2 * i] - start) * timestamp_period_),
        .end = static_cast<uint64_t>(static_cast<float>(query_results_[2 * i + 1] - start) * timestamp_period_)};
  }
}

void fwrk::Graph::reset_timestamps(QueryPool& pool, VkCommandBuffer cmd, const uint32_t count) const
{
  if (count >= pool.capacity) {
    if (pool.pool) vkDestroyQueryPool(context_->device_, pool.pool, nullptr);
    const VkQueryPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0U,
                                     .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                     .queryCount = count,
                                     .pipelineStatistics = 0U};
    vkCreateQueryPool(context_->device_, &info, nullptr, &pool.pool);
    pool.capacity = count;
  }
  vkCmdResetQueryPool(cmd, pool.pool, 0, count);
  pool.count = count;
}

VkImageAspectFlags fwrk::Graph::get_aspect_for_format(const VkFormat format)
{
  switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:
      return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}
