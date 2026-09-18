#include "framework/context.hpp"
#include <stdexcept>

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

  for (auto& image: transient_images_) {
    for (auto [_, view]: image.views) {
      if (view) {
        vkDestroyImageView(device_, view, nullptr);
      }
    }
    image.views.clear();
  }

  for (auto& [_, transients]: deletion_queue_) {
    for (Resource& transient: transients) {
      destroy_transient(transient);
    }
  }
  for (Resource& transient: transients_) {
    destroy_transient(transient);
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

VkImageView fwrk::Context::acquire_image_view(const ResourceID resource, const ViewKey& view_key,
                                              const uint32_t frame_index)
{
  const Resource& res = get_resource(resource);
  assert(std::holds_alternative<Image>(res.desc) && "Passed a buffer into get image view");
  auto& physical = get_physical_image(res.physical_id, resource.type(), frame_index);
  const auto& image = std::get<Image>(res.desc);
  auto image_view = get_image_view(view_key, image, physical);
  return image_view;
}

std::vector<VkImageView> fwrk::Context::get_image_views(const ResourceID resource)
{
  std::vector<VkImageView> result;
  const Resource& res = get_resource(resource);
  assert(std::holds_alternative<Image>(res.desc) && "Passed a buffer into get image views");
  const auto& views = get_physical_image(res.physical_id, resource.type()).views;
  for (const auto& [_, view]: views) {
    result.push_back(view);
  }
  return result;
}

std::optional<VkImageView> fwrk::Context::get_first_image_view(ResourceID resource)
{
  const auto& views = get_image_views(resource);
  if (views.empty()) return std::nullopt;
  return views.at(0);
}

VkImage fwrk::Context::get_raw_image(const ResourceID resource)
{
  const Resource& res = get_resource(resource);
  assert(std::holds_alternative<Image>(res.desc) && "Passed a buffer into get raw image");
  return get_physical_image(res.physical_id, resource.type()).handle;
}

VkBuffer fwrk::Context::get_raw_buffer(const ResourceID resource)
{
  const Resource& res = get_resource(resource);
  assert(std::holds_alternative<Buffer>(res.desc) && "Passed an image into get raw buffer");
  return get_physical_buffer(res.physical_id, resource.type()).handle;
}

void fwrk::Context::allocate_transients(std::vector<Graph::TransientInfo> infos)
{
  for (auto& [desc, name]: infos) {
    if (std::holds_alternative<ImageCreateInfo>(desc)) {
      auto& info = std::get<ImageCreateInfo>(desc);
      const auto physical_id = transient_images_.size();
      for (uint32_t i = 0; i < frames_in_flight_; i++) {
        std::optional<PhysicalImage> physical = alloc_.create_image(info);
        transient_images_.push_back(std::move(*physical));
      }
      transients_.emplace_back(Image{info.type, info.size, info.format}, physical_id, std::move(name));
    } else if (std::holds_alternative<BufferCreateInfo>(desc)) {
      auto& info = std::get<BufferCreateInfo>(desc);
      const auto physical_id = transient_buffers_.size();
      for (uint32_t i = 0; i < frames_in_flight_; i++) {
        std::optional<PhysicalBuffer> physical = alloc_.create_buffer(info);
        transient_buffers_.push_back(*physical);
      }
      transients_.emplace_back(Buffer{info.size}, physical_id, std::move(name));
    }
  }
}
void fwrk::Context::schedule_destroy_transients()
{
  if (transients_.empty()) return;
  const uint64_t safe_frame = frame_ + frames_in_flight_;
  deletion_queue_.emplace_back(safe_frame, std::move(transients_));
  transients_.clear();
}

void fwrk::Context::destroy_transients(const uint32_t frame_index)
{
  auto it = deletion_queue_.begin();
  for (; it != deletion_queue_.end() && it->first <= frame_; ++it) {
    for (Resource& transient: it->second) {
      destroy_transient(transient);
    }
  }
  deletion_queue_.erase(deletion_queue_.begin(), it);
  frame_ += 1;
  frame_index_ = frame_index;
}

void fwrk::Context::destroy_transient(const Resource& transient)
{
  if (std::holds_alternative<Image>(transient.desc)) {
    for (uint32_t i = 0; i < frames_in_flight_; i++) {
      alloc_.destroy_image(transient_images_.at(transient.physical_id + i));
    }
  } else if (std::holds_alternative<Buffer>(transient.desc)) {
    for (uint32_t i = 0; i < frames_in_flight_; i++) {
      alloc_.destroy_buffer(transient_buffers_.at(transient.physical_id + i));
    }
  }
}


VkImageView fwrk::Context::get_image_view(const ViewKey& key, const Image& image, PhysicalImage& phys) const
{
  if (device_ == VK_NULL_HANDLE) return VK_NULL_HANDLE;


  auto it = phys.views.find(key);
  if (it != phys.views.end()) {
    return it->second;
  }

  const VkImageViewCreateInfo view_create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0u,
                                               .image = phys.handle,
                                               .viewType = key.view_type,
                                               .format = image.format,
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

fwrk::ResourceID fwrk::Context::resolve_proxy(const ResourceID resource) const
{
  assert(resource && "Invalid resource ID for resolving proxy");
  if (resource.type() == ResourceType::Proxy) {
    return proxies_.at(resource.index());
  }
  return resource;
}

fwrk::Resource& fwrk::Context::get_resource(const ResourceID id)
{
  switch (id.type()) {
    case ResourceType::Import:
      return resources_.at(id.index());
    case ResourceType::Transient:
      return transients_.at(id.index());
    default:
      throw std::runtime_error("Passed in a proxy into get resource");
  }
}

fwrk::PhysicalImage& fwrk::Context::get_physical_image(const uint64_t id, const ResourceType type,
                                                       const std::optional<uint32_t> frame_index)
{
  switch (type) {
    case ResourceType::Import:
      return images_.at(id);
    case ResourceType::Transient:
      return transient_images_.at(id + frame_index.value_or(frame_index_));
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
      return transient_buffers_.at(id + frame_index_);
    default:
      throw std::runtime_error("Passed in a proxy into get physical buffer");
  }
}
