#include "pass_graph.hpp"
#include <algorithm>
#include <iostream>
#include "types/pass.hpp"

passgraph::ResourceID passgraph::Graph::import_image(std::string name, const ImageResource& image, VkImage raw)
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

passgraph::ResourceID passgraph::Graph::import_buffer(std::string name, const BufferResource& buffer, VkBuffer raw)
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

passgraph::PassBuilder passgraph::Graph::add_pass(std::string name)
{
  const auto id = passes_.size();
  passes_.emplace_back().name = std::move(name);
  return PassBuilder{&passes_.back(), this, id};
}

bool passgraph::Graph::compile() const
{
  // --------------
  // you like DAGs?
  // --------------
  std::unordered_map<uint32_t, std::pair<std::unordered_set<uint32_t>, std::unordered_set<uint32_t>>> dag;
  dag[1].first.insert(0);
  dag[0].second.insert(1);

  dag[2].first.insert(1);

  dag[1].second.insert(2);


  // ----------------
  // Topological sort
  // ----------------
  std::vector<uint32_t> sorted_passes;
  std::unordered_set<uint32_t> root_nodes;

  // find all nodes with no incoming edges
  for (const auto& [node, edges]: dag) {
    if (edges.first.empty()) {
      root_nodes.insert(node);
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
  return true;
}

void passgraph::Graph::execute() const
{
  for (const Pass& pass: passes_) {
    pass.func();
  }
}
