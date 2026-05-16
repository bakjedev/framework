#include <gtest/gtest.h>
#include "pass_graph.hpp"

TEST(Passgraph, SimpleTest)
{
  passgraph::Graph graph;

  const auto buf = graph.import_buffer("Data", {.size = 0, .usage = 0u}, nullptr);

  const auto img = graph.import_image("RenderTarget",
                                      {.x = 1920,
                                       .y = 1080,
                                       .z = 0,
                                       .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                       .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
                                      nullptr);

  EXPECT_TRUE(buf);
  EXPECT_TRUE(img);

  graph.add_pass("First").add_color_attachment(img).execute([] { std::cout << "A" << "\n"; });

  graph.add_pass("Second").add_color_attachment(img).execute([] { std::cout << "B" << "\n"; });

  EXPECT_TRUE(graph.compile());

  graph.execute();
}
