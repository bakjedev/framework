#include "pass_graph.hpp"
#include <iostream>
#include "types/pass.hpp"

passgraph::ResourceID passgraph::Graph::import_image(std::string name, const ImageResource &image, VkImage raw) {
  if (raw == VK_NULL_HANDLE) {
    //return ResourceID{};
  }

  const auto slot_id = images_.size();
  images_.push_back(image);

  const auto raw_id = raw_images_.size();
  raw_images_.push_back(raw);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Image, slot_id, raw_id, std::move(name));

  return ResourceID{id};
}

passgraph::ResourceID passgraph::Graph::import_buffer(std::string name, const BufferResource &buffer, VkBuffer raw) {
  if (raw == VK_NULL_HANDLE) {
    //return ResourceID{};
  }

  const auto slot_id = buffers_.size();
  buffers_.push_back(buffer);

  const auto raw_id = raw_buffers_.size();
  raw_buffers_.push_back(raw);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Buffer, slot_id, raw_id, std::move(name));

  return ResourceID{id};
}

passgraph::PassBuilder passgraph::Graph::add_pass(std::string name) {
  const auto id = passes_.size();
  passes_.emplace_back().name = std::move(name);
  return PassBuilder{&passes_.back(), this, id};
}

bool passgraph::Graph::compile() const {
  return true;
}

void passgraph::Graph::execute() const {
  for (const Pass &pass: passes_) {
    // insert pass.barrier whatever

    // // debug log
    // std::cout << pass.name << " start\n";
    // std::cout << "accesses:\n";
    // for (const auto &res: pass.accesses) {
    //   const auto &resource = resources_[res.id.id];
    //   std::cout << resource.name << " (" << (resource.type == ResourceType::Buffer ? "buffer" : "image") << ", " <<
    //       res.pass << "):\n" << "passes it gets written: ";
    //   for (const auto &pass_id: resource.write_passes) {
    //     std::cout << pass_id << " ";
    //   }
    //   std::cout << "\npasses its gets read in: ";
    //   for (const auto &pass_id: resource.read_passes) {
    //     std::cout << pass_id << " ";
    //   }
    //   std::cout << "\n\n";
    // }
    // std::cout << "executing\n";
    pass.func();
    // std::cout << pass.name << " end\n\n\n";
  }
}
