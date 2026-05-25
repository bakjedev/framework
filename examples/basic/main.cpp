#include <iostream>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>

#define VK_CHECK(x)                                 \
  do {                                              \
    VkResult err = x;                               \
    if (err) {                                      \
      std::cout << "Vulkan error: " << err << "\n"; \
      abort();                                      \
    }                                               \
  } while (0)


int main()
{
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

  std::vector<VkImageView> swap_chain_image_views;
  swap_chain_image_views.resize(image_count);

  VkImageViewCreateInfo view_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .image = VK_NULL_HANDLE,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = image_format,
      .components = {},
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  for (uint32_t i = 0; i < image_count; i++) {
    view_create_info.image = swap_chain_images[i];
    VK_CHECK(vkCreateImageView(device, &view_create_info, nullptr, &swap_chain_image_views[i]));
  }


  std::vector depth_formats{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
  VkFormat depth_format{VK_FORMAT_UNDEFINED};
  for (VkFormat& format: depth_formats) {
    VkFormatProperties2 format_properties{
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, .pNext = nullptr, .formatProperties = {}};
    vkGetPhysicalDeviceFormatProperties2(physical_device, format, &format_properties);
    if (format_properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      depth_format = format;
      break;
    }
  }

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

  VkImageViewCreateInfo depth_view_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .image = depth_image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth_format,
      .components = {},
      .subresourceRange{
          .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };
  VkImageView depth_image_view = VK_NULL_HANDLE;
  VK_CHECK(vkCreateImageView(device, &depth_view_create_info, nullptr, &depth_image_view));

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

  auto recreate_swap_chain = [&] {
    vkDeviceWaitIdle(device);
    swap_chain_create_info.oldSwapchain = swap_chain;
    VK_CHECK(vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swap_chain));
    for (uint32_t i = 0; i < image_count; i++) {
      vkDestroyImageView(device, swap_chain_image_views[i], nullptr);
    }
    VK_CHECK(vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr));
    swap_chain_images.resize(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data()));
    swap_chain_image_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
      view_create_info.image = swap_chain_images[i];
      VK_CHECK(vkCreateImageView(device, &view_create_info, nullptr, &swap_chain_image_views[i]));
    }
    for (const auto& semaphore: render_complete_semaphores) {
      vkDestroySemaphore(device, semaphore, nullptr);
    }
    render_complete_semaphores.resize(image_count);
    for (auto& semaphore: render_complete_semaphores) {
      VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphore));
    }
    vkDestroySwapchainKHR(device, swap_chain_create_info.oldSwapchain, nullptr);
    vmaDestroyImage(allocator, depth_image, depth_image_allocation);
    vkDestroyImageView(device, depth_image_view, nullptr);

    VK_CHECK(vmaCreateImage(allocator, &depth_image_create_info, &alloc_create_info, &depth_image,
                            &depth_image_allocation, nullptr));
    VK_CHECK(vkCreateImageView(device, &depth_view_create_info, nullptr, &depth_image_view));
  };

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

    std::array barriers{
        VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swap_chain_images[image_index],
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .baseMipLevel = 0,
                              .levelCount = 1,
                              .baseArrayLayer = 0,
                              .layerCount = 1}},
        VkImageMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                              .pNext = nullptr,
                              .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                              .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                              .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                              .image = depth_image,
                              .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                                .baseMipLevel = 0,
                                                .levelCount = 1,
                                                .baseArrayLayer = 0,
                                                .layerCount = 1}}};
    VkDependencyInfo barriers_dep_info{};
    barriers_dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    barriers_dep_info.imageMemoryBarrierCount = barriers.size();
    barriers_dep_info.pImageMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(cmd, &barriers_dep_info);

    // something here

    VkImageMemoryBarrier2 present_barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                          .pNext = nullptr,
                                          .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                          .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          .dstAccessMask = 0,
                                          .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                          .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .image = swap_chain_images[image_index],
                                          .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                            .baseMipLevel = 0,
                                                            .levelCount = 1,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1}};
    VkDependencyInfo present_barrier_dep_info{};
    present_barrier_dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    present_barrier_dep_info.imageMemoryBarrierCount = 1;
    present_barrier_dep_info.pImageMemoryBarriers = &present_barrier;
    vkCmdPipelineBarrier2(cmd, &present_barrier_dep_info);

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


  vkDeviceWaitIdle(device);
  vkDestroyCommandPool(device, command_pool, nullptr);
  for (uint32_t i = 0; i < max_frames_in_flight; i++) {
    vkDestroyFence(device, fences[i], nullptr);
    vkDestroySemaphore(device, image_acquired_semaphores[i], nullptr);
  }
  for (auto& semaphore: render_complete_semaphores) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }

  vkDestroyImageView(device, depth_image_view, nullptr);
  vmaDestroyImage(allocator, depth_image, depth_image_allocation);
  for (uint32_t i = 0; i < image_count; i++) {
    vkDestroyImageView(device, swap_chain_image_views[i], nullptr);
  }
  vkDestroySwapchainKHR(device, swap_chain, nullptr);
  vmaDestroyAllocator(allocator);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
