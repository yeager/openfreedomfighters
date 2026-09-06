#include "off/graphics/intro_prepared_resources.hpp"
#include "off/data/packed_resource.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using Bytes = std::vector<std::byte>;
int failures = 0;
void check(bool condition, const char* text) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported intro preparation rejects");
}
void word(Bytes& b, std::uint32_t v) {
    for (unsigned s = 0; s < 32; s += 8) b.push_back(static_cast<std::byte>((v >> s) & 255));
}
void set(Bytes& b, std::size_t at, std::uint32_t v) {
    for (unsigned s = 0; s < 32; s += 8) b.at(at++) = static_cast<std::byte>((v >> s) & 255);
}
void text(Bytes& b, std::string_view value) {
    for (char c : value) b.push_back(static_cast<std::byte>(c));
    b.push_back(std::byte{0});
}
void scalar(Bytes& b, std::uint8_t tag, std::uint32_t v) {
    b.push_back(static_cast<std::byte>(tag)); word(b, v);
}
void floating(Bytes& b, std::uint8_t tag, float v) { scalar(b, tag, std::bit_cast<std::uint32_t>(v)); }
void real(Bytes& b, std::uint8_t tag, double v) {
    b.push_back(static_cast<std::byte>(tag));
    const auto bits = std::bit_cast<std::uint64_t>(v);
    word(b, static_cast<std::uint32_t>(bits)); word(b, static_cast<std::uint32_t>(bits >> 32));
}
void finish(Bytes& b) { b.push_back(std::byte{0xff}); set(b, 0, static_cast<std::uint32_t>(b.size())); }
Bytes list(std::initializer_list<std::uint32_t> references) {
    Bytes b(4); scalar(b, 0x89, static_cast<std::uint32_t>(4 + references.size() * 4));
    for (auto r : references) word(b, r);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes controller() {
    Bytes b(4); scalar(b, 9, 4); b.push_back(std::byte{6});
    scalar(b, 0x88, 6); scalar(b, 0x88, 7); scalar(b, 8, 0);
    b.push_back(std::byte{0x84}); text(b, "IndependentDestination");
    scalar(b, 0x83, 17); scalar(b, 0x88, 0); scalar(b, 8, 2);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes first_cut() {
    Bytes b(4); scalar(b, 0x89, 8); word(b, 5); b.push_back(std::byte{6});
    constexpr std::array<std::uint8_t, 7> tags{0x83, 0x83, 3, 8, 3, 0x83, 3};
    for (std::size_t i = 0; i < tags.size(); ++i) scalar(b, tags[i], i == 3 ? 0 : 1);
    floating(b, 2, -0.0F); b.push_back(std::byte{6});
    for (std::uint32_t i = 0; i < 5; ++i) {
        scalar(b, 3, 31 - i); scalar(b, 0x8a, i == 4 ? 0 : 1);
        scalar(b, 0x88, 2); scalar(b, 0x83, 100 + i);
        b.push_back(std::byte{4}); text(b, "IndependentTarget"); b.push_back(std::byte{6});
    }
    finish(b); return b;
}
Bytes member() {
    Bytes b(4); scalar(b, 0x89, 28);
    for (auto r : {1U, 4U, 0U, 2U, 0U, 0U}) word(b, r);
    b.push_back(std::byte{6}); floating(b, 2, 13); floating(b, 0x82, 217);
    scalar(b, 3, 1); b.push_back(std::byte{6}); finish(b); return b;
}
Bytes camera() {
    Bytes b(4); real(b, 1, 2.5); real(b, 1, 1200);
    scalar(b, 3, 11); scalar(b, 0x43, 22); scalar(b, 0x43, 33);
    real(b, 1, 0.75); real(b, 0x81, 75);
    scalar(b, 3, 7); scalar(b, 0x83, 0); scalar(b, 0x83, 4);
    scalar(b, 3, 0); scalar(b, 0x83, 3);
    floating(b, 2, 1); floating(b, 2, 2); scalar(b, 3, 5);
    floating(b, 2, 0); floating(b, 0x42, 0); floating(b, 0x42, 1); floating(b, 0x42, 1);
    scalar(b, 3, 1); b.push_back(std::byte{6}); finish(b); return b;
}
Bytes picture(bool legal) {
    Bytes b(4);
    scalar(b, 3, 3); scalar(b, legal ? 3 : 0x83, 9); scalar(b, 3, 137);
    scalar(b, 0x83, 0); scalar(b, 3, 7);
    b.push_back(std::byte{6}); scalar(b, 3, 16);
    b.push_back(std::byte{6}); b.push_back(std::byte{6}); finish(b); return b;
}
struct Fixture {
    Bytes payload, names, prm, tex;
    std::array<std::size_t, 8> block_offsets{};
    std::array<std::size_t, 8> attachment_offsets{};
    Fixture() : payload(1024) {
        // Deliberately permuted directory roles; no retail source indices.
        set(payload, 0, 32); set(payload, 4, 128); set(payload, 12, 4); set(payload, 20, 256);
        set(payload, 32, 8); set(payload, 128, 1); set(payload, 132, 144);
        const std::string_view event = "IndependentEvent";
        for (std::size_t i = 0; i < event.size(); ++i) payload[144 + i] = static_cast<std::byte>(event[i]);
        set(payload, 256, 1); set(payload, 260 + 4, 2); set(payload, 260 + 12, 6);
        constexpr std::array<float, 9> basis{0, 0, 1, 0, 1, 0, 1, 0, 0};
        for (std::size_t i = 0; i < basis.size(); ++i) set(payload, 384 + i * 4, std::bit_cast<std::uint32_t>(basis[i]));
        constexpr std::array<std::uint32_t, 8> types{0x00400003, 0x00200046, 0x0800001a, 0x00200046,
                                                     0x0800001a, 0x0800001a, 0x0800001a, 0x0800001a};
        for (std::size_t i = 0; i < types.size(); ++i) {
            const auto record = 512 + 48 * i;
            set(payload, 36 + 8 * i, static_cast<std::uint32_t>(record / 4));
            set(payload, record, static_cast<std::uint32_t>(names.size())); text(names, "FixtureNode" + std::to_string(i));
            set(payload, record + 4, 384); set(payload, record + 8, 420); set(payload, record + 16, types[i]);
        }
        const auto attach = [&](std::size_t node, const std::vector<std::pair<std::string_view, float>>& entries) {
            const auto start = payload.size(); attachment_offsets[node] = start;
            set(payload, 512 + 48 * node + 20, static_cast<std::uint32_t>(start));
            word(payload, static_cast<std::uint32_t>(entries.size())); payload.resize(payload.size() + entries.size() * 8);
            for (std::size_t i = 0; i < entries.size(); ++i) {
                set(payload, start + 4 + i * 8, static_cast<std::uint32_t>(payload.size()));
                set(payload, start + 8 + i * 8, std::bit_cast<std::uint32_t>(entries[i].second));
                text(payload, entries[i].first);
            }
        };
        attach(1, {{"ZWINPIC_FadeToBlack", 0}}); attach(2, {{"ZGEOM_MovieControl", 0}});
        attach(3, {{"ZGEOM_Center", 1}}); attach(4, {{"ZLIST_CutSequence", 1}});
        attach(7, {{"ZLIST_CutSequenceList", 0}, {"ZLIST_CutSequenceCommand", 1},
                   {"ZLIST_CutSequenceCommand", 1}, {"ZLIST_CutSequenceCommand", 1},
                   {"ZLIST_CutSequenceCommand", 1}, {"ZLIST_CutSequenceCommand", 1}});
        const std::array<Bytes, 8> blocks{camera(), picture(false), controller(), picture(true), member(), list({8, 8}), list({4, 2, 4}), first_cut()};
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            block_offsets[i] = payload.size(); set(payload, 512 + 48 * i + 32, static_cast<std::uint32_t>(payload.size()));
            payload.insert(payload.end(), blocks[i].begin(), blocks[i].end());
        }
        prm.resize(16); word(prm, 1);
        for (float v : {3.F, 4.F, 9.F, 0.F, 1.F, 1.F, 0.F, 6.F, 8.F}) word(prm, std::bit_cast<std::uint32_t>(v));
        word(prm, 0xA1B2C3D4); word(prm, 1); word(prm, 76); word(prm, 1); word(prm, 0);
        word(prm, 0x00020100); word(prm, 2048); prm.resize(108);
        set(prm, 0, 16); set(prm, 4, 108); set(prm, 8, 108); set(prm, 12, 0);
        tex.resize(16); word(tex, 0); word(tex, 0x52474241); word(tex, 0x52474241); word(tex, 0);
        word(tex, 2 | (2 << 16)); word(tex, 1); word(tex, 0); word(tex, 0); word(tex, 0); text(tex, "IndependentTexture");
        word(tex, 16); tex.insert(tex.end(), 16, std::byte{0x5a}); set(tex, 16, static_cast<std::uint32_t>(tex.size() - 16));
        const auto end = tex.size(); tex.resize(end + 8192); set(tex, end, 16);
        const auto sequences = tex.size(); tex.resize(sequences + 8192);
        set(tex, 0, static_cast<std::uint32_t>(end)); set(tex, 4, static_cast<std::uint32_t>(sequences)); set(tex, 8, 3); set(tex, 12, 4);
    }
    Bytes packed() const {
        Bytes packed; word(packed, static_cast<std::uint32_t>(payload.size()));
        word(packed, static_cast<std::uint32_t>(payload.size() + 9)); packed.push_back(std::byte{1});
        packed.insert(packed.end(), payload.begin(), payload.end());
        return packed;
    }
    off::data::GmsImage image() const {
        return off::data::GmsImage::parse(off::data::PackedResource::parse(packed()));
    }
    off::graphics::IntroPreparedResources build(std::size_t budget = 16) const {
        return off::graphics::build_intro_prepared_resources(image(), names, prm, off::data::TextureCatalog::parse(tex), budget);
    }
};
void halfword(Bytes& b, std::uint16_t value) {
    b.push_back(static_cast<std::byte>(value & 255)); b.push_back(static_cast<std::byte>(value >> 8));
}
std::uint32_t crc(const Bytes& b) {
    std::uint32_t result = 0xffffffffU;
    for (auto byte : b) {
        result ^= std::to_integer<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) result = (result >> 1) ^ ((result & 1) ? 0xedb88320U : 0U);
    }
    return ~result;
}
void archive(const std::filesystem::path& path, const std::vector<std::pair<std::string, Bytes>>& entries) {
    Bytes output, central;
    const auto filename = [](Bytes& b, const std::string& name) {
        for (char c : name) b.push_back(static_cast<std::byte>(c));
    };
    for (const auto& [name, data] : entries) {
        const auto offset = static_cast<std::uint32_t>(output.size());
        const auto size = static_cast<std::uint32_t>(data.size());
        const auto checksum = crc(data);
        word(output, 0x04034b50); halfword(output, 20); halfword(output, 0); halfword(output, 0);
        halfword(output, 0); halfword(output, 0); word(output, checksum); word(output, size); word(output, size);
        halfword(output, static_cast<std::uint16_t>(name.size())); halfword(output, 0); filename(output, name);
        output.insert(output.end(), data.begin(), data.end());
        word(central, 0x02014b50); halfword(central, 20); halfword(central, 20); halfword(central, 0); halfword(central, 0);
        halfword(central, 0); halfword(central, 0); word(central, checksum); word(central, size); word(central, size);
        halfword(central, static_cast<std::uint16_t>(name.size())); halfword(central, 0); halfword(central, 0);
        halfword(central, 0); halfword(central, 0); word(central, 0); word(central, offset); filename(central, name);
    }
    const auto central_offset = static_cast<std::uint32_t>(output.size());
    output.insert(output.end(), central.begin(), central.end());
    word(output, 0x06054b50); halfword(output, 0); halfword(output, 0);
    halfword(output, static_cast<std::uint16_t>(entries.size())); halfword(output, static_cast<std::uint16_t>(entries.size()));
    word(output, static_cast<std::uint32_t>(central.size())); word(output, central_offset); halfword(output, 0);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()));
    if (!file) throw std::runtime_error("cannot write independent intro test archive");
}
}

