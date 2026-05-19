#include "pass_builder.hpp"
#include "pass_graph.hpp"
#include "types/pass.hpp"

passgraph::PassBuilder::PassBuilder(Pass* pass, Graph* graph, const size_t id) :
    pass_(pass), graph_(graph), id_(static_cast<uint32_t>(id))
{
}


passgraph::PassBuilder& passgraph::PassBuilder::add_color_attachment(const AttachmentInfo& info)
{
  auto& res = graph_->resources_.at(*info.resource.id);
  res.write_passes.insert(id_);

  ImageAccess& image = pass_->images.emplace_back(
      info.resource, info.pass ? info.pass : res.last_writer, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && res.last_writer.has_value()) {
    res.read_passes.insert(id_);
    image.access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  }
  res.last_writer = id_;

  return *this;
}

passgraph::PassBuilder& passgraph::PassBuilder::add_depth_attachment(const AttachmentInfo& info)
{
  auto& res = graph_->resources_.at(*info.resource.id);
  res.write_passes.insert(id_);

  ImageAccess& image = pass_->images.emplace_back(
      info.resource, info.pass ? info.pass : res.last_writer, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  if (info.load_op == LoadOp::Load && res.last_writer.has_value()) {
    res.read_passes.insert(id_);
    image.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  }
  res.last_writer = id_;

  return *this;
}

passgraph::PassBuilder& passgraph::PassBuilder::execute(std::function<void(VkCommandBuffer)> func)
{
  pass_->func = std::move(func);
  return *this;
}
