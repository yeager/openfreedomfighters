#include "off/graphics/intro_prepared_resources.hpp"
#include "off/graphics/intro_runtime.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/archive_vfs.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
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
    for (auto r : {9U, 4U, 0U, 2U, 0U, 0U}) word(b, r);
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
Bytes window() {
    Bytes b(4); scalar(b, 3, 19); floating(b, 2, -0.0F);
    scalar(b, 3, 7); scalar(b, 3, 9); scalar(b, 3, 23);
    b.push_back(std::byte{6}); b.push_back(std::byte{6});
    scalar(b, 0x88, 9); scalar(b, 0x88, 0); scalar(b, 0x88, 2);
    scalar(b, 0x83, 5); scalar(b, 0x83, 6); scalar(b, 3, 7);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes sound_owner() {
    Bytes b(4); scalar(b,0x83,5); scalar(b,0x8b,128);
    for(float value:{271.F,272.F,-0.0F,7.F}) floating(b,2,value);
    scalar(b,3,123); scalar(b,3,6); floating(b,2,66); floating(b,2,0.5F);
    scalar(b,3,7); scalar(b,3,9); floating(b,2,-0.0F);
    b.push_back(std::byte{6});
    // Opaque placeholder groups: only the owner prefix is tested here.
    for(int i=0;i<4;++i) b.push_back(std::byte{6});
    finish(b); return b;
}
Bytes audio_header() {
    Bytes b(16); set(b,8,3); set(b,12,4);
    for(const auto global:{false,true}) {
      const auto size=global?8U:2U;
      for(const auto value:{6U,0U,global?0x80000001U:1U,22050U,16U,size,size,1U,
                           global?7U:0U,size/2,2U,1U}) word(b,value);
    }
    word(b,0); word(b,0); set(b,0,static_cast<std::uint32_t>(b.size()-8));
    set(b,4,static_cast<std::uint32_t>(b.size())); return b;
}
void save_fixture(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!file) throw std::runtime_error("Cannot write independent audio fixture");
}
struct Fixture {
    Bytes payload, names, prm, tex, snd;
    std::array<std::size_t, 10> block_offsets{};
    std::array<std::size_t, 10> attachment_offsets{};
    explicit Fixture(bool include_sound=false) : payload(1024), snd(16) {
        // Deliberately permuted directory roles; no retail source indices.
        set(payload, 0, 32); set(payload, 4, 128); set(payload, 12, 4); set(payload, 20, 176);
        set(payload, 32, include_sound?10:9); set(payload, 128, 1); set(payload, 132, 144);
        const std::string_view event = "IndependentEvent";
        for (std::size_t i = 0; i < event.size(); ++i) payload[144 + i] = static_cast<std::byte>(event[i]);
        set(payload, 176, 2); set(payload, 180, 1); set(payload, 280, 2); set(payload, 288, include_sound?7:6);
        constexpr std::array<float, 9> basis{0, 0, 1, 0, 1, 0, 1, 0, 0};
        for (std::size_t i = 0; i < basis.size(); ++i) set(payload, 384 + i * 4, std::bit_cast<std::uint32_t>(basis[i]));
        constexpr std::array<std::uint32_t, 10> types{0x00400003, 0x00200046, 0x0800001a, 0x00200046,
                                                     0x0800001a, 0x0800001a, 0x0800001a, 0x0800001a, 0x00100030,0x00200012};
        const std::size_t node_count=include_sound?10:9;
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto record = 512 + 48 * i;
            set(payload, 36 + 8 * i, static_cast<std::uint32_t>(record / 4));
            set(payload, record, static_cast<std::uint32_t>(names.size())); text(names, "FixtureNode" + std::to_string(i));
            set(payload, record + 4, 384); set(payload, record + 8, 420); set(payload, record + 16, types[i]);
        }
        // Only directory order is swapped: the window precedes and parents the
        // camera, while all other role indices retain their independent values.
        set(payload, 36, (1U << 24U) | static_cast<std::uint32_t>((512 + 48 * 8) / 4));
        set(payload, 36 + 8 * 8, 512U / 4U);
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
        if(include_sound) attach(9,{{"ZSNDOBJ_SoundExtend",0},{"ZSNDOBJ_SoundNotify",0},
                                  {"ZSNDOBJ_SoundSegment",1},{"ZGEOM_ZSetZDefine",0}});
        const std::array<Bytes, 10> blocks{camera(), picture(false), controller(), picture(true), member(), list({8, 8}), list({4, 2, 4}), first_cut(), window(),sound_owner()};
        for (std::size_t i = 0; i < node_count; ++i) {
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
        if(include_sound) {
            text(snd,"Independent/Sound.asset"); snd.resize(144);
            set(snd,128,1); set(snd,132,16); set(snd,136,0x1234);
            set(snd,140,std::bit_cast<std::uint32_t>(12.375F));
        }
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
        return off::graphics::build_intro_prepared_resources(image(), names, prm, off::data::TextureCatalog::parse(tex), budget,snd);
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
    {
      Fixture sound_fixture(true);
      const auto prepared=sound_fixture.build();
      check(prepared.sounds().size()==1 && prepared.sound_bank()->size()==144,
            "prepared intro owns complete sound bank and source-bound definition");
      const auto& sound=prepared.sounds().front();
      check(sound.directory_index==9 && sound.source.authored_type==5 &&
            sound.source.sound_definition_reference==128 && sound.definition.definition_offset==128 &&
            sound.definition.identifier_offset==16 && sound.definition.resource_link==0x1234 &&
            sound.definition.logical_identifier=="Independent/Sound.asset" && sound.definition.duration==12.375F,
            "SND offset, logical identifier, opaque link and authored duration retain distinct domains");
      check(sound.source.cone_scalars[0]==271 && std::signbit(sound.source.cone_scalars[2]) &&
            sound.source.legacy_integer==123 && sound.source.loop_option==6 &&
            sound.source.gain_multiplier==66 && sound.source.pitch_scalar==0.5F &&
            sound.source.category==7 && sound.source.enabled_option==9 &&
            std::signbit(sound.source.final_scalar) &&
            sound.source.component_groups_offset==sound_fixture.block_offsets[9]+70,
            "owner prefix preserves raw fields and leaves component groups unread");
      auto missing=sound_fixture; missing.snd.clear(); rejects([&] { (void)missing.build(); });
      auto null_definition=sound_fixture; set(null_definition.payload,null_definition.block_offsets[9]+10,0);
      rejects([&] { (void)null_definition.build(); });
      auto wrong_domain=sound_fixture; set(wrong_domain.payload,wrong_domain.block_offsets[9]+10,9);
      rejects([&] { (void)wrong_domain.build(); });
      auto unsupported=sound_fixture; set(unsupported.snd,128,2); rejects([&] { (void)unsupported.build(); });
      for(std::size_t field=0;field<13;++field) {
        auto malformed=sound_fixture;
        malformed.payload[malformed.block_offsets[9]+4+field*5]=std::byte{0x7f};
        rejects([&] { (void)malformed.build(); });
      }
      auto nonfinite=sound_fixture;
      set(nonfinite.payload,nonfinite.block_offsets[9]+15,0x7f800000);
      rejects([&] { (void)nonfinite.build(); });
      auto wrong_type=sound_fixture; set(wrong_type.payload,wrong_type.block_offsets[9]+5,10);
      rejects([&] { (void)wrong_type.build(); });
      auto wrong_parameter=sound_fixture;
      set(wrong_parameter.payload,wrong_parameter.attachment_offsets[9]+8,std::bit_cast<std::uint32_t>(1.0F));
      rejects([&] { (void)wrong_parameter.build(); });
    }
    std::int32_t clock_sample=1000;
    off::runtime::SceneComponentSequence component_sequence;
    off::runtime::ApplicationServices application(
        off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[] { return std::int64_t{99}; },[&] { return clock_sample; }},
        []() -> off::audio::SoundVolumeBackend* { return nullptr; });
    {
      off::graphics::IntroRuntime host(fixture.build(),application,component_sequence);
      check(host.components().size()==11 && component_sequence.next_identity()==0,
            "complete fixture attachment catalog plus root does not invent construction IDs");
      check(host.components().at(0).source().factory_name=="ZGROUP_RootGroup" &&
            !host.components().at(0).constructed(), "synthesized root awaits its concrete factory");
      const auto controller_component=host.controller_component_index();
      check(host.components().at(controller_component).source().factory_name=="ZGEOM_MovieControl" &&
            host.owner_components(host.source_handle(host.resources().controller_index())).size()==1,
            "controller joins same retained owner attachment catalog");
      check(host.controller_initialization().deadline()==0 &&
            !host.controller_initialization().phase_two_completed(),
            "retained controller starts uninitialized; resource construction does not invoke phase two");
      static_assert(!std::is_move_constructible_v<off::graphics::IntroRuntime>);
      check(host.hierarchy().size() == 10 && !host.source_index(host.root_handle()),
            "runtime retains synthesized root distinct from all source objects");
      check(host.hierarchy()[0].parent == off::graphics::no_picture_transform_parent &&
            host.hierarchy()[0].position == std::array<float,3>{0,0,0} &&
            !std::signbit(host.hierarchy()[0].position[0]),
            "runtime root has positive zero translation and no parent");
      check(host.additional_owner_order().size() == 9 &&
            host.additional_owner_order()[8] == host.source_handle(8) &&
            host.hierarchy()[host.hierarchy_index(host.source_handle(8))].parent ==
                host.hierarchy_index(host.source_handle(0)),
            "runtime construction retains directory order and actual window parent");
      auto& first = host.picture_for_source(1);
      auto& second = host.picture_for_source(3);
      check(first.descriptors().data() == second.descriptors().data() &&
            first.descriptors()[0].modulation_color == 0xA1B2C3D4U &&
            first.color_state().alpha() == 137 && first.color_state().color() == 0xffffffffU,
            "shared PRM descriptors preserve authored colors independently of owner alpha");
      const auto authored = host.resources().pictures()[0].picture.descriptors()[0].modulation_color;
      first.color_state().set_alpha(17);
      check(second.draw_plan().groups()[0].quads[0].modulation_color == 0x11ffffffU &&
            host.resources().pictures()[0].picture.descriptors()[0].modulation_color == authored &&
            host.paired_material(76) == first.color_state().material(),
            "runtime alias writes reach draw snapshots and paired material without changing sources");
      check(!first.runtime_resource_flags() && host.frame_clock().value() == 1 &&
            host.view_transition().last_clear_frame() == 0 && !host.camera().associated_target(),
            "construction preserves unknown admission and fresh canonical state");
      host.set_local_transform(host.root_handle(),host.hierarchy()[0].matrix,{2,3,4});
      check(host.hierarchy()[0].position == std::array<float,3>{2,3,4} && first.submission_cache().dirty(),
            "live root transforms persist and invalidate picture submissions");
      rejects([&] { (void)host.source_index({0}); });
      rejects([&] { (void)host.picture_for_source(0); });
      rejects([&] { host.project_selected_window_camera_state(); });
      check(!host.camera().associated_target(), "unsupported window projection has no camera effects");
    }
    {
      auto projection_fixture = fixture;
      set(projection_fixture.payload, projection_fixture.block_offsets[8]+42,0);
      off::graphics::IntroRuntime host(projection_fixture.build(),application,component_sequence);
      const auto old_flags = host.camera().flags();
      host.project_selected_window_camera_state();
      check(host.window_camera_projection_applied() &&
            host.camera().associated_target() == host.source_handle(0).value &&
            host.camera().flags() == ((old_flags & ~0x8000U) | 0x210000U) &&
            host.camera().render_control() == 0 && host.camera().enabled(),
            "explicit window projection updates the same canonical camera without registration");
      rejects([&] { host.project_selected_window_camera_state(); });
    }
    {
      application.reset_clock();
      application.clock().assign_crt_mode(true);
      clock_sample=1250;
      application.advance_crt();
      application.clock().publish_scene(false);
      off::graphics::IntroRuntime host(fixture.build(),application,component_sequence);
      off::graphics::IntroControllerPhaseTwoServices services;
      services.input_manager_exists=[] { return false; };
      services.register_movie_control_action_map=[] { throw std::runtime_error("no input manager in this fixture"); };
      services.query_global_property=[](std::string_view key,std::uint32_t& out) {
        out=key=="SoundReadFromMem"?1:60;
      };
      unsigned presented=0;
      services.first_renderer=[] { return 7U; };
      services.renderer_height=[](auto) { return 480; };
      services.renderer_width=[](auto) { return 640; };
      services.set_viewport=[](auto,const auto&) {};
      services.renderer_has_stencil=[](auto) { return false; };
      services.clear=[](auto,const auto&) {};
      services.present=[&](auto) { ++presented; return off::graphics::IntroPresentationResult::presented; };
      // Independent lifecycle fixture, not original class factories: all other
      // entries have phase-one-only callbacks. MovieControl uses the real host
      // binding, shared clock/preferences and the explicit renderer fixture.
      const auto movie_callback=host.controller_phase_two_callback(services);
      for (std::size_t i=0; i<host.components().size(); ++i) {
        host.components().construct(i,[&](off::runtime::ComponentRecord& record) {
          const bool movie=&record==&host.components().at(host.controller_component_index());
          return off::runtime::ConstructedComponent{
            {0,0,movie?3U:1U,0,0,0x20,0,record.source().owner},
            [](auto&) {},movie?movie_callback:off::runtime::ComponentCallback{}};
        });
      }
      host.components().run_global_phases({
        [](bool,auto&,std::size_t) {},
        [](auto) { return std::optional<std::uint32_t>{0}; },
        [](auto) {},[](auto&) { throw std::runtime_error("unexpected fixture retirement"); }});
      check(&host.application()==&application && application.sound().volume()==60 &&
            *application.configuration().find("SoundEffectsVolume")=="60" &&
            host.controller_initialization().deadline()==2304 && presented==2 &&
            host.components().phases_completed() &&
            host.components().at(host.controller_component_index()).state().status==0x2c,
            "host phase two consumes canonical application clock and actual retained sound configuration");
      off::graphics::IntroRuntime next_host(fixture.build(),application,component_sequence);
      check(next_host.application().sound().volume()==60 &&
            next_host.application().clock().scene_integer_word()==256 &&
            !next_host.controller_initialization().phase_two_completed(),
            "new scene host preserves application state without fabricating initialization");
    }
    auto asset = fixture.build();
    check(asset.controller_index() == 2 && asset.first_cut_index() == 7 && asset.member_index() == 4 && asset.camera_index() == 8,
          "preparation follows authored directory references rather than fixed retail indices or pool slots");
    check(asset.window_index() == 0 && asset.window().selected_camera_reference == 9 &&
          asset.window().options == std::array<std::uint32_t, 3>{5, 6, 7},
          "matching window is joined by selected authored camera and retains unnormalized options");
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
    check(moved.sources().directory().size() == 9 && moved.window_index() == 0 && !moved.source_names().empty() &&
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
    mutate_rejects([](auto& f) { set(f.payload, 512 + 48 * 8 + 16, 0x00100000U); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[8] + 32, 2); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[8] + 32, 0); });
    mutate_rejects([](auto& f) { f.payload[f.block_offsets[8] + 4] = std::byte{0x83}; });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[8] + 37, 99); });
    mutate_rejects([](auto& f) { set(f.payload, f.block_offsets[8] + 42, 99); });
    mutate_rejects([](auto& f) {
        // Leave the camera outside the window's child pool while retaining
        // valid pool counts: the supported authored parent link is absent.
        set(f.payload, 36 + 8 * 8, (1U << 25U) | (512U / 4U));
        set(f.payload, 180 + 12, 1); set(f.payload, 288, 5);
    });
    {
        auto unrelated = fixture;
        set(unrelated.payload, 32, 10);
        while (unrelated.payload.size() % 4 != 0) unrelated.payload.push_back(std::byte{0});
        const auto pools = unrelated.payload.size();
        unrelated.payload.resize(pools + 4 + 3 * 96);
        set(unrelated.payload, 20, static_cast<std::uint32_t>(pools));
        set(unrelated.payload, pools, 3); set(unrelated.payload, pools + 4, 1);
        set(unrelated.payload, pools + 100, 1); set(unrelated.payload, pools + 104, 2);
        set(unrelated.payload, pools + 112, 6);
        std::copy_n(unrelated.payload.begin() + 512 + 48 * 8, 48,
                    unrelated.payload.begin() + 512 + 48 * 9);
        set(unrelated.payload, 36 + 8 * 9, static_cast<std::uint32_t>((512 + 48 * 9) / 4));
        // An unrelated same-family variant is deliberately outside the strict
        // window schema. It must not be decoded while following the camera parent.
        set(unrelated.payload, 512 + 48 * 9 + 20,
            static_cast<std::uint32_t>(unrelated.attachment_offsets[1]));
        check(unrelated.build().window_index() == 0,
              "unrelated window variants do not invalidate the selected camera-parent pair");
    }
    const auto image = fixture.image();
    check(image.attachment_identifier(2, 0) == "ZGEOM_MovieControl", "checked attachment identity accessor");
    rejects([&] { (void)image.attachment_identifier(9, 0); });
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
            {"owned.PRM", fixture.prm}, {"owned.TEX", fixture.tex}, {"owned.SND",fixture.snd}};
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
        const auto scenes=work/"Scenes"; std::filesystem::create_directories(scenes);
        Fixture sound_fixture(true); set(sound_fixture.snd,136,0x41);
        const auto sound_path=scenes/"independent-sound.zip";
        archive(sound_path,{{"sound.GMS",sound_fixture.packed()},{"sound.BUF",sound_fixture.names},
            {"sound.PRM",sound_fixture.prm},{"sound.TEX",sound_fixture.tex},{"sound.SND",sound_fixture.snd}});
        save_fixture(scenes/"independent-sound.WHD",audio_header());
        save_fixture(scenes/"independent-sound.WAV",Bytes(2,std::byte{0x11}));
        const Bytes global_bytes(15,std::byte{0x5a}); save_fixture(work/"streams.wav",global_bytes);
        const auto with_audio=off::graphics::load_intro_prepared_resources(sound_path);
        check(with_audio.audio() && with_audio.audio()->record_indices().size()==1 &&
              with_audio.audio()->record_indices()[0]==1 &&
              with_audio.audio()->read_encoded(0)==Bytes(8,std::byte{0x5a}),
              "normal archive loader follows odd SND resource link into the correct global bank range");
        check(with_audio.sounds()[0].definition.duration==12.375F &&
              with_audio.audio()->header().records()[1].sample_rate==22050,
              "authored duration remains independent of WHD counts and sample rate");
        const auto pcm=with_audio.audio()->decode(0);
        check(pcm.frame_count()==4 && pcm.channels==1 &&
              with_audio.sounds()[0].definition.duration==12.375F,
              "source-bound offline decode does not replace authored duration or emit readiness");
        rejects([&] { (void)with_audio.audio()->read_encoded(0,7); });
        rejects([&] { (void)with_audio.audio()->read_encoded(1); });
        const auto header=off::data::AudioBankHeader::parse(audio_header());
        check(!header.record_index_for_sound_link(0) && !header.record_index_for_sound_link(1) &&
              header.record_index_for_sound_link(16)==0 && header.record_index_for_sound_link(17)==0 &&
              header.record_index_for_sound_link(64)==1,"WHD offsets are full-image boundaries, not row numbers");
        for(const auto invalid:{2U,14U,18U,48U,112U,0xffffffffU})
          rejects([&] { (void)header.record_index_for_sound_link(invalid); });
        // The read view outlives its mounting VFS, but still detects truncation.
        save_fixture(work/"streams.wav",Bytes(8));
        rejects([&] { (void)with_audio.audio()->read_encoded(0); });
        rejects([&] { (void)off::graphics::load_intro_prepared_resources(sound_path); });
    }
    return failures == 0 ? 0 : 1;
}
