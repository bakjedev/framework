# framework
A Vulkan render graph which leaves it up to you how you use it.

### Still quite WIP

## Usage
### API
The API of the library is split in two: A `Context` and a `Graph`. The `Context` handles things like importing/updating resources. The `Graph` handles things like the setup phase (adding passes), compiling the graph, and executing the graph. 

```cpp
fwrk::Context context{device};

... // imports/other configuring

fwrk::Graph& graph = context.graph();

... // setup phase

graph.compile();

graph.execute(cmd);
```

### Importing resources
You can import images and buffers in two ways.

Describing the image/buffer inline:

```cpp
const fwrk::ResourceID img_import = context.import_image({.type = VK_IMAGE_TYPE_2D,
                                                          .size = {1920, 1080, 1},
                                                          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                                          .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                          .state = fwrk::ImageState::Undefined},
                                                         image, "RenderTarget");

const fwrk::ResourceID buf_import =
    context.import_buffer({.size = 0, .usage = 0u, .state = fwrk::BufferState::Undefined}, buffer, "Data");
```

Using an interface which describes the image/buffer:

```cpp
struct ImageWrapper
{
  VkImageType type() const;
  VkExtent3D size() const;
  VkFormat format() const;
  VkImageUsageFlags usage() const;
  VkImage image() const;

  VulkanImage *image_; // Your image type
};

struct BufferWrapper
{
  VkDeviceSize size() const;
  VkImageUsageFlags usage() const;
  VkBuffer buffer() const;

  VulkanBuffer *buffer_; // Your buffer type
};

...

const ImageWrapper img_wrap(image);
const fwrk::ResourceID img_import = context.import_image(img_wrap, fwrk::ImageState::Undefined, "RenderTarget");

const BufferWrapper buf_wrap(buffer);
const fwrk::ResourceID buf_import = context.import_buffer(buf_wrap, fwrk::BufferState::Undefined, "Data");
```

These interfaces use C++ 20's concepts. If you want you could even make your own image/buffer type comply with the concept directly and you wouldn't need this wrapper.

These imported resources stay inside the context, and the context should live for the entire duration of your renderer's lifetime.

The names given here: (`"RenderTarget", "Data"`) are purely for debug purposes and can be left empty (defaults to `"Unnamed <buffer/image>"`)

### Updating imported resources
After you've imported the resources into the context you might later want to update them. Maybe the images were recreated with a different size, maybe you used the image outside the graph which made it transition to a different layout, and so on. The context needs to be synced with these external changes. To do so, you can update the images, again, in two ways.

Describing the updated image/buffer inline:

```cpp
context.update_image(img_import,
                         {.type = VK_IMAGE_TYPE_2D,
                          .size = {800, 600, 1},
                          .format = VK_FORMAT_B8G8R8A8_SRGB,
                          .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          .state = fwrk::ImageState::Undefined},
                         image);

context.update_buffer(buf_import, {.size = 0, .usage = 0u, .state = fwrk::BufferState::Undefined}, buffer);
```
{% endraw %}

Using an interface which describes the updated image/buffer:

```cpp
const ImageWrapper img_wrap(image);
context_->update_image(img_import, img_wrap, fwrk::ImageState::Undefined);

const BufferWrapper buf_wrap(buffer);
context_->update_buffer(buf_import, buf_wrap, fwrk::BufferState::Undefined);
```

### Proxies
The context also allows you to create and update something called a 'proxy'. A proxy is a resource which points to another resource. You can update a proxy at any time and it will resolve this proxy's pointer at graph execution, allowing you to swap out which resource a render graph pass uses without having to recompile the graph. This is useful for i.e. cycling swapchain images.

```cpp
const ImageWrapper img_a_wrap(image_a);
const fwrk::ResourceID img_a_import = context.import_image(img_a_wrap, fwrk::ImageState::Undefined, "RenderTarget");

const ImageWrapper img_b_wrap(image_b);
const fwrk::ResourceID img_b_import = context.import_image(img_b_wrap, fwrk::ImageState::Undefined, "RenderTarget");

const fwrk::ResourceID img_proxy = context.create_proxy(img_a_import, "image proxy"); // can also be left empty, proxy will point to nothing

context.update_proxy(img_proxy, img_b_import);
```

Similar to imports, the names given here are purely for debug purposes and can be left empty (defaults to `"Unnamed proxy"`)

### Passes
From the graph you can create passes:

```cpp
graph.add_graphics_pass("RenderPass").set_execute([](VkCommandBuffer) { std::cout << "Hello, graphics world!\n"; });

graph.add_compute_pass("ComputePass").set_execute([](VkCommandBuffer) { std::cout << "Hello, compute world!\n"; });
```

The builder pattern that this gives you has many functions.
```cpp
graph.add_graphics_pass("render mesh vis")
      .set_color_attachment({.resource = {vis_proxy_},
                             .load_op = fwrk::LoadOp::Clear,
                             .store_op = fwrk::StoreOp::Store,
                             .clear_value = {1.0F, 1.0F, 1.0f, 1.0F}})
      .set_depth_attachment({.resource = {depth_proxy_},
                             .load_op = fwrk::LoadOp::Clear,
                             .store_op = fwrk::StoreOp::Store,
                             .clear_value = {1.0F, 1.0F, 1.0F, 1.0F}})
      .set_indirect_buffer_input({.resource = {draw_count_proxy_}, .stages = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT})
      .set_indirect_buffer_input({.resource = {indirect_proxy_}, .stages = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT})
      .set_execute(
          [this](vk::CommandBuffer cmd)
          {
          ...
```
Both pass types share some common functions and the graphics pass has some extra functions of its own like color/depth attachments, vertex/index buffer input, etc.

Pass dependencies can also explicitly depend on a resource from a certain pass.

```cpp
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

graph.compile();
graph.execute(cmd);
```
```
A
C
B
```

### End states
The graph also allows you to set image/buffer end states. Which will transition your image/buffer states to whatever you want after the final pass. This is useful for example for transitioning your swapchain images from whatever stage/access/layout they were to the present layout.

```cpp
graph.set_image_end_state(
      swapchain_proxy_,
      {.access = VK_ACCESS_2_NONE, .stages = VK_PIPELINE_STAGE_2_NONE, .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
```

## Prerequisites

- Vulkan SDK >= 1.3
- Meson >= 1.1.0
- Ninja
- C++20 compiler
- GoogleTest (not required if not running tests)

## Build

```
meson setup build
```

```
meson compile -C build
```

## Test

```
meson test -C build
```
