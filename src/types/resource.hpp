#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>

namespace passgraph {
  enum class ResourceType : uint8_t {
    Image,
    Buffer,
  };

  struct ResourceID {
    std::optional<uint32_t> id;

    ResourceID() = default;

    explicit ResourceID(const size_t id_) : id(static_cast<uint32_t>(id_)) {}

    explicit operator bool() const { return id.has_value(); }
    bool operator==(const ResourceID&) const = default;
  };

  struct Resource {
    ResourceType type;
    uint32_t slot;
    uint32_t raw;
    std::string name;

    Resource(const ResourceType type_, const size_t slot_, const size_t raw_, std::string name_) :
        type(type_), slot(static_cast<uint32_t>(slot_)), raw(static_cast<uint32_t>(raw_)), name(std::move(name_))
    {
    }
  };
} // namespace passgraph

template<>
struct std::hash<passgraph::ResourceID> {
  size_t operator()(const passgraph::ResourceID& resource) const noexcept
  {
    return resource.id ? std::hash<uint32_t>{}(*resource.id) : 0;
  }
};
