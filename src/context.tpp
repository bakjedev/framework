#pragma once

template<fwrk::ImageInterface I>
fwrk::ResourceID fwrk::Context::import_image(const I& image, const PhysicalState& state, std::string name)
{
  const auto physical_id = images_.size();
  images_.emplace_back(image.image(), VK_NULL_HANDLE, state);

  const auto id = resources_.size();
  resources_.emplace_back(Image{image.type(), image.size(), image.format()}, physical_id, std::move(name));

  return ResourceID{ResourceType::Import, id};
}

template<fwrk::BufferInterface I>
fwrk::ResourceID fwrk::Context::import_buffer(const I& buffer, const PhysicalState& state, std::string name)
{
  const auto physical_id = buffers_.size();
  buffers_.emplace_back(buffer.buffer(), VK_NULL_HANDLE, state);

  const auto id = resources_.size();
  resources_.emplace_back(Buffer{buffer.size()}, physical_id, std::move(name));

  return ResourceID{ResourceType::Import, id};
}

template<fwrk::ImageInterface I>
void fwrk::Context::update_image(const ResourceID resource, const I& image, const PhysicalState& state)
{
  if (!resource || resource.type() != ResourceType::Import || resource.type() == ResourceType::Transient) return;
  Resource& res = resources_.at(resource.index());
  if (!std::holds_alternative<Image>(res.desc)) return;

  PhysicalImage& phys = images_.at(res.physical_id);
  destroy_views(phys);

  auto& [type, size, format] = std::get<Image>(res.desc);
  type = image.type();
  size = image.size();
  format = image.format();

  phys.state = state;
  phys.handle = image.image();
}

template<fwrk::BufferInterface I>
void fwrk::Context::update_buffer(const ResourceID resource, const I& buffer, const PhysicalState& state)
{
  if (!resource || resource.type() != ResourceType::Import || resource.type() == ResourceType::Transient) return;
  Resource& res = resources_.at(resource.index());
  if (!std::holds_alternative<Buffer>(res.desc)) return;

  auto& [size] = std::get<Buffer>(res.desc);
  size = buffer.size();

  PhysicalBuffer& phys = buffers_.at(res.physical_id);
  phys.state = state;
  phys.handle = buffer.buffer();
}
