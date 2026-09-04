#include "off/data/audio_bank_header.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
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

void append_record(
    std::vector<std::byte>& bytes,
    std::uint32_t flags,
    std::uint32_t size,
    std::uint32_t offset
) {
    for (const auto value : {
             6U, 0U, flags, 22'050U, 4U, 1'000U, size, 1U, offset, 1'000U, 512U, 1'017U,
         }) {
        append_u32(bytes, value);
    }
}

std::vector<std::byte> make_header() {
    std::vector<std::byte> bytes;
    append_u32(bytes, 112);
    append_u32(bytes, 120);
    append_u32(bytes, 3);
    append_u32(bytes, 4);
    append_record(bytes, 17, 64, 32);
    append_record(bytes, 0x80000011U, 50, 100);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    return bytes;
}

}  // namespace

int main() {
    const auto fixture = make_header();
    const auto header = off::data::AudioBankHeader::parse(fixture);
    check(header.records().size() == 2, "parse synthetic WHD records");
    check(!header.records()[0].uses_global_bank(), "identify a local-bank record");
    check(header.records()[1].uses_global_bank(), "identify a global-bank record");
    header.validate_payload_ranges(96, 150);

    bool range_rejected = false;
    try {
        header.validate_payload_ranges(95, 150);
    } catch (const std::runtime_error&) {
        range_rejected = true;
    }
    check(range_rejected, "reject a WHD range beyond its bank");

    auto truncated = fixture;
    truncated.pop_back();
    bool truncated_rejected = false;
    try {
        static_cast<void>(off::data::AudioBankHeader::parse(truncated));
    } catch (const std::runtime_error&) {
        truncated_rejected = true;
    }
    check(truncated_rejected, "reject a truncated WHD table");

    auto bad_marker = fixture;
    bad_marker[16] = std::byte{7};
    bool marker_rejected = false;
    try {
        static_cast<void>(off::data::AudioBankHeader::parse(bad_marker));
    } catch (const std::runtime_error&) {
        marker_rejected = true;
    }
    check(marker_rejected, "reject an invalid WHD record marker");

    return failures == 0 ? 0 : 1;
}
