#include "context.hpp"

fwrk::Context::~Context()
{
  if (device_ == VK_NULL_HANDLE) return;
  for (auto& image: images_) {
    for (auto [_, view]: image.views) {
      if (view) {
        vkDestroyImageView(device_, view, nullptr);
      }
    }
    image.views.clear();
  }
}

fwrk::ResourceID fwrk::Context::import_image(const ImageImportInfo& info, VkImage raw, std::string name)
{
  const auto physical_id = images_.size();
  images_.emplace_back(raw, info.state);

  const auto id = resources_.size();
  resources_.emplace_back(Image{info.type, info.size, info.format}, physical_id, std::move(name));

  return ResourceID{ResourceType::Import, id};
}

fwrk::ResourceID fwrk::Context::import_buffer(const BufferImportInfo& info, VkBuffer raw, std::string name)
{
  const auto physical_id = buffers_.size();
  buffers_.emplace_back(raw, info.state);

  const auto id = resources_.size();
  resources_.emplace_back(Buffer{info.size}, physical_id, std::move(name));

  return ResourceID{ResourceType::Import, id};
}

void fwrk::Context::update_image(const ResourceID resource, const ImageImportInfo& info, VkImage raw)
{
  if (!resource || resource.type() != ResourceType::Import || resource.type() == ResourceType::Transient) return;
  Resource& res = resources_.at(resource.index());
  if (!std::holds_alternative<Image>(res.desc)) return;

  PhysicalImage& phys = images_.at(res.physical_id);
  destroy_views(phys);

  auto& [type, size, format] = std::get<Image>(res.desc);
  type = info.type;
  size = info.size;
  format = info.format;

  phys.state = info.state;
  phys.handle = raw;
}

void fwrk::Context::update_buffer(const ResourceID resource, const BufferImportInfo& info, VkBuffer raw)
{
  if (!resource || resource.type() != ResourceType::Import || resource.type() == ResourceType::Transient) return;
  Resource& res = resources_.at(resource.index());
  if (!std::holds_alternative<Buffer>(res.desc)) return;

  auto& [size] = std::get<Buffer>(res.desc);
  size = info.size;

  PhysicalBuffer& phys = buffers_.at(res.physical_id);
  phys.state = info.state;
  phys.handle = raw;
}

fwrk::ResourceID fwrk::Context::create_proxy(const ResourceID resource)
{
  if (resource && (resource.type() == ResourceType::Proxy || resource.type() == ResourceType::Transient)) return {};

  const auto id = proxies_.size();
  proxies_.push_back(resource);

  return ResourceID{ResourceType::Proxy, id};
}

void fwrk::Context::update_proxy(const ResourceID proxy, const ResourceID resource)
{
  if (!resource || resource.type() == ResourceType::Proxy || resource.type() == ResourceType::Transient) return;
  proxies_.at(proxy.index()) = resource;
}

VkImageView fwrk::Context::get_image_view(const ViewKey& key, const Resource& resource)
{
  if (device_ == VK_NULL_HANDLE || !std::holds_alternative<Image>(resource.desc)) return VK_NULL_HANDLE;

  PhysicalImage& phys = images_.at(resource.physical_id);

  auto it = phys.views.find(key);
  if (it != phys.views.end()) {
    return it->second;
  }

  const VkImageViewCreateInfo view_create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0u,
                                               .image = phys.handle,
                                               .viewType = key.view_type,
                                               .format = std::get<Image>(resource.desc).format,
                                               .components = {},
                                               .subresourceRange = {.aspectMask = key.aspect,
                                                                    .baseMipLevel = key.base_level,
                                                                    .levelCount = key.level_count,
                                                                    .baseArrayLayer = key.base_layer,
                                                                    .layerCount = key.layer_count}};

  VkImageView view = VK_NULL_HANDLE;
  if (vkCreateImageView(device_, &view_create_info, nullptr, &view) != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  phys.views[key] = view;

  return view;
}

void fwrk::Context::destroy_views(PhysicalImage& image) const
{
  if (device_ == VK_NULL_HANDLE) return;

  for (auto [_, view]: image.views) {
    if (view) {
      vkDestroyImageView(device_, view, nullptr);
    }
  }
  image.views.clear();
}

fwrk::PhysicalImage& fwrk::Context::get_physical_image(const uint64_t id, const ResourceType type)
{
  switch (type) {
    case ResourceType::Import:
      return images_.at(id);
    case ResourceType::Transient:
      return images_.at(id + current_frame_);
    default:
      throw std::runtime_error("Passed in a proxy into get physical image");
  }
}

fwrk::PhysicalBuffer& fwrk::Context::get_physical_buffer(const uint64_t id, const ResourceType type)
{
  switch (type) {
    case ResourceType::Import:
      return buffers_.at(id);
    case ResourceType::Transient:
      return buffers_.at(id + current_frame_);
    default:
      throw std::runtime_error("Passed in a proxy into get physical buffer");
  }
}
