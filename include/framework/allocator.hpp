#pragma once
#include "types/buffer_resource.hpp"
#include "types/image_resource.hpp"

namespace fwrk {
  struct Allocator {
    virtual ~Allocator() = default;
    virtual std::optional<PhysicalImage> create_image(const ImageCreateInfo&) = 0;
    virtual std::optional<PhysicalBuffer> create_buffer(const BufferCreateInfo&) = 0;
    virtual void destroy_image(PhysicalImage&) = 0;
    virtual void destroy_buffer(PhysicalBuffer&) = 0;
  };
} // namespace fwrk
