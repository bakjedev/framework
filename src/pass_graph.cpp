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
  std::vector<std::pair<std::unordered_set<uint32_t>, std::unordered_set<uint32_t>>> dag{passes_.size() /*2*/};
  // first = incoming, second = outgoing

  // dag[0].second.insert(1);
  // dag[1].first.insert(0);


  for (const auto& resource: resources_) {
    std::vector<uint32_t> sorted_writes{resource.write_passes.begin(), resource.write_passes.end()};
    std::ranges::sort(sorted_writes);
    const auto write_count = static_cast<uint32_t>(sorted_writes.size());

    std::vector<uint32_t> sorted_reads{resource.read_passes.begin(), resource.read_passes.end()};
    std::ranges::sort(sorted_reads);
    const auto read_count = static_cast<uint32_t>(sorted_reads.size());

    // the last pass we accessed the resource and if it was a write or not
    std::optional<uint32_t> previous_access;
    bool previous_write = false;

    uint32_t write_idx = 0;
    uint32_t read_idx = 0;
    while (write_idx < write_count || read_idx < read_count) {
      // what pass were accessing and if its a write or not
      uint32_t current_access;
      bool current_write = false;

      // check if we still have either a write or a read and if so advance the smallest one
      if (write_idx < write_count && read_idx < read_count) {
        if (sorted_writes[write_idx] < sorted_reads[read_idx]) {
          current_access = sorted_writes[write_idx];
          current_write = true;
          write_idx++;
        } else {
          current_access = sorted_reads[read_idx];
          read_idx++;
        }
      } else if (write_idx < write_count) {
        current_access = sorted_writes[write_idx];
        current_write = true;
        write_idx++;
      } else {
        current_access = sorted_reads[read_idx];
        read_idx++;
      }

      if (previous_access.has_value() && *previous_access != current_access) {
        if (previous_write || current_write) {
          dag[current_access].first.insert(*previous_access);
          dag[*previous_access].second.insert(current_access);
          std::cout << "Inserted edge\n";
        }

        if (previous_write && current_write) {
          std::cout << "WAW\n";
        } else if (previous_write && !current_write) {
          std::cout << "RAW\n";
        } else if (!previous_write && current_write) {
          std::cout << "WAR\n";
        } else if (!previous_write && !current_write) {
          std::cout << "RAR - don't care\n";
        }
      }

      previous_access = current_access;
      previous_write = current_write;
    }
  }

  // ----------------
  // Topological sort
  // ----------------
  std::vector<uint32_t> sorted_passes;
  std::unordered_set<uint32_t> root_nodes;

  // find all nodes with no incoming edges
  for (uint32_t i = 0; i < dag.size(); i++) {
    if (dag[i].first.empty()) {
      root_nodes.insert(i);
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
