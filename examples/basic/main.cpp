#include <cstdint>
#include "context.hpp"
#include "vk_common.hpp"

int main()
{
  constexpr int window_width = 800;
  constexpr int window_height = 600;
  constexpr uint32_t frames_in_flight = 2;
  constexpr VkExtent2D extent{window_width, window_height};

  GLFWwindow* window = create_window(window_width, window_height, "basic");
  Backend backend = create_backend(window);
  Swapchain swapchain = create_swapchain(backend, extent);
  Image depth = create_image(backend, {.extent = extent,
                                       .format = VK_FORMAT_D32_SFLOAT,
                                       .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                       .dedicated = true});
  Frames frames = create_frames(backend, frames_in_flight, static_cast<uint32_t>(swapchain.images.size()));

  VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device);
  VkShaderModule vert = load_shader(backend.device, "basic.vert.spv");
  VkShaderModule frag = load_shader(backend.device, "basic.frag.spv");
  VkPipeline pipeline =
      create_default_pipeline(backend.device, pipeline_layout, vert, frag, swapchain.format, depth.format);

  {
    // Creating the framework context
    fwrk::Context context{backend.device};

    std::vector<fwrk::ResourceID> swapchain_imports(swapchain.images.size());
    fwrk::ResourceID depth_import{};
    fwrk::ResourceID swapchain_proxy = context.create_proxy();

    auto import_resources = [&] {
      const fwrk::ImageResource color_img_desc{.type = VK_IMAGE_TYPE_2D,
                                               .size = {swapchain.extent.width, swapchain.extent.height, 1},
                                               .format = swapchain.format,
                                               .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                               .state = fwrk::ImageState::Undefined};
      swapchain_imports.resize(swapchain.images.size());
      for (uint32_t i = 0; i < swapchain.images.size(); i++) {
        fwrk::ResourceID& res = swapchain_imports[i];
        if (res) {
          context.update_image(res, color_img_desc, swapchain.images[i]);
        } else {
          res = context.import_image(color_img_desc, swapchain.images[i], "Swapchain image");
        }
      }

      const fwrk::ImageResource depth_img_desc{.type = VK_IMAGE_TYPE_2D,
                                               .size = {depth.extent.width, depth.extent.height, 1},
                                               .format = depth.format,
                                               .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                               .state = fwrk::ImageState::Undefined};
      if (depth_import)
        context.update_image(depth_import, depth_img_desc, depth.image);
      else
        depth_import = context.import_image(depth_img_desc, depth.image);
    };

    auto recreate_swapchain = [&] {
      vkDeviceWaitIdle(backend.device);
      VkSwapchainKHR old = swapchain.handle;
      swapchain = create_swapchain(backend, extent, swapchain.format, swapchain.color_space, old);
      vkDestroySwapchainKHR(backend.device, old, nullptr);

      destroy_image(backend, depth);
      depth = create_image(backend, {.extent = swapchain.extent,
                                     .format = VK_FORMAT_D32_SFLOAT,
                                     .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                     .dedicated = true});

      resize_frames(backend, frames, static_cast<uint32_t>(swapchain.images.size()));
      import_resources();
    };

    import_resources();

    uint32_t frame = 0;
    while (!glfwWindowShouldClose(window)) {
      VK_CHECK(vkWaitForFences(backend.device, 1, &frames.fences[frame], true, UINT64_MAX));

      uint32_t image_index = 0;
      VkResult result = vkAcquireNextImageKHR(backend.device, swapchain.handle, UINT64_MAX,
                                              frames.image_acquired[frame], VK_NULL_HANDLE, &image_index);

      if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        continue;
      }
      if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cout << "Failed to acquire swapchain image\n";
        std::abort();
      }

      VK_CHECK(vkResetFences(backend.device, 1, &frames.fences[frame]));

      VkCommandBuffer cmd = frames.command_buffers[frame];
      VK_CHECK(vkResetCommandBuffer(cmd, 0));
      VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
      VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

      // --------------------------------
      // Using the framework render graph
      fwrk::Graph& graph = context.graph();

      graph.add_graphics_pass("RenderPass")
          .set_color_attachment({.resource = {swapchain_proxy},
                                 .load_op = fwrk::LoadOp::Clear,
                                 .store_op = fwrk::StoreOp::Store,
                                 .clear_value = {1.0F, 1.0F, 1.0F, 1.0F}})
          .set_depth_attachment({.resource = {depth_import},
                                 .load_op = fwrk::LoadOp::Clear,
                                 .store_op = fwrk::StoreOp::Store,
                                 .clear_value = {1.0f, 1.0f, 1.0f, 1.0f}})
          .set_execute([&](VkCommandBuffer cb) {
            const VkViewport viewport{.width = static_cast<float>(swapchain.extent.width),
                                      .height = static_cast<float>(swapchain.extent.height),
                                      .minDepth = 0.0f,
                                      .maxDepth = 1.0f};
            vkCmdSetViewport(cb, 0, 1, &viewport);

            const VkRect2D scissor{.extent = swapchain.extent};
            vkCmdSetScissor(cb, 0, 1, &scissor);

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vkCmdDraw(cb, 3, 1, 0, 0);
          });

      graph.add_graphics_pass("RenderPass")
          .set_color_attachment(
              {.resource = {swapchain_proxy}, .load_op = fwrk::LoadOp::Load, .store_op = fwrk::StoreOp::Store})
          .set_depth_attachment(
              {.resource = {depth_import}, .load_op = fwrk::LoadOp::Load, .store_op = fwrk::StoreOp::Store})
          .set_execute([&](VkCommandBuffer cb) {
            const VkViewport viewport{.width = static_cast<float>(swapchain.extent.width),
                                      .height = static_cast<float>(swapchain.extent.height),
                                      .minDepth = 0.0f,
                                      .maxDepth = 1.0f};
            vkCmdSetViewport(cb, 0, 1, &viewport);

            const VkRect2D scissor{.extent = swapchain.extent};
            vkCmdSetScissor(cb, 0, 1, &scissor);

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vkCmdDraw(cb, 3, 1, 3, 0);
          });

      graph.set_image_end_state(
          swapchain_imports[image_index],
          {.access = VK_ACCESS_2_NONE, .stages = VK_PIPELINE_STAGE_2_NONE, .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});

      static bool compiled = false;
      if (!compiled) {
        graph.compile();
        compiled = true;
      }

      context.update_proxy(swapchain_proxy, swapchain_imports[image_index]);

      graph.execute(cmd);

      // --------------------------------
      vkEndCommandBuffer(cmd);

      VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .waitSemaphoreCount = 1,
                          .pWaitSemaphores = &frames.image_acquired[frame],
                          .pWaitDstStageMask = &wait_stage,
                          .commandBufferCount = 1,
                          .pCommandBuffers = &cmd,
                          .signalSemaphoreCount = 1,
                          .pSignalSemaphores = &frames.render_complete[image_index]};
      VK_CHECK(vkQueueSubmit(backend.queue, 1, &submit, frames.fences[frame]));

      VkPresentInfoKHR present{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &frames.render_complete[image_index],
                               .swapchainCount = 1,
                               .pSwapchains = &swapchain.handle,
                               .pImageIndices = &image_index};
      result = vkQueuePresentKHR(backend.queue, &present);
      if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        recreate_swapchain();
      else if (result != VK_SUCCESS) {
        std::cout << "Failed to present\n";
        std::abort();
      }

      frame = (frame + 1) % frames_in_flight;
      glfwPollEvents();
    }
  }

  vkDeviceWaitIdle(backend.device);
  vkDestroyPipeline(backend.device, pipeline, nullptr);
  vkDestroyPipelineLayout(backend.device, pipeline_layout, nullptr);
  vkDestroyShaderModule(backend.device, vert, nullptr);
  vkDestroyShaderModule(backend.device, frag, nullptr);
  destroy_frames(backend, frames);
  destroy_image(backend, depth);
  destroy_swapchain(backend, swapchain);
  destroy_backend(backend);
  destroy_window(window);
  return 0;
}
