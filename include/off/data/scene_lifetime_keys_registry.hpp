#pragma once

#include "off/data/component_reader_context.hpp"
#include "off/data/matpos_owner_child_selector.hpp"
#include "off/data/owner_buf_keys_profile.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::data {

// An input names an already-created scene object. The registry does not create
// object handles: it only records the caller's nonzero scene-lifetime handle
// after validating the canonical owner-local BUF/KEYS profile.
struct SceneLifetimeKeysRegistryInput final {
    OwnerAuxiliaryPropertyBlock property{};
    std::uint64_t opaque_handle{};
};

// Immutable lookup table for the one recovered owner-local KEYS profile. It
// copies the canonical property bytes during construction so later changes to
// a loader's buffer cannot turn a registry entry into a stale alias.
class SceneLifetimeKeysRegistry final {
public:
    [[nodiscard]] static SceneLifetimeKeysRegistry construct(
        std::span<const SceneLifetimeKeysRegistryInput> inputs) {
        std::vector<Entry> staged;
        staged.reserve(inputs.size());

        // Validate every input before publishing any registry state. This is
        // intentionally a transaction boundary: a failing later record leaves
        // no partially constructed resolver for the scene.
        for (const auto& input : inputs) {
            if (input.property.owner == 0U || input.property.buf_auxiliary_offset == 0U ||
                input.property.bytes.empty() || input.opaque_handle == 0U) {
                fail("scene KEYS registry requires live canonical owners and handles");
            }

            Entry entry{};
            entry.owner = input.property.owner;
            entry.buf_auxiliary_offset = input.property.buf_auxiliary_offset;
            entry.opaque_handle = input.opaque_handle;
            entry.property_bytes.assign(input.property.bytes.begin(), input.property.bytes.end());

            const OwnerAuxiliaryPropertyBlock canonical{
                entry.owner, entry.buf_auxiliary_offset, entry.property_bytes};
            const auto child = OwnerBufKeysProfileParser::parse(canonical);
            if (child.name != matpos_owner_child_selector || child.declared_extent != keys_extent) {
                fail("scene KEYS registry received an unsupported canonical child");
            }
            staged.push_back(std::move(entry));
        }

        const auto same_identity = [](const Entry& left, const Entry& right) {
            return left.owner == right.owner &&
                   left.buf_auxiliary_offset == right.buf_auxiliary_offset;
        };
        for (std::size_t first = 0U; first < staged.size(); ++first) {
            for (std::size_t second = first + 1U; second < staged.size(); ++second) {
                if (same_identity(staged[first], staged[second])) {
                    fail("scene KEYS registry rejects duplicate owner-local KEYS identities");
                }
                if (staged[first].owner == staged[second].owner) {
                    fail("scene KEYS registry rejects ambiguous KEYS children for one owner");
                }
                if (staged[first].opaque_handle == staged[second].opaque_handle) {
                    fail("scene KEYS registry rejects a handle reused by another KEYS child");
                }
            }
        }

        return SceneLifetimeKeysRegistry(std::move(staged));
    }

    [[nodiscard]] std::optional<SceneLifetimeKeysChildMapping> resolve(
        const OwnerAuxiliaryPropertyBlock& property,
        const OwnerAuxiliaryPropertyChild& child) const noexcept {
        if (property.owner == 0U || property.buf_auxiliary_offset == 0U || property.bytes.empty() ||
            child.name != matpos_owner_child_selector || child.declared_extent != keys_extent ||
            child.buf_auxiliary_offset != property.buf_auxiliary_offset) {
            return std::nullopt;
        }

        const auto* entry = find(property.owner, property.buf_auxiliary_offset);
        if (entry == nullptr || !is_subview(property.bytes, child.bytes) ||
            !matches_canonical_property(*entry, property) ||
            !matches_canonical_child(*entry, child)) {
            return std::nullopt;
        }
        return SceneLifetimeKeysChildMapping{entry->owner, entry->buf_auxiliary_offset,
                                             matpos_owner_child_selector, keys_extent, entry->opaque_handle};
    }

    [[nodiscard]] std::optional<ComponentReaderKeysChild> resolve_required_keys(
        std::uint64_t owner, std::array<char, 4> key) const noexcept {
        if (owner == 0U || key != matpos_owner_child_selector) {
            return std::nullopt;
        }

        const auto iterator = std::find_if(entries_.begin(), entries_.end(), [owner](const Entry& entry) {
            return entry.owner == owner;
        });
        if (iterator == entries_.end()) {
            return std::nullopt;
        }
        return ComponentReaderKeysChild{iterator->owner, iterator->opaque_handle};
    }

    [[nodiscard]] SceneLifetimeKeysChildResolver materializer_resolver() const {
        return [this](const OwnerAuxiliaryPropertyBlock& property,
                      const OwnerAuxiliaryPropertyChild& child) { return resolve(property, child); };
    }

    [[nodiscard]] RequiredKeysChildResolver component_reader_resolver() const {
        return [this](std::uint64_t owner, std::array<char, 4> key) {
            return resolve_required_keys(owner, key);
        };
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

private:
    struct Entry final {
        std::uint64_t owner{};
        std::uint32_t buf_auxiliary_offset{};
        std::uint64_t opaque_handle{};
        std::vector<std::byte> property_bytes{};
    };

    explicit SceneLifetimeKeysRegistry(std::vector<Entry> entries) noexcept
        : entries_(std::move(entries)) {}

    [[nodiscard]] const Entry* find(std::uint64_t owner,
                                    std::uint32_t buf_auxiliary_offset) const noexcept {
        const auto iterator = std::find_if(entries_.begin(), entries_.end(),
                                           [owner, buf_auxiliary_offset](const Entry& entry) {
                                               return entry.owner == owner &&
                                                      entry.buf_auxiliary_offset == buf_auxiliary_offset;
                                           });
        return iterator == entries_.end() ? nullptr : &*iterator;
    }

    [[nodiscard]] static bool matches_canonical_property(const Entry& entry,
                                                           const OwnerAuxiliaryPropertyBlock& property) noexcept {
        return property.bytes.size() == entry.property_bytes.size() &&
               std::memcmp(property.bytes.data(), entry.property_bytes.data(), property.bytes.size()) == 0;
    }

    [[nodiscard]] static bool matches_canonical_child(const Entry& entry,
                                                        const OwnerAuxiliaryPropertyChild& child) noexcept {
        constexpr std::size_t child_offset = 16U;
        return child.bytes.size() == keys_extent && entry.property_bytes.size() >= child_offset + keys_extent &&
               std::memcmp(child.bytes.data(), entry.property_bytes.data() + child_offset, keys_extent) == 0;
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

    static constexpr std::size_t keys_extent = 48U;
    std::vector<Entry> entries_{};
};

} // namespace off::data
