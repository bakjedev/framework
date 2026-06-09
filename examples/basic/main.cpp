#include <iostream>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <fstream>

#include "context.hpp"

#define VK_CHECK(x)                                 \
  do {                                              \
    VkResult err = x;                               \
    if (err) {                                      \
      std::cout << "Vulkan error: " << err << "\n"; \
      abort();                                      \
    }                                               \
  } while (0)


std::vector<uint32_t> read_spv(const char* path)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return {};

  const std::streamsize size = file.tellg();
  if (size % 4 != 0) return {};

  std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(code.data()), size);
  return code;
}

int main()
{
  // glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11); // for renderdoc
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  constexpr int window_width = 800;
  constexpr int window_height = 600;
  GLFWwindow* window = glfwCreateWindow(window_width, window_height, "basic", nullptr, nullptr);

  if (!window) {
    std::cerr << "Failed to create window\n";
    return 1;
  }

  constexpr VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                       .pNext = nullptr,
                                       .pApplicationName = "Basic",
                                       .applicationVersion = 0,
                                       .pEngineName = "BasicEngine",
                                       .engineVersion = 0,
                                       .apiVersion = VK_API_VERSION_1_3};

  uint32_t glfw_extension_count = 0;
  const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

  const VkInstanceCreateInfo instance_create_info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .pApplicationInfo = &app_info,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = glfw_extension_count,
      .ppEnabledExtensionNames = glfw_extensions,
  };

  VkInstance instance = VK_NULL_HANDLE;
  VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &instance));

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  glfwCreateWindowSurface(instance, window, nullptr, &surface);
  if (!surface) {
    std::cout << "Failed to create surface\n";
    abort();
  }

  uint32_t device_count{0};
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
  std::vector<VkPhysicalDevice> devices(device_count);
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, devices.data()));
  VkPhysicalDevice physical_device = devices[0];

  VkPhysicalDeviceProperties2 device_properties{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = nullptr, .properties = {}};
  vkGetPhysicalDeviceProperties2(physical_device, &device_properties);
  std::cout << "Selected device: " << device_properties.properties.deviceName << "\n";

  uint32_t queue_family_count{0};
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
  std::array<uint32_t, 1> queue_family = {};
  for (size_t i = 0; i < queue_families.size(); i++) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      queue_family[0] = static_cast<uint32_t>(i);
      break;
    }
  }

  if (glfwGetPhysicalDevicePresentationSupport(instance, physical_device, queue_family[0]) != GLFW_TRUE) {
    std::cout << "Failed to find queue family with present\n";
    abort();
  }

  constexpr float queue_family_priorities = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0u,
                                            .queueFamilyIndex = queue_family[0],
                                            .queueCount = 1,
                                            .pQueuePriorities = &queue_family_priorities};

  const std::vector device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkPhysicalDeviceVulkan12Features vk_12_features{};
  vk_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vk_12_features.descriptorIndexing = true;
  vk_12_features.shaderSampledImageArrayNonUniformIndexing = true;
  vk_12_features.descriptorBindingVariableDescriptorCount = true;
  vk_12_features.runtimeDescriptorArray = true;
  vk_12_features.bufferDeviceAddress = true;

  VkPhysicalDeviceVulkan13Features vk_13_features{};
  vk_13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vk_13_features.pNext = &vk_12_features;
  vk_13_features.synchronization2 = true;
  vk_13_features.dynamicRendering = true;

  VkPhysicalDeviceFeatures vk_10_features{};
  vk_10_features.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo device_create_info{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                        .pNext = &vk_13_features,
                                        .flags = 0u,
                                        .queueCreateInfoCount = 1,
                                        .pQueueCreateInfos = &queue_create_info,
                                        .enabledLayerCount = 0,
                                        .ppEnabledLayerNames = nullptr,
                                        .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
                                        .ppEnabledExtensionNames = device_extensions.data(),
                                        .pEnabledFeatures = &vk_10_features};

  VkDevice device = VK_NULL_HANDLE;
  VK_CHECK(vkCreateDevice(physical_device, &device_create_info, nullptr, &device));

  VkQueue queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(device, queue_family[0], 0, &queue);
  if (!queue) {
    std::cout << "Failed to get queue\n";
    abort();
  }

  VmaVulkanFunctions vk_functions{};
  vk_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vk_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  vk_functions.vkCreateImage = vkCreateImage;

  VmaAllocatorCreateInfo allocator_create_info{.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                               .physicalDevice = physical_device,
                                               .device = device,
                                               .preferredLargeHeapBlockSize = 0,
                                               .pAllocationCallbacks = nullptr,
                                               .pDeviceMemoryCallbacks = nullptr,
                                               .pHeapSizeLimit = nullptr,
                                               .pVulkanFunctions = &vk_functions,
                                               .instance = instance,
                                               .vulkanApiVersion = app_info.apiVersion,
                                               .pTypeExternalMemoryHandleTypes = nullptr};
  VmaAllocator allocator = VK_NULL_HANDLE;
  VK_CHECK(vmaCreateAllocator(&allocator_create_info, &allocator));

  VkSurfaceCapabilitiesKHR surface_capabilities{};
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities));

  VkExtent2D swap_chain_extent{surface_capabilities.currentExtent};
  if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
    swap_chain_extent = {.width = static_cast<uint32_t>(window_width), .height = static_cast<uint32_t>(window_height)};
  }

  constexpr VkFormat image_format{VK_FORMAT_B8G8R8A8_SRGB};

  VkSwapchainCreateInfoKHR swap_chain_create_info{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0u,
      .surface = surface,
      .minImageCount = surface_capabilities.minImageCount,
      .imageFormat = image_format,
      .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
      .imageExtent{.width = swap_chain_extent.width, .height = swap_chain_extent.height},
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = queue_family.size(),
      .pQueueFamilyIndices = queue_family.data(),
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE};

  VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
  VK_CHECK(vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swap_chain));

  uint32_t image_count = 0;
  VK_CHECK(vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr));

  std::vector<VkImage> swap_chain_images;
  swap_chain_images.resize(image_count);
  VK_CHECK(vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data()));

  constexpr VkFormat depth_format = VK_FORMAT_D32_SFLOAT;

  VkImageCreateInfo depth_image_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depth_format,
      .extent{.width = static_cast<uint32_t>(window_width), .height = static_cast<uint32_t>(window_height), .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = queue_family.size(),
      .pQueueFamilyIndices = queue_family.data(),
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

  VmaAllocationCreateInfo alloc_create_info{};
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
  VkImage depth_image = VK_NULL_HANDLE;
  VmaAllocation depth_image_allocation = VK_NULL_HANDLE;
  VK_CHECK(vmaCreateImage(allocator, &depth_image_create_info, &alloc_create_info, &depth_image,
                          &depth_image_allocation, nullptr));

  constexpr uint32_t max_frames_in_flight{2};

  std::array<VkCommandBuffer, max_frames_in_flight> command_buffers{};
  std::array<VkFence, max_frames_in_flight> fences{};
  std::array<VkSemaphore, max_frames_in_flight> image_acquired_semaphores{};
  std::vector<VkSemaphore> render_complete_semaphores{swap_chain_images.size()};

  constexpr VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0u};
  constexpr VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

  for (uint32_t i = 0; i < max_frames_in_flight; i++) {
    VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &fences[i]));
    VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &image_acquired_semaphores[i]));
  }
  for (auto& semaphore: render_complete_semaphores) {
    VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphore));
  }

  VkCommandPoolCreateInfo command_pool_create_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                   .queueFamilyIndex = queue_family[0]};
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));

  VkCommandBufferAllocateInfo command_buffer_alloc_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = max_frames_in_flight,
  };
  VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_alloc_create_info, command_buffers.data()));

  auto vertex_code = read_spv("basic.vert.spv");
  VkShaderModuleCreateInfo vertex_shader_create_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                     .codeSize = vertex_code.size() * sizeof(uint32_t),
                                                     .pCode = vertex_code.data()};
  VkShaderModule vertex_shader = VK_NULL_HANDLE;
  VK_CHECK(vkCreateShaderModule(device, &vertex_shader_create_info, nullptr, &vertex_shader));

  auto fragment_code = read_spv("basic.frag.spv");
  VkShaderModuleCreateInfo fragment_shader_create_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                       .codeSize = fragment_code.size() * sizeof(uint32_t),
                                                       .pCode = fragment_code.data()};
  VkShaderModule fragment_shader = VK_NULL_HANDLE;
  VK_CHECK(vkCreateShaderModule(device, &fragment_shader_create_info, nullptr, &fragment_shader));

  VkPipelineLayoutCreateInfo pipeline_layout_create_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         .setLayoutCount = 0,
                                                         .pSetLayouts = nullptr,
                                                         .pushConstantRangeCount = 0,
                                                         .pPushConstantRanges = nullptr};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout));

  std::vector<VkPipelineShaderStageCreateInfo> shader_stages{
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vertex_shader,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fragment_shader,
       .pName = "main"}};

  VkPipelineVertexInputStateCreateInfo vertex_input_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineViewportStateCreateInfo viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
  std::vector dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                 .dynamicStateCount = 2,
                                                 .pDynamicStates = dynamic_states.data()};

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};

  VkPipelineRenderingCreateInfo rendering_create_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                      .colorAttachmentCount = 1,
                                                      .pColorAttachmentFormats = &image_format,
                                                      .depthAttachmentFormat = depth_format};

  VkPipelineColorBlendAttachmentState blend_attachment{.colorWriteMask = 0xF};
  VkPipelineColorBlendStateCreateInfo color_blend_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blend_attachment};
  VkPipelineRasterizationStateCreateInfo rasterization_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .lineWidth = 1.0f};
  VkPipelineMultisampleStateCreateInfo multisample_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  VkGraphicsPipelineCreateInfo pipeline_create_info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                    .pNext = &rendering_create_info,
                                                    .stageCount = static_cast<uint32_t>(shader_stages.size()),
                                                    .pStages = shader_stages.data(),
                                                    .pVertexInputState = &vertex_input_state,
                                                    .pInputAssemblyState = &input_assembly_state,
                                                    .pViewportState = &viewport_state,
                                                    .pRasterizationState = &rasterization_state,
                                                    .pMultisampleState = &multisample_state,
                                                    .pDepthStencilState = &depth_stencil_state,
                                                    .pColorBlendState = &color_blend_state,
                                                    .pDynamicState = &dynamic_state,
                                                    .layout = pipeline_layout};
  VkPipeline pipeline = VK_NULL_HANDLE;
  VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));

  {
    fwrk::Context context{device};
    std::vector<fwrk::ResourceID> swap_chain_imports{image_count};
    fwrk::ResourceID depth_import{};

    auto recreate_swap_chain = [&] {
      vkDeviceWaitIdle(device);

      swap_chain_create_info.oldSwapchain = swap_chain;
      VK_CHECK(vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swap_chain));
      vkDestroySwapchainKHR(device, swap_chain_create_info.oldSwapchain, nullptr);

      VK_CHECK(vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr));
      swap_chain_images.resize(image_count);
      VK_CHECK(vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data()));

      swap_chain_imports.resize(image_count);
      for (uint32_t i = 0; i < image_count; i++) {
        fwrk::ResourceID& res = swap_chain_imports[i];
        constexpr fwrk::ImageResource image{.x = window_width,
                                            .y = window_height,
                                            .z = 1,
                                            .format = image_format,
                                            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                            .layer_count = 1,
                                            .level_count = 1,
                                            .state = fwrk::ImageState::Undefined};
        if (res) {
          context.update_image(res, image, swap_chain_images[i]);
        } else {
          res = context.import_image(image, swap_chain_images[i]);
        }
      }

      for (const auto& semaphore: render_complete_semaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
      }
      render_complete_semaphores.resize(image_count);
      for (auto& semaphore: render_complete_semaphores) {
        VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphore));
      }

      vmaDestroyImage(allocator, depth_image, depth_image_allocation);
      VK_CHECK(vmaCreateImage(allocator, &depth_image_create_info, &alloc_create_info, &depth_image,
                              &depth_image_allocation, nullptr));
      constexpr fwrk::ImageResource depth_image_res{.x = window_width,
                                                    .y = window_height,
                                                    .z = 1,
                                                    .format = depth_format,
                                                    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                    .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                    .layer_count = 1,
                                                    .level_count = 1,
                                                    .state = fwrk::ImageState::Undefined};
      if (depth_import) {
        context.update_image(depth_import, depth_image_res, depth_image);
      } else {
        depth_import = context.import_image(depth_image_res, depth_image);
      }
    };

    for (uint32_t i = 0; i < image_count; i++) {
      swap_chain_imports[i] = context.import_image({.x = window_width,
                                                    .y = window_height,
                                                    .z = 1,
                                                    .format = image_format,
                                                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                    .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                                    .layer_count = 1,
                                                    .level_count = 1,
                                                    .state = fwrk::ImageState::Undefined},
                                                   swap_chain_images[i], "Swapchain image");
    }

    depth_import = context.import_image({.x = window_width,
                                         .y = window_height,
                                         .z = 1,
                                         .format = depth_format,
                                         .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                         .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                                         .layer_count = 1,
                                         .level_count = 1,
                                         .state = fwrk::ImageState::Undefined},
                                        depth_image);

    uint32_t frame_index = 0;
    while (!glfwWindowShouldClose(window)) {
      VK_CHECK(vkWaitForFences(device, 1, &fences[frame_index], true, UINT64_MAX));
      VK_CHECK(vkResetFences(device, 1, &fences[frame_index]));

      uint32_t image_index = 0;
      VkResult result = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_acquired_semaphores[frame_index],
                                              VK_NULL_HANDLE, &image_index);

      if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swap_chain();
        continue;
      }

      if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cout << "Failed to acquire swapchain image\n";
        abort();
      }

      auto cmd = command_buffers[frame_index];
      VK_CHECK(vkResetCommandBuffer(cmd, 0));

      VkCommandBufferBeginInfo cmd_begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                              .pNext = nullptr,
                                              .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                              .pInheritanceInfo = nullptr};
      VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

      fwrk::Graph& graph = context.graph();

      graph.add_graphics_pass("RenderPass")
          .set_color_attachment({.resource = {swap_chain_imports[image_index]},
                                 .load_op = fwrk::LoadOp::Clear,
                                 .store_op = fwrk::StoreOp::Store,
                                 .clear_value = {1.0F, 1.0F, 1.0F, 1.0F}})
          .set_depth_attachment(
              {.resource = {depth_import}, .load_op = fwrk::LoadOp::Clear, .store_op = fwrk::StoreOp::Store})
          .set_execute([&]([[maybe_unused]] VkCommandBuffer cb) {
            constexpr VkViewport viewport{.width = static_cast<float>(window_width),
                                          .height = static_cast<float>(window_height),
                                          .minDepth = 0.0f,
                                          .maxDepth = 1.0f};
            vkCmdSetViewport(cb, 0, 1, &viewport);

            constexpr VkRect2D scissor{
                .extent{.width = static_cast<uint32_t>(window_width), .height = static_cast<uint32_t>(window_height)}};
            vkCmdSetScissor(cb, 0, 1, &scissor);

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vkCmdDraw(cb, 3, 1, 0, 0);
          });

      graph.set_image_end_state(
          swap_chain_imports[image_index],
          {.access = VK_ACCESS_2_NONE, .stage = VK_PIPELINE_STAGE_2_NONE, .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});


      graph.compile();

      graph.execute(cmd);

      vkEndCommandBuffer(cmd);

      VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .pNext = nullptr,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &image_acquired_semaphores[frame_index],
                               .pWaitDstStageMask = &wait_stages,
                               .commandBufferCount = 1,
                               .pCommandBuffers = &cmd,
                               .signalSemaphoreCount = 1,
                               .pSignalSemaphores = &render_complete_semaphores[image_index]};
      VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, fences[frame_index]));

      frame_index = (frame_index + 1) % max_frames_in_flight;

      VkPresentInfoKHR present_info{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                    .pNext = nullptr,
                                    .waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &render_complete_semaphores[image_index],
                                    .swapchainCount = 1,
                                    .pSwapchains = &swap_chain,
                                    .pImageIndices = &image_index,
                                    .pResults = nullptr};
      result = vkQueuePresentKHR(queue, &present_info);
      if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swap_chain();
      } else if (result != VK_SUCCESS) {
        std::cout << "Failed to present\n";
        abort();
      }

      glfwPollEvents();
    }
  }


  vkDeviceWaitIdle(device);
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  vkDestroyShaderModule(device, vertex_shader, nullptr);
  vkDestroyShaderModule(device, fragment_shader, nullptr);
  vkDestroyCommandPool(device, command_pool, nullptr);
  for (uint32_t i = 0; i < max_frames_in_flight; i++) {
    vkDestroyFence(device, fences[i], nullptr);
    vkDestroySemaphore(device, image_acquired_semaphores[i], nullptr);
  }
  for (auto& semaphore: render_complete_semaphores) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }

  vmaDestroyImage(allocator, depth_image, depth_image_allocation);
  vkDestroySwapchainKHR(device, swap_chain, nullptr);
  vmaDestroyAllocator(allocator);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
