#include "off/data/zgf_bundle.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t zgf_magic = 0x5a474654U;
constexpr std::uint32_t root_flag = 0x80000000U;
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

void set_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes[offset + shift / 8] = static_cast<std::byte>((value >> shift) & 0xffU);
    }
}

void append_entry(
    std::vector<std::byte>& bytes,
    std::string_view name,
    std::string_view payload
) {
    const auto entry_start = bytes.size();
    append_u32(bytes, 1);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 1);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    const auto padded_payload_size = (payload.size() + 3U) & ~std::size_t{3};
    append_u32(bytes, static_cast<std::uint32_t>(8 + padded_payload_size));
    std::ranges::transform(payload, std::back_inserter(bytes), [](char value) {
        return static_cast<std::byte>(value);
    });
    bytes.resize(entry_start + 24 + padded_payload_size);
    const auto name_offset = bytes.size() - entry_start;
    std::ranges::transform(name, std::back_inserter(bytes), [](char value) {
        return static_cast<std::byte>(value);
    });
    bytes.push_back(std::byte{0});
    bytes.resize((bytes.size() + 3U) & ~std::size_t{3});
    const auto entry_size = bytes.size() - entry_start;
    set_u32(bytes, entry_start + 4, root_flag | static_cast<std::uint32_t>(entry_size));
    set_u32(bytes, entry_start + 8, static_cast<std::uint32_t>(name_offset));
}

std::vector<std::byte> decoded_fixture() {
    std::vector<std::byte> bytes;
    append_u32(bytes, zgf_magic);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 2);
    append_entry(bytes, "Scenes/../Shared/First.bin", "first");
    append_entry(bytes, "Scenes\\Test\\Second.bin", "second payload");
    set_u32(bytes, 4, root_flag | static_cast<std::uint32_t>(bytes.size()));
    set_u32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    return bytes;
}

std::vector<std::byte> packed_stored(std::vector<std::byte> payload) {
    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size() + 9));
    bytes.push_back(std::byte{1});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

template <typename Mutation>
void check_rejected(Mutation mutation, const char* message) {
    auto payload = decoded_fixture();
    mutation(payload);
    bool rejected = false;
    try {
        static_cast<void>(off::data::ZgfBundle::parse(
            off::data::PackedResource::parse(packed_stored(std::move(payload)))
        ));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

}  // namespace

int main() {
    const auto bundle = off::data::ZgfBundle::parse(
        off::data::PackedResource::parse(packed_stored(decoded_fixture()))
    );
    check(bundle.entries().size() == 2, "parse ZGF entry count");
    check(bundle.entries()[0].name == "Scenes/../Shared/First.bin", "parse ZGF entry name");
    check(bundle.entries()[1].payload_size == 14, "parse ZGF payload size");
    const auto second_payload = bundle.entry_payload(1);
    check(
        std::equal(
            second_payload.begin(),
            second_payload.end(),
            std::string_view{"second payload"}.begin(),
            [](std::byte left, char right) { return left == static_cast<std::byte>(right); }
        ),
        "expose a bounded ZGF entry payload"
    );
    bool rejected_index = false;
    try {
        static_cast<void>(bundle.entry_payload(2));
    } catch (const std::runtime_error&) {
        rejected_index = true;
    }
    check(rejected_index, "reject an out-of-range ZGF entry index");

    std::vector<std::byte> empty;
    append_u32(empty, zgf_magic);
    append_u32(empty, 8);
    const auto empty_bundle = off::data::ZgfBundle::parse(
        off::data::PackedResource::parse(packed_stored(std::move(empty)))
    );
    check(empty_bundle.entries().empty(), "parse an empty ZGF bundle");

    check_rejected(
        [](auto& value) { set_u32(value, 0, 0); },
        "reject an invalid ZGF signature"
    );
    check_rejected(
        [](auto& value) { set_u32(value, 8, static_cast<std::uint32_t>(value.size() - 4)); },
        "reject a mismatched ZGF root size"
    );
    check_rejected(
        [](auto& value) { set_u32(value, 16, 2); },
        "reject an unknown ZGF entry type"
    );
    check_rejected(
        [](auto& value) { set_u32(value, 32, 0); },
        "reject an empty embedded payload"
    );
    check_rejected(
        [](auto& value) { value[45] = std::byte{1}; },
        "reject nonzero embedded-payload padding"
    );
    check_rejected(
        [](auto& value) {
            const auto name_offset = 16U + (static_cast<std::uint32_t>(value[24]) & 0xffU);
            value[name_offset] = std::byte{0};
        },
        "reject an empty entry name"
    );
    check_rejected(
        [](auto& value) {
            const auto name_offset = 16U + (static_cast<std::uint32_t>(value[24]) & 0xffU);
            value[name_offset] = std::byte{'/'};
        },
        "reject an absolute entry name"
    );

    return failures == 0 ? 0 : 1;
}
