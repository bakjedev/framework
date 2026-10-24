#pragma once
#include <vulkan/vulkan.h>

#include "framework/util/flat_hash_map.hpp"
#include "framework/util/hash_combine.hpp"
#include "physical_state.hpp"

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
    uint32_t mip_count;
    uint32_t layer_count;
  };

  struct PhysicalImage {
    VkImage handle;
    PhysicalState uniform_state;
    std::vector<PhysicalState> states;
    uint32_t levels;
    uint32_t layers;
    flat_hash_map<ViewKey, VkImageView, ViewKeyHasher> views;

    PhysicalImage(VkImage h, const PhysicalState& s, const uint32_t lvl, const uint32_t lyr) :
        handle(h), uniform_state(s), levels(lvl), layers(lyr)
    {
    }

    [[nodiscard]] bool is_state_uniform() const { return states.empty(); }
    [[nodiscard]] PhysicalState& state_at(const uint32_t mip, const uint32_t layer)
    {
      return states.at(mip * layers + layer);
    }
  };

  template<typename T>
  concept ImageInterface = requires(const T& img) {
    { img.type() } -> std::same_as<VkImageType>;
    { img.size() } -> std::same_as<VkExtent3D>;
    { img.format() } -> std::same_as<VkFormat>;
    { img.mips() } -> std::same_as<uint32_t>;
    { img.layers() } -> std::same_as<uint32_t>;
    { img.image() } -> std::same_as<VkImage>;
  };

  struct ImageImportInfo {
    VkImageType type;
    VkExtent3D size;
    VkFormat format;
    uint32_t mip_count;
    uint32_t layer_count;
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
