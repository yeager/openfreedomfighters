#pragma once

#include "off/data/matpos_owner_child_selector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace off::data {

// This is deliberately an opaque reader input.  The owner-specific loader
// decides what it denotes; this boundary must not imply that it is a GMS byte
// range (or any other particular serialized representation).
class ComponentReaderInput final {
public:
    explicit ComponentReaderInput(const void* opaque_context) noexcept
        : opaque_context_(opaque_context) {}

    [[nodiscard]] const void* opaque_context() const noexcept { return opaque_context_; }

private:
    const void* opaque_context_{};
};

// Attachment identity is provenance for the component invocation. It is not
// a second lookup key: KEYS belongs to the component's bound owner.
struct ComponentReaderIdentity final {
    std::uint64_t owner{};
    std::size_t attachment_index{};

    constexpr bool operator==(const ComponentReaderIdentity&) const = default;
};

// A KEYS child is intentionally not interpreted here.  Zero is not a bound
// native child handle and is rejected before a concrete reader is entered.
struct ComponentReaderKeysChild final {
    std::uint64_t owner{};
    std::uint64_t opaque_handle{};
};

using RequiredKeysChildResolver = std::function<std::optional<ComponentReaderKeysChild>(
    std::uint64_t owner, std::array<char, 4> key)>;
using ComponentReader = std::function<void(const ComponentReaderInput&, std::uint64_t keys_handle)>;

enum class ComponentReaderInvocationResult : std::uint8_t {
    invoked,
    absent,
    unbound,
    wrong_owner,
};

// Resolves exactly one required KEYS child for the bound owner. The concrete
// reader is called only after the response binds to that same owner and
// supplies a nonzero opaque child handle.
class ComponentReaderContext final {
public:
    [[nodiscard]] static ComponentReaderInvocationResult invoke_required_keys(
        const ComponentReaderIdentity& identity,
        const ComponentReaderInput& input,
        const RequiredKeysChildResolver& resolve_keys_child,
        const ComponentReader& reader) {
        if (!resolve_keys_child) {
            return ComponentReaderInvocationResult::absent;
        }

        const auto child = resolve_keys_child(identity.owner, matpos_owner_child_selector);
        if (!child) {
            return ComponentReaderInvocationResult::absent;
        }
        if (child->owner != identity.owner) {
            return ComponentReaderInvocationResult::wrong_owner;
        }
        if (child->opaque_handle == 0) {
            return ComponentReaderInvocationResult::unbound;
        }
        if (!reader) {
            return ComponentReaderInvocationResult::absent;
        }

        reader(input, child->opaque_handle);
        return ComponentReaderInvocationResult::invoked;
    }
private:
};

} // namespace off::data
