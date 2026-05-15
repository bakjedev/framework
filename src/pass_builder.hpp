#pragma once
#include <cstdint>
#include <functional>

#include "types/pass.hpp"
#include "types/resource.hpp"

namespace passgraph
{
  class Graph;

  class PassBuilder
  {
  public:
    explicit PassBuilder(Pass *pass, Graph *graph, size_t id);

    PassBuilder &add_color_attachment(ResourceID resource);

    PassBuilder &add_color_attachment(ResourceID resource, uint32_t pass);

    PassBuilder &execute(std::function<void()> func);

    [[nodiscard]] uint32_t id() const { return id_; }

  private:
    Pass *pass_;
    Graph *graph_;
    uint32_t id_;
  };
} // namespace passgraph
