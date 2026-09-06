#include "off/data/audio_bank_header.hpp"

#include "off/data/byte_reader.hpp"

#include <limits>
#include <stdexcept>

namespace off::data {
namespace {

constexpr std::size_t header_size = 16;
constexpr std::size_t record_size = 48;
constexpr std::size_t footer_size = 8;
constexpr std::size_t maximum_record_count = 1'000'000;

}  // namespace

AudioBankHeader AudioBankHeader::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size + footer_size ||
        bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("WHD file size is invalid");
    }

    const ByteReader reader(bytes);
    if (reader.u32(0) != bytes.size() - footer_size || reader.u32(4) != bytes.size() ||
        reader.u32(8) != 3 || reader.u32(12) != 4) {
        throw std::runtime_error("WHD header does not match the supported layout");
    }
    const auto records_size = bytes.size() - header_size - footer_size;
    if (records_size % record_size != 0) {
        throw std::runtime_error("WHD record table is truncated");
    }
    const auto record_count = records_size / record_size;
    if (record_count > maximum_record_count) {
        throw std::runtime_error("WHD record count exceeds the safety limit");
    }
    if (reader.u32(bytes.size() - footer_size) != 0 || reader.u32(bytes.size() - 4) != 0) {
        throw std::runtime_error("WHD footer is invalid");
    }

    AudioBankHeader header;
    header.records_.reserve(record_count);
    for (std::size_t index = 0; index < record_count; ++index) {
        const auto offset = header_size + index * record_size;
        if (reader.u32(offset) != 6 || reader.u32(offset + 4) != 0) {
            throw std::runtime_error("WHD record marker is invalid");
        }
        AudioStreamRecord record{
            .format_flags = reader.u32(offset + 8),
            .sample_rate = reader.u32(offset + 12),
            .bits_per_sample = reader.u32(offset + 16),
            .decoded_byte_count = reader.u32(offset + 20),
            .encoded_size = reader.u32(offset + 24),
            .channels = reader.u32(offset + 28),
            .data_offset = reader.u32(offset + 32),
            .sample_value_count = reader.u32(offset + 36),
            .block_align = reader.u32(offset + 40),
            .samples_per_block = reader.u32(offset + 44),
        };
        if (record.sample_rate == 0 || record.sample_rate > 384'000 ||
            record.bits_per_sample == 0 || record.bits_per_sample > 64 ||
            record.channels == 0 || record.channels > 8 || record.encoded_size == 0 ||
            record.block_align == 0 || record.samples_per_block == 0) {
            throw std::runtime_error("WHD record contains invalid stream metadata");
        }
        header.records_.push_back(record);
    }
    return header;
}

void AudioBankHeader::validate_payload_ranges(
    std::uint64_t local_bank_size,
    std::uint64_t global_bank_size
) const {
    for (const auto& record : records_) {
        const auto bank_size = record.uses_global_bank() ? global_bank_size : local_bank_size;
        if (record.data_offset > bank_size || record.encoded_size > bank_size - record.data_offset) {
            throw std::runtime_error("WHD stream range exceeds its audio bank");
        }
    }
}

std::optional<std::size_t> AudioBankHeader::record_index_for_sound_link(std::uint32_t link) const {
    const auto offset=static_cast<std::size_t>(link & 0xfffffffeU);
    if(offset==0) return std::nullopt;
    if(offset<header_size || (offset-header_size)%record_size!=0 ||
       (offset-header_size)/record_size>=records_.size())
        throw std::runtime_error("SND resource link does not address a WHD record boundary");
    return (offset-header_size)/record_size;
}

}  // namespace off::data
