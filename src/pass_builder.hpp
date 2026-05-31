#pragma once
#include <functional>
#include "types/pass.hpp"
#include "types/resource.hpp"

namespace passgraph {
  class Graph;

  enum class LoadOp { Load = 0, Clear = 1, DontCare = 2 };
  enum class StoreOp { Store = 0, DontCare = 1 };

  struct AttachmentInfo {
    ResourceID resource;
    LoadOp load_op = LoadOp::DontCare;
    StoreOp store_op = StoreOp::DontCare;
    ClearValue clear_value{};
    std::optional<uint32_t> pass = std::nullopt;
  };

  class GraphicsPassBuilder {
  public:
    explicit GraphicsPassBuilder(Pass* pass, Graph* graph, size_t id);

    GraphicsPassBuilder& set_color_attachment(const AttachmentInfo& info);
    GraphicsPassBuilder& set_depth_attachment(const AttachmentInfo& info);

    GraphicsPassBuilder& set_render_area(RenderArea area);

    GraphicsPassBuilder& execute(std::function<void(VkCommandBuffer)> func);

    [[nodiscard]] uint32_t id() const { return id_; }

  private:
    Pass* pass_;
    Graph* graph_;
    uint32_t id_;
  };
} // namespace passgraph
