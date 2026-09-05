#include "off/audio/decode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

off::data::AudioStreamRecord record(
    std::uint32_t flags,
    std::uint32_t encoded_size,
    std::uint32_t channels,
    std::uint32_t block_align,
    std::uint32_t samples_per_block
) {
    return {
        .format_flags = flags,
        .sample_rate = 22'050,
        .bits_per_sample = flags == 1 ? 16U : 4U,
        .encoded_size = encoded_size,
        .channels = channels,
        .block_align = block_align,
        .samples_per_block = samples_per_block,
    };
}

void append_page(std::vector<std::byte>& bytes, const ogg_page& page) {
    const auto header = std::as_bytes(std::span{page.header, static_cast<std::size_t>(page.header_len)});
    const auto body = std::as_bytes(std::span{page.body, static_cast<std::size_t>(page.body_len)});
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.insert(bytes.end(), body.begin(), body.end());
}

std::vector<std::byte> encode_synthetic_vorbis() {
    vorbis_info info{};
    vorbis_info_init(&info);
    if (vorbis_encode_init_vbr(&info, 2, 44'100, 0.1F) != 0) {
        throw std::runtime_error("could not initialize synthetic Vorbis encoder");
    }
    vorbis_comment comment{};
    vorbis_comment_init(&comment);
    char encoder_tag[] = "ENCODER";
    char encoder_value[] = "synthetic-test";
    vorbis_comment_add_tag(&comment, encoder_tag, encoder_value);
    vorbis_dsp_state state{};
    vorbis_block block{};
    ogg_stream_state stream{};
    if (vorbis_analysis_init(&state, &info) != 0 || vorbis_block_init(&state, &block) != 0 ||
        ogg_stream_init(&stream, 0x4f4646) != 0) {
        throw std::runtime_error("could not initialize synthetic Ogg stream");
    }

    ogg_packet identification{};
    ogg_packet comments{};
    ogg_packet setup{};
    if (vorbis_analysis_headerout(&state, &comment, &identification, &comments, &setup) != 0) {
        throw std::runtime_error("could not create synthetic Vorbis headers");
    }
    ogg_stream_packetin(&stream, &identification);
    ogg_stream_packetin(&stream, &comments);
    ogg_stream_packetin(&stream, &setup);

    std::vector<std::byte> encoded;
    ogg_page page{};
    while (ogg_stream_flush(&stream, &page) != 0) {
        append_page(encoded, page);
    }

    constexpr int synthetic_frames = 256;
    auto** samples = vorbis_analysis_buffer(&state, synthetic_frames);
    for (int channel = 0; channel < 2; ++channel) {
        std::fill_n(samples[channel], synthetic_frames, 0.0F);
    }
    vorbis_analysis_wrote(&state, synthetic_frames);
    vorbis_analysis_wrote(&state, 0);

    bool end_of_stream = false;
    while (!end_of_stream && vorbis_analysis_blockout(&state, &block) == 1) {
        vorbis_analysis(&block, nullptr);
        vorbis_bitrate_addblock(&block);
        ogg_packet packet{};
        while (vorbis_bitrate_flushpacket(&state, &packet) != 0) {
            ogg_stream_packetin(&stream, &packet);
            while (ogg_stream_pageout(&stream, &page) != 0) {
                append_page(encoded, page);
                end_of_stream = ogg_page_eos(&page) != 0;
            }
        }
    }

    ogg_stream_clear(&stream);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&state);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    if (!end_of_stream) {
        throw std::runtime_error("synthetic Vorbis stream has no end page");
    }
    return encoded;
}

}  // namespace

