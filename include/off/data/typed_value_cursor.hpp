#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// A bounded cursor over values that a schema-specific reader has already
// classified. This intentionally does not define a byte encoding for tags or
// values: the grammar belongs to the individual recovered source reader.
enum class TypedValueKind : std::uint8_t {
    scalar,
    integer,
    continuation,
    opaque_reference,
};

struct TypedValue {
    TypedValueKind kind{};
    std::uint32_t raw{};
    // This is a schema classifier result, rather than a retained byte tag.
    // The compact source encoding is deliberately outside this boundary.
    std::uint8_t schema_class{};
};

class TypedValueCursor final {
public:
    explicit TypedValueCursor(std::span<const TypedValue> values) : values_(values) {}

    [[nodiscard]] bool empty() const noexcept { return cursor_ == values_.size(); }
    [[nodiscard]] std::size_t remaining() const noexcept { return values_.size() - cursor_; }

    [[nodiscard]] const TypedValue* peek() const noexcept {
        return empty() ? nullptr : &values_[cursor_];
    }

    [[nodiscard]] std::uint8_t next_schema_class() const {
        if (empty()) fail();
        return values_[cursor_].schema_class;
    }
    void require_next_schema_class(std::uint8_t expected) const {
        if (next_schema_class() != expected) fail();
    }

    [[nodiscard]] std::uint32_t scalar_bits() { return consume(TypedValueKind::scalar).raw; }
    [[nodiscard]] float scalar() { return std::bit_cast<float>(scalar_bits()); }
    [[nodiscard]] std::uint32_t integer_bits() { return consume(TypedValueKind::integer).raw; }
    [[nodiscard]] std::int32_t integer() {
        return std::bit_cast<std::int32_t>(integer_bits());
    }
    [[nodiscard]] std::uint32_t opaque_reference() {
        return consume(TypedValueKind::opaque_reference).raw;
    }
    void continuation() { static_cast<void>(consume(TypedValueKind::continuation)); }
    void finish() const {
        if (!empty()) fail();
    }

private:
    [[nodiscard]] const TypedValue& consume(TypedValueKind expected) {
        if (empty() || values_[cursor_].kind != expected) fail();
        return values_[cursor_++];
    }
    [[noreturn]] static void fail() {
        throw std::runtime_error("typed value cursor does not match its supplied schema");
    }

    std::span<const TypedValue> values_;
    std::size_t cursor_{};
};

} // namespace off::data
