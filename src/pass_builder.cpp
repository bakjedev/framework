#include "framework/pass_builder.hpp"

#include <cassert>

#include "framework/graph.hpp"
#include "framework/types/pass.hpp"

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_color_attachment(const AttachmentInfo& info)
{
  if (!try_access_image(info)) return *this;
  auto& res = graph_->resource_deps_[info.resource.id];

  VkImageAspectFlags aspect = info.aspect;
  if (info.aspect == VK_IMAGE_ASPECT_NONE) {
    aspect |= VK_IMAGE_ASPECT_COLOR_BIT;
  }

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value},
      aspect, info.base_level, 1, info.base_layer, 1, info.view_type, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && !res.write_passes.empty()) {
    res.read_passes.push_back(id_);
    set_possible_explicit_read(info.resource.pass, res);

    image.access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  }

  res.write_passes.push_back(id_);

  if (info.resolve) {
    if (!try_access_image(info, true)) return *this;
    auto& res_olve = graph_->resource_deps_[info.resolve->resource];

    const auto idx = static_cast<uint32_t>(pass_->images.size());
    pass_->images.emplace_back(info.resolve->resource, std::nullopt, aspect, info.resolve->base_level, 1,
                               info.resolve->base_layer, 1, info.view_type, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    res_olve.write_passes.push_back(id_);

    image.attachment->resolve = idx;
    image.attachment->resolve_mode = info.resolve->mode;
  }

  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_depth_attachment(const AttachmentInfo& info)
{
  if (!try_access_image(info)) return *this;
  auto& res = graph_->resource_deps_[info.resource.id];

  VkImageAspectFlags aspect = info.aspect;
  if (info.aspect == VK_IMAGE_ASPECT_NONE) {
    aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value},
      aspect, info.base_level, 1, info.base_layer, 1, info.view_type, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && !res.write_passes.empty()) {
    res.read_passes.push_back(id_);
    set_possible_explicit_read(info.resource.pass, res);

    image.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  }

  res.write_passes.push_back(id_);

  if (info.resolve) {
    if (!try_access_image(info, true)) return *this;
    auto& res_olve = graph_->resource_deps_[info.resolve->resource];

    const auto idx = static_cast<uint32_t>(pass_->images.size());
    pass_->images.emplace_back(
        info.resolve->resource, std::nullopt, aspect, info.resolve->base_level, 1, info.resolve->base_layer, 1,
        info.view_type, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    res_olve.write_passes.push_back(id_);

    image.attachment->resolve = idx;
    image.attachment->resolve_mode = info.resolve->mode;
  }
  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_vertex_buffer_input(const BufferInfo& info)
{
  set_buffer_read(info, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);
  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_index_buffer_input(const BufferInfo& info)
{
  set_buffer_read(info, VK_ACCESS_2_INDEX_READ_BIT);
  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_indirect_buffer_input(const BufferInfo& info)
{
  if (!try_access_buffer(info.resource.id, info.offset, info.size)) return *this;
  auto& res = graph_->resource_deps_[info.resource.id];
  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  BufferAccess& buffer = pass_->buffers.emplace_back(info.resource.id, info.size, info.offset,
                                                     VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, info.stages);

  if (buffer.stages == VK_PIPELINE_STAGE_2_NONE) {
    buffer.stages = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }

  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_render_area(const VkExtent2D area)
{
  pass_->render_info.render_area = area;
  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_render_view(const uint32_t layer_count,
                                                                      const uint32_t view_mask)
{
  pass_->render_info.layer_count = layer_count;
  pass_->render_info.view_mask = view_mask;
  return *this;
}

template<typename T>
T& fwrk::PassBuilder<T>::set_image_read(const ImageInfo& info)
{
  if (!try_access_image(info)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];
  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id, std::nullopt, info.aspect, info.base_level, info.level_count, info.base_layer, info.layer_count,
      info.view_type, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, info.stages, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  set_stages_fallback(image.stages);

  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_uniform_buffer_read(const BufferInfo& info)
{
  set_buffer_read(info, VK_ACCESS_2_UNIFORM_READ_BIT);
  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_storage_buffer_read(const BufferInfo& info)
{
  set_buffer_read(info, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_storage_buffer_write(const BufferInfo& info)
{
  if (!try_access_buffer(info.resource.id, info.offset, info.size)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];

  BufferAccess& buffer = pass_->buffers.emplace_back(info.resource.id, info.size, info.offset,
                                                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, info.stages);

  set_stages_fallback(buffer.stages);

  set_possible_read(info.resource, res, buffer.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

  res.write_passes.push_back(id_);
  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_storage_image_read(const ImageInfo& info)
{
  if (!try_access_image(info)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];
  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id, std::nullopt, info.aspect, info.base_level, info.level_count, info.base_layer, info.layer_count,
      info.view_type, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, info.stages, VK_IMAGE_LAYOUT_GENERAL);

  set_stages_fallback(image.stages);

  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_storage_image_write(const ImageInfo& info)
{
  if (!try_access_image(info)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id, std::nullopt, info.aspect, info.base_level, info.level_count, info.base_layer, info.layer_count,
      info.view_type, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, info.stages, VK_IMAGE_LAYOUT_GENERAL);

  set_stages_fallback(image.stages);

  set_possible_read(info.resource, res, image.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

  res.write_passes.push_back(id_);
  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_image_transfer_src(const ImageInfo& info)
{
  if (!try_access_image(info)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];

  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id, std::nullopt, info.aspect, info.base_level, info.level_count, info.base_layer, info.layer_count,
      info.view_type, VK_ACCESS_2_TRANSFER_READ_BIT, info.stages, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  if (image.stages == VK_PIPELINE_STAGE_2_NONE) {
    image.stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  }

  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_image_transfer_dst(const ImageInfo& info)
{
  if (!try_access_image(info)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id, std::nullopt, info.aspect, info.base_level, info.level_count, info.base_layer, info.layer_count,
      info.view_type, VK_ACCESS_2_TRANSFER_WRITE_BIT, info.stages, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  if (image.stages == VK_PIPELINE_STAGE_2_NONE) {
    image.stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  }

  res.write_passes.push_back(id_);
  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_buffer_transfer_src(const BufferInfo& info)
{
  if (!try_access_buffer(info.resource.id, info.offset, info.size)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];

  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  BufferAccess& buffer =
      pass_->buffers.emplace_back(info.resource.id, info.size, info.offset, VK_ACCESS_2_TRANSFER_READ_BIT, info.stages);

  if (buffer.stages == VK_PIPELINE_STAGE_2_NONE) {
    buffer.stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  }

  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_buffer_transfer_dst(const BufferInfo& info)
{
  if (!try_access_buffer(info.resource.id, info.offset, info.size)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];

  BufferAccess& buffer = pass_->buffers.emplace_back(info.resource.id, info.size, info.offset,
                                                     VK_ACCESS_2_TRANSFER_WRITE_BIT, info.stages);

  if (buffer.stages == VK_PIPELINE_STAGE_2_NONE) {
    buffer.stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  }

  res.write_passes.push_back(id_);
  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_execute(std::function<void(VkCommandBuffer)> func)
{
  pass_->func = std::move(func);
  return static_cast<T&>(*this);
}

template<typename T>
void fwrk::PassBuilder<T>::set_buffer_read(const BufferInfo& info, const VkAccessFlags2 access) const
{
  if (!try_access_buffer(info.resource.id, info.offset, info.size)) return;
  auto& res = graph_->resource_deps_[info.resource.id];
  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  BufferAccess& buffer = pass_->buffers.emplace_back(info.resource.id, info.size, info.offset, access, info.stages);

  set_stages_fallback(buffer.stages);
}

template<typename T>
void fwrk::PassBuilder<T>::set_stages_fallback(VkPipelineStageFlags2& stages) const
{
  if (stages == VK_PIPELINE_STAGE_2_NONE) {
    if constexpr (std::is_same_v<T, GraphicsPassBuilder>) {
      stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    } else if constexpr (std::is_same_v<T, ComputePassBuilder>) {
      stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
  }
}

template<typename T>
void fwrk::PassBuilder<T>::set_possible_read(const ResourceAccess& resource, ResourceDependencies& deps,
                                             VkAccessFlags2& access, const VkAccessFlags2 access_flags) const
{
  if (!deps.write_passes.empty()) {
    deps.read_passes.push_back(id_);
    set_possible_explicit_read(resource.pass, deps);
    access |= access_flags;
  }
}

template<typename T>
bool fwrk::PassBuilder<T>::try_access_image(const ResourceID resource, const VkImageAspectFlags aspect,
                                            const uint32_t base_level, const uint32_t level_count,
                                            const uint32_t base_layer, const uint32_t layer_count) const
{
  for (const ImageAccess& other: pass_->images) {
    if (other.resource != resource) continue;
    if (other.aspect && aspect && !(other.aspect & aspect)) continue;
    if (!ranges_overlap(other.base_level, other.level_count, base_level, level_count)) continue;
    if (!ranges_overlap(other.base_layer, other.layer_count, base_layer, layer_count)) continue;
    return false;
  }
  return true;
}

template<typename T>
bool fwrk::PassBuilder<T>::try_access_image(const AttachmentInfo& info, const bool resolve) const
{
  return try_access_image(info.resource.id, info.aspect, resolve ? info.resolve->base_level : info.base_level, 1,
                          resolve ? info.resolve->base_layer : info.base_layer, 1);
}

template<typename T>
bool fwrk::PassBuilder<T>::try_access_image(const ImageInfo& info) const
{
  return try_access_image(info.resource.id, info.aspect, info.base_level, info.level_count, info.base_layer,
                          info.layer_count);
}

template<typename T>
bool fwrk::PassBuilder<T>::try_access_buffer(const ResourceID resource, const VkDeviceSize offset,
                                             const VkDeviceSize size) const
{
  for (const BufferAccess& other: pass_->buffers) {
    if (other.resource != resource) continue;
    if (ranges_overlap(other.offset, other.size, offset, size)) return false;
  }
  return true;
}

template<typename T>
void fwrk::PassBuilder<T>::set_possible_explicit_read(const std::optional<uint32_t> pass,
                                                      ResourceDependencies& deps) const
{
  if (pass) {
    assert(*pass < id_ && !deps.read_deps.contains(id_) && "Invalid or duplicate pass id for explicit read dep");
    deps.read_deps[id_] = *pass;
  }
}

template<typename T>
bool fwrk::PassBuilder<T>::ranges_overlap(const uint32_t x, const uint32_t x_count, const uint32_t y,
                                          const uint32_t y_count)
{
  return x < (y_count == ~0U ? UINT64_MAX : y + y_count) && y < (x_count == ~0U ? UINT64_MAX : x + x_count);
}

template<typename T>
bool fwrk::PassBuilder<T>::ranges_overlap(VkDeviceSize x, VkDeviceSize x_size, VkDeviceSize y, VkDeviceSize y_size)
{
  constexpr auto end = [](const VkDeviceSize offset, const VkDeviceSize size) {
    if (size == ~0ULL || size > UINT64_MAX - offset) return UINT64_MAX;
    return offset + size;
  };
  return x < end(y, y_size) && y < end(x, x_size);
}

template class fwrk::PassBuilder<fwrk::GraphicsPassBuilder>;
template class fwrk::PassBuilder<fwrk::ComputePassBuilder>;
