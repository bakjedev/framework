#include "pass_builder.hpp"

#include <cassert>

#include "graph.hpp"
#include "types/pass.hpp"

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_color_attachment(const AttachmentInfo& info)
{
  if (!try_access(info.resource.id)) return *this;
  auto& res = graph_->resource_deps_[info.resource.id];

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value,
                 .is_depth = false},
      info.layer, info.level, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && !res.write_passes.empty()) {
    res.read_passes.push_back(id_);
    set_possible_explicit_read(info.resource.pass, res);

    image.access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  }

  res.write_passes.push_back(id_);
  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_depth_attachment(const AttachmentInfo& info)
{
  if (!try_access(info.resource.id)) return *this;
  auto& res = graph_->resource_deps_[info.resource.id];

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value,
                 .is_depth = true},
      info.layer, info.level, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && !res.write_passes.empty()) {
    res.read_passes.push_back(id_);
    set_possible_explicit_read(info.resource.pass, res);

    image.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  }

  res.write_passes.push_back(id_);
  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_resolve_attachment(const ResolveInfo& info)
{
  if (!try_access(info.resource)) return *this;
  auto& res = graph_->resource_deps_[info.resource];
  for (auto& image_access: pass_->images) {
    if (image_access.resource == info.color) {
      if (image_access.attachment.has_value()) {
        image_access.attachment->resolve = info.resource;
        image_access.attachment->resolve_mode = info.mode;

        res.write_passes.push_back(id_);

        break;
      }
    }
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
  set_buffer_read(info, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
  return *this;
}

fwrk::GraphicsPassBuilder& fwrk::GraphicsPassBuilder::set_render_area(const RenderArea area)
{
  pass_->render_area = area;
  return *this;
}

template<typename T>
T& fwrk::PassBuilder<T>::set_image_read(const ImageInfo& info)
{
  if (!try_access(info.resource.id)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];
  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  ImageAccess& image = pass_->images.emplace_back(info.resource.id, std::nullopt, info.layer, info.level,
                                                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, info.stages,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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
  if (!try_access(info.resource.id)) return static_cast<T&>(*this);
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
  if (!try_access(info.resource.id)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];
  res.read_passes.push_back(id_);
  set_possible_explicit_read(info.resource.pass, res);

  ImageAccess& image =
      pass_->images.emplace_back(info.resource.id, std::nullopt, info.layer, info.level,
                                 VK_ACCESS_2_SHADER_STORAGE_READ_BIT, info.stages, VK_IMAGE_LAYOUT_GENERAL);

  set_stages_fallback(image.stages);

  return static_cast<T&>(*this);
}

template<typename T>
T& fwrk::PassBuilder<T>::set_storage_image_write(const ImageInfo& info)
{
  if (!try_access(info.resource.id)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[info.resource.id];

  ImageAccess& image =
      pass_->images.emplace_back(info.resource.id, std::nullopt, info.layer, info.level,
                                 VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, info.stages, VK_IMAGE_LAYOUT_GENERAL);

  set_stages_fallback(image.stages);

  set_possible_read(info.resource, res, image.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

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
void fwrk::PassBuilder<T>::set_buffer_read(const BufferInfo& info, const VkAccessFlags2 access)
{
  if (accessed_.contains(info.resource.id)) return;
  accessed_.insert(info.resource.id);
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
bool fwrk::PassBuilder<T>::try_access(const ResourceID resource)
{
  if (accessed_.contains(resource)) return false;
  accessed_.insert(resource);
  return true;
}

template<typename T>
void fwrk::PassBuilder<T>::set_possible_explicit_read(std::optional<uint32_t> pass, ResourceDependencies& deps) const
{
  if (pass) {
    assert(*pass < id_);
    deps.read_deps[id_] = *pass;
  }
}

template class fwrk::PassBuilder<fwrk::GraphicsPassBuilder>;
