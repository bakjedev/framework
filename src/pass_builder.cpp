#include "pass_builder.hpp"
#include "pass_graph.hpp"
#include "types/pass.hpp"

passgraph::PassBuilder::PassBuilder(Pass* pass, Graph* graph, const size_t id) :
    pass_(pass), graph_(graph), id_(static_cast<uint32_t>(id))
{
}


passgraph::PassBuilder& passgraph::PassBuilder::add_color_attachment(const ResourceID resource,
                                                                     const std::optional<uint32_t> pass)
{
  auto& res = graph_->resources_.at(*resource.id);
  res.write_passes.insert(id_);
  pass_->images.emplace_back(resource, pass ? pass : res.last_writer, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                             VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  res.last_writer = id_;

  return *this;
}

passgraph::PassBuilder& passgraph::PassBuilder::execute(std::function<void()> func)
{
  pass_->func = std::move(func);
  return *this;
}
