#include "pass_builder.hpp"

#include <cassert>

#include "graph.hpp"
#include "types/pass.hpp"

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_color_attachment(const AttachmentInfo& info)
{
  if (!try_access(info.resource.id)) return *this;
  auto& res = graph_->resource_deps_[info.resource.id];

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value,
                 .is_depth = false},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && !res.write_passes.empty()) {
    const auto [_, inserted] = res.read_passes.insert(id_);
    assert(inserted);
    if (info.resource.pass) {
      res.read_deps[id_] = *info.resource.pass;
    }
    image.access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  }

  res.write_passes.insert(id_);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_depth_attachment(const AttachmentInfo& info)
{
  if (!try_access(info.resource.id)) return *this;
  auto& res = graph_->resource_deps_[info.resource.id];

  ImageAccess& image = pass_->images.emplace_back(
      info.resource.id,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value,
                 .is_depth = true},
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && !res.write_passes.empty()) {
    const auto [_, inserted] = res.read_passes.insert(id_);
    assert(inserted);
    if (info.resource.pass) {
      res.read_deps[id_] = *info.resource.pass;
    }

    image.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  }

  res.write_passes.insert(id_);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_resolve_attachment(
    const ResourceID resolve_resource, const ResourceID color_resource, const VkResolveModeFlags mode)
{
  if (!try_access(resolve_resource)) return *this;
  auto& res = graph_->resource_deps_[resolve_resource];
  for (auto& image_access: pass_->images) {
    if (image_access.resource == color_resource) {
      if (image_access.attachment.has_value()) {
        image_access.attachment->resolve = resolve_resource;
        image_access.attachment->resolve_mode = mode;

        res.write_passes.insert(id_);

        break;
      }
    }
  }
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_vertex_buffer_input(const ResourceAccess& resource)
{
  set_buffer_read(resource, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_index_buffer_input(const ResourceAccess& resource)
{
  set_buffer_read(resource, VK_ACCESS_2_INDEX_READ_BIT, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_indirect_buffer_input(
    const ResourceAccess& resource)
{
  set_buffer_read(resource, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_render_area(const RenderArea area)
{
  pass_->render_area = area;
  return *this;
}

template<typename T>
T& passgraph::PassBuilder<T>::set_image_read(const ResourceAccess& resource, const VkPipelineStageFlags2 stages)
{
  if (!try_access(resource.id)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[resource.id];
  res.read_passes.insert(id_);
  if (resource.pass) {
    res.read_deps[id_] = *resource.pass;
  }

  ImageAccess& image = pass_->images.emplace_back(resource.id, std::nullopt, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                                  stages, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  set_stages_fallback(image.stages);

  return static_cast<T&>(*this);
}

template<typename T>
T& passgraph::PassBuilder<T>::set_uniform_buffer_read(const ResourceAccess& resource,
                                                      const VkPipelineStageFlags2 stages)
{
  set_buffer_read(resource, VK_ACCESS_2_UNIFORM_READ_BIT, stages);
  return static_cast<T&>(*this);
}

template<typename T>
T& passgraph::PassBuilder<T>::set_storage_buffer_read(const ResourceAccess& resource, VkPipelineStageFlags2 stages)
{
  set_buffer_read(resource, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, stages);
  return static_cast<T&>(*this);
}

template<typename T>
T& passgraph::PassBuilder<T>::set_storage_buffer_write(const ResourceAccess& resource, VkPipelineStageFlags2 stages)
{
  if (!try_access(resource.id)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[resource.id];

  BufferAccess& buffer = pass_->buffers.emplace_back(resource.id, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, stages);

  set_stages_fallback(buffer.stages);

  set_possible_read(resource, res, buffer.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

  res.write_passes.insert(id_);
  return static_cast<T&>(*this);
}

template<typename T>
T& passgraph::PassBuilder<T>::set_storage_image_read(const ResourceAccess& resource, VkPipelineStageFlags2 stages)
{
  if (!try_access(resource.id)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[resource.id];
  res.read_passes.insert(id_);
  if (resource.pass) {
    res.read_deps[id_] = *resource.pass;
  }

  ImageAccess& image = pass_->images.emplace_back(resource.id, std::nullopt, VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                                                  stages, VK_IMAGE_LAYOUT_GENERAL);

  set_stages_fallback(image.stages);

  return static_cast<T&>(*this);
}

template<typename T>
T& passgraph::PassBuilder<T>::set_storage_image_write(const ResourceAccess& resource, VkPipelineStageFlags2 stages)
{
  if (!try_access(resource.id)) return static_cast<T&>(*this);
  auto& res = graph_->resource_deps_[resource.id];

  ImageAccess& image = pass_->images.emplace_back(resource.id, std::nullopt, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                                  stages, VK_IMAGE_LAYOUT_GENERAL);

  set_stages_fallback(image.stages);

  set_possible_read(resource, res, image.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

  res.write_passes.insert(id_);
  return static_cast<T&>(*this);
}

template<typename T>
T& passgraph::PassBuilder<T>::set_execute(std::function<void(VkCommandBuffer)> func)
{
  pass_->func = std::move(func);
  return static_cast<T&>(*this);
}

template<typename T>
void passgraph::PassBuilder<T>::set_buffer_read(const ResourceAccess& resource, VkAccessFlags2 access,
                                                const VkPipelineStageFlags2 stages)
{
  if (accessed_.contains(resource.id)) return;
  accessed_.insert(resource.id);
  auto& res = graph_->resource_deps_[resource.id];
  res.read_passes.insert(id_);
  if (resource.pass) {
    res.read_deps[id_] = *resource.pass;
  }

  BufferAccess& buffer = pass_->buffers.emplace_back(resource.id, access, stages);

  set_stages_fallback(buffer.stages);
}

template<typename T>
void passgraph::PassBuilder<T>::set_stages_fallback(VkPipelineStageFlags2& stages) const
{
  if (stages == VK_PIPELINE_STAGE_2_NONE) {
    if constexpr (std::is_same_v<T, GraphicsPassBuilder>) {
      stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
  }
}

template<typename T>
void passgraph::PassBuilder<T>::set_possible_read(const ResourceAccess& resource, ResourceDependencies& deps,
                                                  VkAccessFlags2& access, const VkAccessFlags2 access_flags) const
{
  if (!deps.write_passes.empty()) {
    deps.read_passes.insert(id_);
    if (resource.pass) {
      deps.read_deps[id_] = *resource.pass;
    }
    access |= access_flags;
  }
}

template<typename T>
bool passgraph::PassBuilder<T>::try_access(const ResourceID resource)
{
  if (accessed_.contains(resource)) return false;
  accessed_.insert(resource);
  return true;
}

template class passgraph::PassBuilder<passgraph::GraphicsPassBuilder>;
