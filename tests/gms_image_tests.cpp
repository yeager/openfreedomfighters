#include "off/data/gms_image.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
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

std::vector<std::byte> packed_fixture() {
    std::vector<std::byte> payload(64);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<std::byte>(index);
    }
    std::vector<std::byte> bytes;
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size() + 9));
    bytes.push_back(std::byte{1});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

template <typename Operation>
void check_rejected(Operation operation, const char* message) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

}  // namespace

int main() {
    const auto image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(packed_fixture())
    );
    check(image.decoded_size() == 64, "retain the decoded GMS image");
    const auto beginning = image.resolve_reference(0x40000000U, 4);
    check(beginning.size() == 4 && beginning[3] == std::byte{3},
          "resolve a tagged zero-offset GMS reference");
    const auto middle = image.resolve_reference(0x40000020U, 8);
    check(middle.size() == 8 && middle.front() == std::byte{32},
          "resolve a tagged GMS reference");

    check_rejected(
        [&image] { static_cast<void>(image.resolve_reference(0, 4)); },
        "reject a null reference"
    );
    check_rejected(
        [&image] { static_cast<void>(image.resolve_reference(0x80000020U, 4)); },
        "reject an unsupported reference tag"
    );
    check_rejected(
        [&image] { static_cast<void>(image.resolve_reference(0x40000040U, 1)); },
        "reject a reference at the end of the image"
    );
    check_rejected(
        [&image] { static_cast<void>(image.resolve_reference(0x40000020U, 0)); },
        "reject an empty reference view"
    );

    return failures == 0 ? 0 : 1;
}