int main() {
    using off::graphics::IntroPreparedResources;
    static_assert(!std::is_copy_constructible_v<IntroPreparedResources> && std::is_move_constructible_v<IntroPreparedResources>);
    Fixture fixture;
    auto asset = fixture.build();
    check(asset.controller_index() == 2 && asset.first_cut_index() == 7 && asset.member_index() == 4 && asset.camera_index() == 0,
          "preparation follows authored directory references rather than fixed retail indices or pool slots");
    check(std::vector<std::uint32_t>(asset.cut_references().begin(), asset.cut_references().end()) == std::vector<std::uint32_t>{8, 8} &&
          std::vector<std::uint32_t>(asset.group_references().begin(), asset.group_references().end()) == std::vector<std::uint32_t>{4, 2, 4},
          "ordered raw lists preserve duplicate references");
    check(asset.controller().destination == "IndependentDestination" && asset.camera().angle_degrees == 75 &&
          asset.member().values == std::array<float, 2>{13, 217} &&
          asset.command_events()[0] == std::optional<std::string>{"IndependentEvent"} && !asset.command_events()[4],
          "owned source values and nullable event identities retained without execution");
    check(asset.pictures().size() == 2 && asset.pictures()[0].directory_index == 3 && asset.pictures()[1].directory_index == 1 &&
          asset.images().size() == 1 && asset.images()[0].mip_zero.pixels.size() == 16,
          "picture source identities deduplicate independently from shared texture image identity");
    check(asset.pictures()[0].picture.descriptors()[0].modulation_color == 0xA1B2C3D4U &&
          asset.pictures()[0].bindings.entries()[0].image_index == 0,
          "PRM descriptors and paired texture bindings remain authored data");
    for (std::size_t i = 0; i < 5; ++i)
        check(asset.first_cut().commands[i].timeline_position == 31 - i && asset.first_cut().commands[i].event_argument == 100 + i,
              "preparation retains attachment command order without choosing lifecycle registration order");
    auto owned = [] { Fixture local; return local.build(); }();
    auto moved = std::move(owned);
    check(moved.sources().directory().size() == 8 && !moved.source_names().empty() &&
          moved.images()[0].mip_zero.pixels[0] == 0x5aU &&
          moved.pictures()[1].picture.descriptors()[0].local_z == 9,
          "prepared resources own all required data after temporary sources are destroyed and object moved");
    rejects([&] { (void)fixture.build(15); });
    rejects([&] { (void)fixture.build(0); });
    const auto mutate_rejects = [&](auto mutate) { auto bad = fixture; mutate(bad); rejects([&] { (void)bad.build(); }); };
    mutate_rejects([](auto& f) { f.names.clear(); });
    mutate_rejects([](auto& f) { set(f.payload, 512 + 48 * 2 + 20, 0); });
    mutate_rejects([](auto& f) { f.payload[f.block_offsets[2] + 4] = std::byte{8}; });
    mutate_rejects([](auto& f) { set(f.payload, 512 + 48 * 6 + 20, static_cast<std::uint32_t>(f.attachment_offsets[2]));
                               set(f.payload, 512 + 48 * 6 + 32, static_cast<std::uint32_t>(f.block_offsets[2])); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[5] + 9, 0); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[6] + 9, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[5] + 13, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[7] + 30, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[7] + 61, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[2] + 21, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[2] + controller().size() - 11, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[2] + controller().size() - 6, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[4] + 9, 4); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[4] + 13, 2); });
    mutate_rejects([](auto& f) { f.prm.resize(32); });
    mutate_rejects([](auto& f) { set(f.prm, 80, 2049); });
    const auto image = fixture.image();
    check(image.attachment_identifier(2, 0) == "ZGEOM_MovieControl", "checked attachment identity accessor");
    rejects([&] { (void)image.attachment_identifier(8, 0); });
    rejects([&] { (void)image.attachment_identifier(0, 0); });
    {
        auto unterminated = fixture;
        set(unterminated.payload, unterminated.attachment_offsets[2] + 4,
            static_cast<std::uint32_t>(unterminated.payload.size()));
        unterminated.payload.push_back(std::byte{0x5a});
        const auto bad = unterminated.image();
        rejects([&] { (void)bad.attachment_identifier(2, 0); });
        rejects([&] { (void)unterminated.build(); });
    }
    {
        const auto work = std::filesystem::path{OFF_TEST_WORK_DIR};
        std::filesystem::create_directories(work);
        const std::vector<std::pair<std::string, Bytes>> members{
            {"owned.GMS", fixture.packed()}, {"owned.BUF", fixture.names},
            {"owned.PRM", fixture.prm}, {"owned.TEX", fixture.tex}};
        const auto path = work / "independent-intro.zip";
        archive(path, members);
        const auto loaded = off::graphics::load_intro_prepared_resources(path);
        check(loaded.controller_index() == 2 && loaded.pictures().size() == 2 && loaded.images().size() == 1,
              "exact archive loader preserves full prepared chain with case-insensitive member extensions");
        for (std::size_t i = 0; i < members.size(); ++i) {
            auto incomplete = members; incomplete.erase(incomplete.begin() + static_cast<std::ptrdiff_t>(i));
            const auto missing = work / ("missing-" + std::to_string(i) + ".zip"); archive(missing, incomplete);
            rejects([&] { (void)off::graphics::load_intro_prepared_resources(missing); });
            auto duplicate = members; duplicate.emplace_back("extra" + members[i].first.substr(5), members[i].second);
            const auto repeated = work / ("duplicate-" + std::to_string(i) + ".zip"); archive(repeated, duplicate);
            rejects([&] { (void)off::graphics::load_intro_prepared_resources(repeated); });
        }
        auto corrupt = members; corrupt[1].second.clear();
        const auto bad = work / "invalid-paired-buf.zip"; archive(bad, corrupt);
        rejects([&] { (void)off::graphics::load_intro_prepared_resources(bad); });
    }
    return failures == 0 ? 0 : 1;
}
