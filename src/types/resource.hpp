#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>

namespace fwrk {
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

  struct ResourceDependencies {
    std::vector<uint32_t> write_passes;
    std::vector<uint32_t> read_passes;
    std::unordered_map<uint32_t, uint32_t> read_deps;
  };
} // namespace fwrk

template<>
struct std::hash<fwrk::ResourceID> {
  size_t operator()(const fwrk::ResourceID& resource) const noexcept
  {
    return resource.id ? std::hash<uint32_t>{}(*resource.id) : 0;
  }
};