int main() {
    const std::array pcm_bytes{
        std::byte{0x00}, std::byte{0x80}, std::byte{0xff}, std::byte{0xff},
        std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0x7f},
    };
    const auto pcm = off::audio::decode_stream(record(1, 8, 1, 2, 1), pcm_bytes);
    check(
        pcm.interleaved_samples == std::vector<std::int16_t>{-32'768, -1, 0, 32'767},
        "decode signed little-endian PCM"
    );
    check(pcm.frame_count() == 4, "report decoded PCM frames");

    const std::array mono_block{
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x11}, std::byte{0x11}, std::byte{0x11}, std::byte{0x11},
    };
    const auto mono = off::audio::decode_stream(record(0x80000011U, 8, 1, 8, 9), mono_block);
    check(
        mono.interleaved_samples == std::vector<std::int16_t>{0, 1, 2, 3, 4, 5, 6, 7, 8},
        "decode low-nibble-first mono IMA ADPCM"
    );

    const std::array stereo_block{
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{100}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x11}, std::byte{0x11}, std::byte{0x11}, std::byte{0x11},
        std::byte{0x99}, std::byte{0x99}, std::byte{0x99}, std::byte{0x99},
    };
    const auto stereo = off::audio::decode_stream(record(0x11, 16, 2, 16, 9), stereo_block);
    check(stereo.frame_count() == 9, "report decoded stereo IMA ADPCM frames");
    check(
        stereo.interleaved_samples == std::vector<std::int16_t>{
            0, 100, 1, 99, 2, 98, 3, 97, 4, 96, 5, 95, 6, 94, 7, 93, 8, 92,
        },
        "interleave stereo IMA ADPCM channel groups"
    );

    auto invalid_index = mono_block;
    invalid_index[2] = std::byte{89};
    bool index_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(
            record(0x80000011U, 8, 1, 8, 9),
            invalid_index
        ));
    } catch (const std::runtime_error&) {
        index_rejected = true;
    }
    check(index_rejected, "reject an invalid IMA ADPCM step index");

    bool layout_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(record(0x11, 8, 1, 8, 8), mono_block));
    } catch (const std::runtime_error&) {
        layout_rejected = true;
    }
    check(layout_rejected, "reject inconsistent IMA ADPCM block metadata");

    bool zero_channels_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(record(1, 8, 0, 0, 1), pcm_bytes));
    } catch (const std::runtime_error&) {
        zero_channels_rejected = true;
    }
    check(zero_channels_rejected, "reject zero-channel PCM without division by zero");

    const auto vorbis_bytes = encode_synthetic_vorbis();
    auto vorbis_record = record(
        0x80001000U,
        static_cast<std::uint32_t>(vorbis_bytes.size()),
        2,
        4,
        1
    );
    vorbis_record.sample_rate = 44'100;
    vorbis_record.bits_per_sample = 16;
    const auto vorbis = off::audio::decode_stream(vorbis_record, vorbis_bytes);
    check(vorbis.encoding == off::audio::Encoding::vorbis, "identify Ogg Vorbis audio");
    check(vorbis.frame_count() == 256, "decode all synthetic Vorbis frames");
    check(
        std::all_of(
            vorbis.interleaved_samples.begin(),
            vorbis.interleaved_samples.end(),
            [](std::int16_t sample) { return sample == 0; }
        ),
        "decode synthetic Vorbis silence"
    );
    auto corrupt_vorbis = vorbis_bytes;
    corrupt_vorbis.front() = std::byte{'X'};
    bool corrupt_vorbis_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(vorbis_record, corrupt_vorbis));
    } catch (const std::runtime_error&) {
        corrupt_vorbis_rejected = true;
    }
    check(corrupt_vorbis_rejected, "reject a corrupt Ogg capture pattern");

    auto wrong_rate = vorbis_record;
    wrong_rate.sample_rate = 22'050;
    bool wrong_rate_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(wrong_rate, vorbis_bytes));
    } catch (const std::runtime_error&) {
        wrong_rate_rejected = true;
    }
    check(wrong_rate_rejected, "reject Vorbis metadata that disagrees with WHD");

    bool unknown_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(record(0x80001234U, 8, 2, 4, 1), pcm_bytes));
    } catch (const std::runtime_error&) {
        unknown_rejected = true;
    }
    check(unknown_rejected, "reject an unverified encoding family");

    return failures == 0 ? 0 : 1;
}
