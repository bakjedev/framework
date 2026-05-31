#include "pass_builder.hpp"
#include "graph.hpp"
#include "types/pass.hpp"

passgraph::GraphicsPassBuilder::GraphicsPassBuilder(Pass* pass, Graph* graph, const size_t id) :
    pass_(pass), graph_(graph), id_(static_cast<uint32_t>(id))
{
}


passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_color_attachment(const AttachmentInfo& info)
{
  auto& res = graph_->resource_infos_[info.resource];
  res.write_passes.insert(id_);

  ImageAccess& image = pass_->images.emplace_back(
      info.resource, info.pass ? info.pass : res.last_writer,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value,
                 .is_depth = false},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && res.last_writer.has_value()) {
    res.read_passes.insert(id_);
    image.access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  }
  res.last_writer = id_;

  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_depth_attachment(const AttachmentInfo& info)
{
  auto& res = graph_->resource_infos_[info.resource];
  res.write_passes.insert(id_);

  ImageAccess& image = pass_->images.emplace_back(
      info.resource, info.pass ? info.pass : res.last_writer,
      Attachment{.load_op = static_cast<VkAttachmentLoadOp>(info.load_op),
                 .store_op = static_cast<VkAttachmentStoreOp>(info.store_op),
                 .clear_value = info.clear_value,
                 .is_depth = true},
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && res.last_writer.has_value()) {
    res.read_passes.insert(id_);
    image.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  }
  res.last_writer = id_;

  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::set_render_area(const RenderArea area)
{
  pass_->render_area = area;
  return *this;
}

passgraph::GraphicsPassBuilder& passgraph::GraphicsPassBuilder::execute(std::function<void(VkCommandBuffer)> func)
{
  pass_->func = std::move(func);
  return *this;
}
