#pragma once

template<fwrk::ImageInterface I>
fwrk::ResourceID fwrk::Context::import_image(const I& image, const PhysicalState& state, std::string name)
{
  const auto physical_id = images_.size();
  images_.emplace_back(image.image(), VK_NULL_HANDLE, state);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Import, Image{image.type(), image.size(), image.format()}, physical_id,
                          std::move(name));

  return ResourceID{id};
}

template<fwrk::BufferInterface I>
fwrk::ResourceID fwrk::Context::import_buffer(const I& buffer, const PhysicalState& state, std::string name)
{
  const auto physical_id = buffers_.size();
  buffers_.emplace_back(buffer.buffer(), VK_NULL_HANDLE, state);

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Import, Buffer{buffer.size()}, physical_id, std::move(name));

  return ResourceID{id};
}

template<fwrk::ImageInterface I>
void fwrk::Context::update_image(const ResourceID resource, const I& image, const PhysicalState& state)
{
  if (!resource) return;

  Resource& res = resources_.at(resource.id);
  if (!res.is_image() || res.type == ResourceType::Proxy) return;
  PhysicalImage& phys = images_.at(res.physical_id);

  destroy_views(phys);
  if (auto* img = std::get_if<Image>(&res.desc)) {
    img->type = image.type();
    img->size = image.size();
    img->format = image.format();
  }
  phys.state = state;
  phys.handle = image.image();
}

template<fwrk::BufferInterface I>
void fwrk::Context::update_buffer(const ResourceID resource, const I& buffer, const PhysicalState& state)
{
  if (!resource) return;

  Resource& res = resources_.at(resource.id);
  if (!res.is_buffer() || res.type == ResourceType::Proxy) return;
  PhysicalBuffer& phys = buffers_.at(res.physical_id);

  if (auto* buf = std::get_if<Buffer>(&res.desc)) {
    buf->size = buffer.size();
  }
  phys.state = state;
  phys.handle = buffer.buffer();
}
