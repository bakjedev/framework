#pragma once
#include <cassert>
#include <iostream>
#include <string>
#include <variant>

#include "buffer_resource.hpp"
#include "image_resource.hpp"
#include "util/flat_hash_map.hpp"

namespace fwrk {
  enum class ResourceType : uint64_t { Import = 0, Transient = 1, Proxy = 2 };

  class ResourceID {
    uint64_t bits;

    static constexpr uint64_t TypeShift = 62;
    static constexpr uint64_t IdMask = (1ull << TypeShift) - 1;

  public:
    [[nodiscard]] ResourceType type() const { return static_cast<ResourceType>(bits >> TypeShift); }
    [[nodiscard]] uint64_t index() const { return bits & IdMask; }
    [[nodiscard]] uint64_t raw() const { return bits; }
    [[nodiscard]] bool valid() const { return bits != UINT64_MAX; }

    ResourceID() : bits(UINT64_MAX) {}
    explicit ResourceID(const ResourceType type, const uint64_t id) :
        bits(static_cast<uint64_t>(type) << TypeShift | id)
    {
      assert(id <= IdMask && "index overflows into type field");
    }
    explicit operator bool() const { return valid(); }
    bool operator==(const ResourceID&) const = default;
  };

  using ResourceDesc = std::variant<Image, Buffer>;

  struct Resource {
    ResourceDesc desc;
    uint64_t physical_id;
    std::string name;
  };

  struct ResourceDependencies {
    std::vector<uint32_t> write_passes;
    std::vector<uint32_t> read_passes;
    flat_hash_map<uint32_t, uint32_t> read_deps;
  };
} // namespace fwrk

template<>
struct std::hash<fwrk::ResourceID> {
  size_t operator()(const fwrk::ResourceID& resource) const noexcept { return std::hash<size_t>{}(resource.raw()); }
};
