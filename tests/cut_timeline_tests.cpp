#include "off/cutscene/timeline_position.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <iostream>

namespace {
int failures = 0;

void check(float actual, float expected) {
    if (std::bit_cast<std::uint32_t>(actual) != std::bit_cast<std::uint32_t>(expected)) {
        std::cerr << "FAIL: explicit-clock timeline bit mismatch\n";
        ++failures;
    }
}

// Independent unsigned modular formulation: the exact product/shift reduces to
// multiplication by 25 modulo 2^32. Decode sign by subtraction, not bit casting.
float oracle(std::uint32_t current, std::uint32_t start) {
    const auto word = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(current - start) * 25U);
    const auto signed_value = word < 0x80000000U ? static_cast<std::int64_t>(word) :
        static_cast<std::int64_t>(word) - 0x100000000LL;
    return static_cast<float>(signed_value) / 1024.0F;
}
}

int main() {
    using off::cutscene::timeline_position;
    static_assert(noexcept(timeline_position(0U, 0U)));
    check(timeline_position(0, 0), 0.0F);
    check(timeline_position(1, 0), 0.0244140625F);
    check(timeline_position(0, 1), -0.0244140625F);
    check(timeline_position(0, 0xffffffffU), 0.0244140625F);
    check(timeline_position(0xffffffffU, 0), -0.0244140625F);
    check(timeline_position(0x80000000U, 0), -2097152.0F);
    check(timeline_position(0x7fffffffU, 0), 2097152.0F);
    check(timeline_position(85899345U, 0), 2097152.0F);
    check(timeline_position(85899346U, 0), -2097152.0F);
    check(timeline_position(171798692U, 0), 0.00390625F);
    check(timeline_position(671089U, 0), 16384.0078125F);
    constexpr std::array<std::uint32_t, 12> words{
        0, 1, 2, 0xffffffffU, 0x80000000U, 0x7fffffffU,
        85899345U, 85899346U, 171798692U, 671089U, 0x12345678U, 0xfedcba98U};
    for (const auto current : words) {
        for (const auto start : words) check(timeline_position(current, start), oracle(current, start));
    }
    std::uint32_t state = 0x31415926U;
    for (unsigned i = 0; i < 10000U; ++i) {
        state = state * 1664525U + 1013904223U;
        const auto current = state;
        state = state * 1664525U + 1013904223U;
        check(timeline_position(current, state), oracle(current, state));
    }
    return failures == 0 ? 0 : 1;
}
