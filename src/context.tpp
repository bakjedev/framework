#pragma once

template<passgraph::ImageInterface I>
passgraph::ResourceID passgraph::Context::import_image(const I& image, const ImageState& state, std::string name)
{
  const auto slot_id = images_.size();

  uint32_t x = 0, y = 0, z = 0;
  if constexpr (requires {
                  image.x();
                  image.y();
                  image.z();
                }) {
    x = image.x();
    y = image.y();
    z = image.z();
  } else {
    const auto [size_x, size_y, size_z] = image.size();
    x = size_x;
    y = size_y;
    z = size_z;
  }
  images_.emplace_back(x, y, z, image.format(), image.usage(), image.aspect(), state);

  const auto raw_id = raw_images_.size();
  raw_images_.push_back(image.image());
  raw_image_views_.push_back(image.view());

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Image, slot_id, raw_id, std::move(name));

  return ResourceID{id};
}

template<passgraph::BufferInterface I>
passgraph::ResourceID passgraph::Context::import_buffer(const I& buffer, const BufferState& state, std::string name)
{
  const auto slot_id = buffers_.size();
  buffers_.emplace_back(buffer.size(), buffer.usage(), state);

  const auto raw_id = raw_buffers_.size();
  raw_buffers_.push_back(buffer.buffer());

  const auto id = resources_.size();
  resources_.emplace_back(ResourceType::Buffer, slot_id, raw_id, std::move(name));

  return ResourceID{id};
}
