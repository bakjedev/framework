#include "context.hpp"
#include <cassert>

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
  images_.emplace_back(raw, VK_NULL_HANDLE, info.state);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Import, Image{info.type, info.size, info.format}, physical_id, std::move(name));

  return ResourceID{id};
}

fwrk::ResourceID fwrk::Context::import_buffer(const BufferImportInfo& info, VkBuffer raw, std::string name)
{
  const auto physical_id = buffers_.size();
  buffers_.emplace_back(raw, VK_NULL_HANDLE, info.state);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Import, Buffer{info.size}, physical_id, std::move(name));

  return ResourceID{id};
}

void fwrk::Context::update_image(const ResourceID resource, const ImageImportInfo& info, VkImage raw)
{
  Resource& res = resources_.at(resource.id);
  PhysicalImage& phys = images_.at(res.physical_id);

  destroy_views(phys);
  if (auto* img = std::get_if<Image>(&res.desc)) {
    img->type = info.type;
    img->size = info.size;
    img->format = info.format;
  }
  phys.state = info.state;
  phys.handle = raw;
}

void fwrk::Context::update_buffer(const ResourceID resource, const BufferImportInfo& info, VkBuffer raw)
{
  Resource& res = resources_.at(resource.id);
  PhysicalBuffer& phys = buffers_.at(res.physical_id);

  if (auto* buf = std::get_if<Buffer>(&res.desc)) {
    buf->size = info.size;
  }
  phys.state = info.state;
  phys.handle = raw;
}

fwrk::ResourceID fwrk::Context::create_proxy(const ResourceID resource, std::string name)
{
  if (resource && resources_.at(resource.id).type == ResourceType::Proxy) return {};

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Proxy, ResourceDesc{}, resource.id, std::move(name));

  return ResourceID{id};
}

void fwrk::Context::update_proxy(const ResourceID proxy, const ResourceID resource)
{
  if (resources_.at(resource.id).type == ResourceType::Proxy) return;
  resources_.at(proxy.id).physical_id = resource.id;
}

VkImageView fwrk::Context::get_image_view(const ViewKey& key, const Resource& resource)
{
  if (device_ == VK_NULL_HANDLE || resource.type == ResourceType::Proxy) return VK_NULL_HANDLE;

  PhysicalImage& phys = images_.at(resource.physical_id);

  auto it = phys.views.find(key);
  if (it != phys.views.end()) {
    return it->second;
  }

  auto* img = std::get_if<Image>(&resource.desc);
  if (!img) return VK_NULL_HANDLE;

  const VkImageViewCreateInfo view_create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0u,
                                               .image = phys.handle,
                                               .viewType = key.view_type,
                                               .format = img->format,
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

const fwrk::Resource& fwrk::Context::resolve_proxy(const ResourceID resource) const
{
  assert(resource && "Invalid resource ID for resolving proxy");
  const Resource* res = &resources_.at(resource.id);
  if (res->type == ResourceType::Proxy) {
    res = &resources_.at(res->physical_id);
  }
  return *res;
}
