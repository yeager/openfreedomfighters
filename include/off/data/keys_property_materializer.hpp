#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>

namespace off::data {

// This is a parsed descriptor, not a BUF grammar.  The property-container
// encoding still needs independent recovery; callers must supply its bounded
// child records after parsing their supported container form.
struct OwnerAuxiliaryPropertyBlock final {
    std::uint64_t owner{};
    std::uint32_t buf_auxiliary_offset{};
    std::span<const std::byte> bytes{};
};

struct OwnerAuxiliaryPropertyChild final {
    std::uint32_t buf_auxiliary_offset{};
    std::array<char, 4> name{};
    std::size_t declared_extent{};
    std::span<const std::byte> bytes{};
};

// The resolver is the sole authority for a child identity's scene lifetime.
// It must return a pre-existing handle; this boundary never allocates or
// derives one from a BUF address, name, owner, or attachment ordinal.
struct SceneLifetimeKeysChildMapping final {
    std::uint64_t owner{};
    std::uint32_t buf_auxiliary_offset{};
    std::array<char, 4> name{};
    std::size_t declared_extent{};
    std::uint64_t opaque_handle{};
};

using SceneLifetimeKeysChildResolver = std::function<std::optional<SceneLifetimeKeysChildMapping>(
    const OwnerAuxiliaryPropertyBlock&, const OwnerAuxiliaryPropertyChild&)>;

struct MaterializedOwnerKeysChild final {
    std::uint64_t owner{};
    std::uint32_t buf_auxiliary_offset{};
    std::uint64_t opaque_handle{};
};

// Admits only the evidenced one-child owner-property shape for MatPosAnim:
// one owner-local KEYS child with a declared and bounded 48-byte extent.  The
// caller's source-record BUF auxiliary offset is the only join used here.
class KeysPropertyMaterializer final {
public:
    [[nodiscard]] static std::optional<MaterializedOwnerKeysChild> materialize(
        const OwnerAuxiliaryPropertyBlock& property,
        std::span<const OwnerAuxiliaryPropertyChild> children,
        const SceneLifetimeKeysChildResolver& resolve_scene_lifetime_child) {
        validate_property(property);
        if (children.size() != 1U) {
            fail("owner auxiliary property requires exactly one child");
        }

        const auto& child = children.front();
        if (child.buf_auxiliary_offset != property.buf_auxiliary_offset) {
            fail("owner auxiliary child does not join its source BUF offset");
        }
        if (child.name != keys_name) {
            fail("owner auxiliary property child is not KEYS");
        }
        if (child.declared_extent != keys_extent || child.bytes.size() != keys_extent) {
            fail("owner auxiliary KEYS child does not have its supported extent");
        }
        if (!is_subview(property.bytes, child.bytes)) {
            fail("owner auxiliary KEYS child is outside its bounded property block");
        }
        if (!resolve_scene_lifetime_child) {
            return std::nullopt;
        }

        const auto mapping = resolve_scene_lifetime_child(property, child);
        if (!mapping || mapping->owner != property.owner ||
            mapping->buf_auxiliary_offset != property.buf_auxiliary_offset ||
            mapping->name != child.name || mapping->declared_extent != child.declared_extent ||
            mapping->opaque_handle == 0U) {
            return std::nullopt;
        }
        return MaterializedOwnerKeysChild{
            .owner = property.owner,
            .buf_auxiliary_offset = property.buf_auxiliary_offset,
            .opaque_handle = mapping->opaque_handle,
        };
    }

private:
    static constexpr std::array<char, 4> keys_name{'K', 'E', 'Y', 'S'};
    static constexpr std::size_t keys_extent = 48U;

    static void validate_property(const OwnerAuxiliaryPropertyBlock& property) {
        if (property.owner == 0U || property.buf_auxiliary_offset == 0U || property.bytes.empty()) {
            fail("owner auxiliary property requires a live owner and explicit BUF offset");
        }
    }

    [[nodiscard]] static bool is_subview(std::span<const std::byte> outer,
                                         std::span<const std::byte> inner) noexcept {
        const auto outer_begin = reinterpret_cast<std::uintptr_t>(outer.data());
        const auto inner_begin = reinterpret_cast<std::uintptr_t>(inner.data());
        if (inner_begin < outer_begin) {
            return false;
        }
        const auto offset = inner_begin - outer_begin;
        return offset <= outer.size() && inner.size() <= outer.size() - offset;
    }

    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }
};

} // namespace off::data
