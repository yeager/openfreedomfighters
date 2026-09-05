#include "off/data/scene_support.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
}

void append_text(std::vector<std::byte>& bytes, std::string_view value) {
    const auto source = std::as_bytes(std::span{value.data(), value.size()});
    bytes.insert(bytes.end(), source.begin(), source.end());
    bytes.push_back(std::byte{0});
}

void set_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes[offset + shift / 8] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

std::vector<std::byte> scalar_fixture() {
    constexpr std::string_view name = "fixture.dlc";
    const auto size = static_cast<std::uint32_t>(24 + name.size() + 1);
    std::vector<std::byte> bytes;
    append_u32(bytes, 0);
    append_u32(bytes, 0x80000000U | size);
    append_u32(bytes, size);
    append_u32(bytes, 1);
    append_u32(bytes, 0x46434c44U);
    append_u32(bytes, size - 16);
    append_text(bytes, name);
    return bytes;
}

std::vector<std::byte> array_fixture() {
    constexpr std::string_view first = "base.test";
    constexpr std::string_view second = "render.test";
    constexpr std::uint32_t count = 3;
    constexpr std::uint32_t data_offset = 16 + count * 4;
    std::vector<std::byte> bytes(16, std::byte{0});
    append_u32(bytes, 0x46434c44U);
    append_u32(bytes, 0);
    append_u32(bytes, data_offset);
    append_u32(bytes, count);
    append_u32(bytes, static_cast<std::uint32_t>(first.size() + 1));
    append_u32(bytes, 1);
    append_u32(bytes, static_cast<std::uint32_t>(second.size() + 1));
    append_text(bytes, first);
    append_text(bytes, "");
    append_text(bytes, second);
    while ((bytes.size() & 3U) != 0U) {
        bytes.push_back(std::byte{0});
    }
    const auto size = static_cast<std::uint32_t>(bytes.size());
    set_u32(bytes, 4, 0x80000000U | size);
    set_u32(bytes, 8, size);
    set_u32(bytes, 12, 1);
    set_u32(bytes, 20, 0x40000000U | (size - 16));
    return bytes;
}

template <typename Mutation>
void check_rejected(Mutation mutate, const char* message) {
    auto bytes = array_fixture();
    mutate(bytes);
    bool rejected = false;
    try {
        static_cast<void>(off::data::SceneSupport::parse(bytes));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

}  // namespace

int main() {
    const auto scalar = off::data::SceneSupport::parse(scalar_fixture());
    check(scalar.dependencies().size() == 1, "parse scalar dependency layout");
    check(scalar.dependencies()[0] == "fixture.dlc", "decode scalar dependency name");

    const auto array = off::data::SceneSupport::parse(array_fixture());
    check(array.dependencies().size() == 3, "parse dependency-array layout");
    check(array.dependencies()[0] == "base.test", "decode first array dependency");
    check(array.dependencies()[1].empty(), "preserve an empty dependency slot");
    check(array.dependencies()[2] == "render.test", "decode final array dependency");

    check_rejected([](auto& bytes) { set_u32(bytes, 8, 1); }, "reject root size mismatch");
    check_rejected([](auto& bytes) { set_u32(bytes, 16, 0); }, "reject missing DLCF block");
    check_rejected(
        [](auto& bytes) { set_u32(bytes, 20, 0xc0000000U | (bytes.size() - 16)); },
        "reject unobserved block flags"
    );
    check_rejected([](auto& bytes) { set_u32(bytes, 24, 4); }, "reject invalid data offset");
    check_rejected([](auto& bytes) { set_u32(bytes, 32, 0); }, "reject zero string length");
    check_rejected(
        [](auto& bytes) { bytes[32 + 12 + 9] = std::byte{'X'}; },
        "reject missing string terminator"
    );
    check_rejected(
        [](auto& bytes) { bytes.back() = std::byte{1}; },
        "reject nonzero alignment padding"
    );

    auto truncated = array_fixture();
    truncated.resize(20);
    bool truncated_rejected = false;
    try {
        static_cast<void>(off::data::SceneSupport::parse(truncated));
    } catch (const std::runtime_error&) {
        truncated_rejected = true;
    }
    check(truncated_rejected, "reject truncated scene-support data");
    return failures == 0 ? 0 : 1;
}
