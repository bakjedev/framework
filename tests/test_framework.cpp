#include <gtest/gtest.h>
#include "context.hpp"

TEST(Framework, SimpleTest)
{
  fwrk::Context context{nullptr, 1};

  const auto buf = context.import_buffer({.size = 0, .state = fwrk::PhysicalState::Undefined}, nullptr, "Data");

  const auto img = context.import_image({.type = VK_IMAGE_TYPE_2D,
                                         .size = {1920, 1080, 1},
                                         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                         .state = fwrk::PhysicalState::Undefined},
                                        nullptr, "RenderTarget");

  EXPECT_TRUE(buf);
  EXPECT_TRUE(img);

  fwrk::Graph& graph = context.graph();

  const uint32_t first = graph.add_graphics_pass("First")
                             .set_color_attachment({.resource = {img}})
                             .set_execute([](VkCommandBuffer) { std::cout << "A" << "\n"; })
                             .id();

  graph.add_graphics_pass("Second").set_color_attachment({.resource = {img}}).set_execute([](VkCommandBuffer) {
    std::cout << "B" << "\n";
  });

  graph.add_graphics_pass("Third").set_image_read({{img, first}}).set_execute([](VkCommandBuffer) {
    std::cout << "C" << "\n";
  });

  EXPECT_TRUE(graph.compile());

  graph.execute(nullptr);
}
