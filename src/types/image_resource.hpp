#pragma once
#include <vulkan/vulkan.h>

#include "physical_state.hpp"
#include "util/flat_hash_map.hpp"
#include "util/hash_combine.hpp"

namespace fwrk {
  struct ViewKey {
    VkImageAspectFlags aspect;
    uint32_t base_level;
    uint32_t level_count;
    uint32_t base_layer;
    uint32_t layer_count;
    VkImageViewType view_type;

    ViewKey(const VkImageSubresourceRange& sub, const VkImageViewType view_type_) :
        aspect(sub.aspectMask), base_level(sub.baseMipLevel), level_count(sub.levelCount),
        base_layer(sub.baseArrayLayer), layer_count(sub.layerCount), view_type(view_type_)
    {
    }

    bool operator==(const ViewKey&) const = default;
  };

  struct ViewKeyHasher {
    size_t operator()(const ViewKey& key) const
    {
      size_t seed = 0;
      hash_combine(seed, key.aspect);
      hash_combine(seed, key.base_level);
      hash_combine(seed, key.level_count);
      hash_combine(seed, key.base_layer);
      hash_combine(seed, key.layer_count);
      hash_combine(seed, key.view_type);
      return seed;
    }
  };

  struct Image {
    VkImageType type;
    VkExtent3D size;
    VkFormat format;
  };

  struct PhysicalImage {
    VkImage handle;
    PhysicalState state;
    flat_hash_map<ViewKey, VkImageView, ViewKeyHasher> views;
  };

  template<typename T>
  concept ImageInterface = requires(const T& img) {
    { img.type() } -> std::same_as<VkImageType>;
    { img.size() } -> std::same_as<VkExtent3D>;
    { img.format() } -> std::same_as<VkFormat>;
    { img.image() } -> std::same_as<VkImage>;
  };

  struct ImageImportInfo {
    VkImageType type;
    VkExtent3D size;
    VkFormat format;
    PhysicalState state;
  };

  struct ImageCreateInfo {
    VkImageType type;
    VkExtent3D size;
    VkFormat format;
    VkImageCreateFlags flags;
    uint32_t mips;
    uint32_t layers;
    VkSampleCountFlagBits samples;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
  };
} // namespace fwrk
