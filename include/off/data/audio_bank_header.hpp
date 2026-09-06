#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>
#include <vector>

namespace off::data {

struct AudioStreamRecord {
    std::uint32_t format_flags{0};
    std::uint32_t sample_rate{0};
    std::uint32_t bits_per_sample{0};
    // Raw WHD word 5: meaningful decoded bytes on supported 16-bit output paths.
    std::uint32_t decoded_byte_count{0};
    std::uint32_t encoded_size{0};
    std::uint32_t channels{0};
    std::uint32_t data_offset{0};
    // Raw WHD word 9: interleaved sample values, not per-channel frames.
    std::uint32_t sample_value_count{0};
    std::uint32_t block_align{0};
    std::uint32_t samples_per_block{0};

    [[nodiscard]] bool uses_global_bank() const noexcept {
        return (format_flags & 0x80000000U) != 0;
    }
};

class AudioBankHeader final {
public:
    [[nodiscard]] static AudioBankHeader parse(std::span<const std::byte> bytes);

    [[nodiscard]] std::span<const AudioStreamRecord> records() const noexcept {
        return records_;
    }

    void validate_payload_ranges(
        std::uint64_t local_bank_size,
        std::uint64_t global_bank_size
    ) const;
    // Simple SND resource link: clear bit zero, then address the COMPLETE WHD
    // image. Zero is no request, not record zero. Reject interior/header/footer.
    [[nodiscard]] std::optional<std::size_t> record_index_for_sound_link(std::uint32_t link) const;

private:
    std::vector<AudioStreamRecord> records_;
};

}  // namespace off::data
