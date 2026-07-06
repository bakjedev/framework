#pragma once
#include <iostream>
#include <string>
#include <variant>

#include "buffer_resource.hpp"
#include "image_resource.hpp"
#include "util/flat_hash_map.hpp"

namespace fwrk {
  struct ResourceID {
    size_t id;

    ResourceID() : id(SIZE_MAX) {}
    explicit ResourceID(const size_t id_) : id(id_) {}
    explicit operator bool() const { return id != SIZE_MAX; }
    bool operator==(const ResourceID&) const = default;
  };

  enum class ResourceType : uint8_t { Import, Transient, Proxy };
  using ResourceDesc = std::variant<Image, Buffer>;

  struct Resource {
    ResourceType type;
    ResourceDesc desc;
    size_t physical_id;
    std::string name;

    [[nodiscard]] bool is_image() const { return desc.index() == 0; }
    [[nodiscard]] bool is_buffer() const { return desc.index() == 1; }
  };

  struct ResourceDependencies {
    std::vector<uint32_t> write_passes;
    std::vector<uint32_t> read_passes;
    flat_hash_map<uint32_t, uint32_t> read_deps;
  };
} // namespace fwrk

template<>
struct std::hash<fwrk::ResourceID> {
  size_t operator()(const fwrk::ResourceID& resource) const noexcept { return std::hash<size_t>{}(resource.id); }
};
