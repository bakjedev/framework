#include "pass_builder.hpp"

#include <cassert>

#include "graph.hpp"
#include "types/pass.hpp"

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_color_attachment(const AttachmentInfo& info)
{
  if (accessed_.contains(info.resource.id)) return *this;
  accessed_.insert(info.resource.id);
  auto& res = graph_->resource_infos_[info.resource.id];
  res.write_passes.insert(id_);

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

  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_depth_attachment(const AttachmentInfo& info)
{
  if (accessed_.contains(info.resource.id)) return *this;
  accessed_.insert(info.resource.id);
  auto& res = graph_->resource_infos_[info.resource.id];
  res.write_passes.insert(id_);

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

  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_vertex_buffer_input(const ResourceAccess& resource)
{
  set_buffer_input(resource, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_index_buffer_input(const ResourceAccess& resource)
{
  set_buffer_input(resource, VK_ACCESS_2_INDEX_READ_BIT, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_indirect_buffer_input(
    const ResourceAccess& resource)
{
  set_buffer_input(resource, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_render_area(const RenderArea area)
{
  pass_->render_area = area;
  return *this;
}

template<typename T>
T& passgraph::PassBuilder<T>::execute(std::function<void(VkCommandBuffer)> func)
{
  pass_->func = std::move(func);
  return static_cast<T&>(*this);
}

template<typename T>
void passgraph::PassBuilder<T>::set_buffer_input(const ResourceAccess& resource, VkAccessFlags2 access,
                                                 VkPipelineStageFlags2 stage)
{
  if (accessed_.contains(resource.id)) return;
  accessed_.insert(resource.id);
  auto& res = graph_->resource_infos_[resource.id];
  res.read_passes.insert(id_);
  if (resource.pass) {
    res.read_deps[id_] = *resource.pass;
  }

  pass_->buffers.emplace_back(resource.id, std::nullopt, access, stage);
}

template class passgraph::PassBuilder<passgraph::GraphicsPassBuilder>;
