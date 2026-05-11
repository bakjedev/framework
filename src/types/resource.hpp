#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_set>
#include <string>
#include <optional>

namespace passgraph {
    enum class ResourceType : uint8_t {
        Image,
        Buffer,
    };

    struct ResourceID {
        std::optional<uint32_t> id;

        ResourceID() = default;

        explicit ResourceID(const size_t id_) : id(static_cast<uint32_t>(id_)) {
        }

        explicit operator bool() const { return id.has_value(); }
    };

    struct Resource {
        ResourceType type;
        uint32_t slot;
        uint32_t raw;
        std::string name;
        std::unordered_set<uint32_t> write_passes;
        std::unordered_set<uint32_t> read_passes;
        uint32_t last_writer = 0;

        Resource(const ResourceType type_, const size_t id_, const size_t raw_, std::string name_) : type(type_),
            slot(static_cast<uint32_t>(id_)), raw(static_cast<uint32_t>(raw_)), name(std::move(name_)) {
        }
    };
}
