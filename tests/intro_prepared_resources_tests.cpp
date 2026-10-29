#include "off/graphics/camera_display.hpp"
#include "off/graphics/intro_prepared_resources.hpp"
#include "off/graphics/intro_outer_loader_tail_readiness.hpp"
#include "off/graphics/intro_named_global_section_envelope.hpp"
#include "off/graphics/intro_renderer_resource_envelope.hpp"
#include "off/graphics/intro_runtime.hpp"
#include "off/data/matpos_owner_child_selector.hpp"
#include "off/cutscene/first_cut_command_session.hpp"
#include "off/graphics/normal_intro_scene_session.hpp"
#include "off/graphics/owned_normal_intro_scene.hpp"
#include "off/graphics/movie_control_first_cut_scene_frame_adapter.hpp"
#include "off/graphics/intro_accepted_picture_registry.hpp"
#include "off/graphics/intro_first_cut_accepted_picture_registry.hpp"
#include "off/graphics/intro_preview_builder.hpp"
#include "off/graphics/preview_camera_component.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/archive_vfs.hpp"
#include "off/data/basic_group_deferred_reader_shape.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using Bytes = std::vector<std::byte>;
int failures = 0;
#if defined(_MSC_VER)
#define OFF_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define OFF_NOINLINE __attribute__((noinline))
#else
#define OFF_NOINLINE
#endif

void check(bool condition, const char* text) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
static OFF_NOINLINE void test_outer_loader_tail_readiness() {
    off::data::GmsOuterLoaderSources sources;
    sources.named_global=Bytes{std::byte{1},std::byte{2},std::byte{3}};
    sources.renderer_resource=Bytes{std::byte{4},std::byte{5},std::byte{6},std::byte{7}};
    sources.resource_associations={{7U,9U},{11U,13U}};
    sources.allocation_sizing_rows={{1U,2U,3U}};
    const auto readiness=off::graphics::inspect_intro_outer_loader_tail_readiness(sources);
    check(readiness.named_global_bytes==3U && readiness.renderer_resource_bytes==4U &&
              readiness.resource_association_count==2U && readiness.allocation_sizing_row_count==1U &&
              !readiness.ready_to_run(),
          "outer loader tail readiness retains exact source section sizes without consuming them");
    const std::vector expected{
        off::graphics::IntroOuterLoaderTailBoundary::named_global_relocation_and_reader,
        off::graphics::IntroOuterLoaderTailBoundary::renderer_allocation_diagnostic_state,
        off::graphics::IntroOuterLoaderTailBoundary::live_resource_association,
        off::graphics::IntroOuterLoaderTailBoundary::loader_source_lease_release,
        off::graphics::IntroOuterLoaderTailBoundary::camera_zero_query_and_fallback_registration,
        off::graphics::IntroOuterLoaderTailBoundary::outer_scene_operations,
        off::graphics::IntroOuterLoaderTailBoundary::saved_resource_spatial_admission,
        off::graphics::IntroOuterLoaderTailBoundary::saved_resource_0x4000_service};
    check(readiness.required_boundaries==expected &&
              std::string_view{off::graphics::intro_outer_loader_tail_boundary_label(expected[1])}==
                  "renderer-allocation-diagnostic-state",
          "outer loader tail readiness reports every still-concrete boundary in source order");
    const auto empty=off::graphics::inspect_intro_outer_loader_tail_readiness({});
    check(empty.named_global_bytes==0U && empty.renderer_resource_bytes==0U &&
              empty.resource_association_count==0U && empty.allocation_sizing_row_count==0U &&
              empty.required_boundaries.size()==5U && !empty.ready_to_run(),
          "outer loader tail readiness never turns absent source sections into a runnable tail");
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
std::size_t keys_property(Bytes& names) {
    const auto property=names.size();
    names.resize(property+96);
    set(names,property,0U);set(names,property+4,0x80000040U);
    set(names,property+8,64U);set(names,property+12,1U);
    set(names,property+16,off::data::matpos_owner_child_selector_word);set(names,property+20,48U);
    // Synthetic KEYS descriptors deliberately point into the same retained
    // names/BUF allocation. This supplies only bounded read-only evaluator
    // coverage; it is not authored game data or a pose fixture.
    set(names,property+24,1U);set(names,property+28,0U);set(names,property+32,0U);
    set(names,property+36,std::bit_cast<std::uint32_t>(1.F));
    set(names,property+40,static_cast<std::uint32_t>(property+64));
    set(names,property+44,static_cast<std::uint32_t>(property+68));
    set(names,property+48,static_cast<std::uint32_t>(property+72));
    set(names,property+52,static_cast<std::uint32_t>(property+80));
    set(names,property+56,static_cast<std::uint32_t>(property+84));
    set(names,property+60,static_cast<std::uint32_t>(property+88));
    names[property+64]=std::byte{1};names[property+68]=std::byte{1};
    names[property+80]=std::byte{1};names[property+84]=std::byte{1};
    return property;
}
struct FixtureReferenceMap {
    std::uint32_t offset{};
    bool picture_order{};
    bool camera_row{};
    bool second_window{};
    bool full_second_scope{};
    bool following_visual_scope{};
    bool room_animation_scope{};
    std::uint32_t operator()(std::uint32_t value) const {
        if(!value) return 0;
        if(picture_order) {
            if(camera_row) {
                if(value==9) return 6;
                if(value==3) return following_visual_scope?49U:full_second_scope?43U:second_window?9U:7U;
                if(value>=5 && value<=8) return value+(following_visual_scope?45U:full_second_scope?39U:second_window?5U:3U);
            }
            if(value==2) return 5;
            if(value==3) return 6;
            if(value==4) return 4;
        }
        return value+offset;
    }
};
Bytes list(std::initializer_list<std::uint32_t> references,FixtureReferenceMap map={}) {
    Bytes b(4); scalar(b, 0x89, static_cast<std::uint32_t>(4 + references.size() * 4));
    for (auto r : references) word(b, map(r));
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes controller(FixtureReferenceMap map={}) {
    Bytes b(4); scalar(b, 9, 4); b.push_back(std::byte{6});
    scalar(b, 0x88, map(6)); scalar(b, 0x88, map(7)); scalar(b, 8, 0);
    b.push_back(std::byte{0x84}); text(b, "IndependentDestination");
    scalar(b, 0x83, 17); scalar(b, 0x88, 0); scalar(b, 8, 2);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes first_cut(FixtureReferenceMap map={},bool include_events=true) {
    Bytes b(4); scalar(b, 0x89, 8); word(b, map(5)); b.push_back(std::byte{6});
    constexpr std::array<std::uint8_t, 7> tags{0x83, 0x83, 3, 8, 3, 0x83, 3};
    for (std::size_t i = 0; i < tags.size(); ++i) scalar(b, tags[i], i == 3 ? 0 : 1);
    floating(b, 2, -0.0F); b.push_back(std::byte{6});
    for (std::uint32_t i = 0; i < 5; ++i) {
        scalar(b, 3, 31 - i); scalar(b, 0x8a, i == 4 || !include_events ? 0 : 1);
        scalar(b, 0x88, map(2)); scalar(b, 0x83, 100 + i);
        b.push_back(std::byte{4}); text(b, "IndependentTarget"); b.push_back(std::byte{6});
    }
    finish(b); return b;
}
Bytes member(FixtureReferenceMap map={}) {
    Bytes b(4); scalar(b, 0x89, 28);
    for (auto r : {9U, 4U, 0U, 2U, 0U, 0U}) word(b, map(r));
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
Bytes window(FixtureReferenceMap map={}) {
    Bytes b(4); scalar(b, 3, 19); floating(b, 2, -0.0F);
    scalar(b, 3, 7); scalar(b, 3, 9); scalar(b, 3, 23);
    b.push_back(std::byte{6}); b.push_back(std::byte{6});
    scalar(b, 0x88, map(9)); scalar(b, 0x88, 0); scalar(b, 0x88, map(2));
    scalar(b, 0x83, 5); scalar(b, 0x83, 6); scalar(b, 3, 7);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes sound_owner(FixtureReferenceMap map={}) {
    Bytes b(4); scalar(b,0x83,5); scalar(b,0x8b,128);
    for(float value:{271.F,272.F,-0.0F,7.F}) floating(b,2,value);
    scalar(b,3,123); scalar(b,3,6); floating(b,2,66); floating(b,2,0.5F);
    scalar(b,3,7); scalar(b,3,9); floating(b,2,-0.0F);
    b.push_back(std::byte{6});
    floating(b,0x82,-2.5F); scalar(b,3,11); scalar(b,3,12);
    for(float value:{0.25F,0.5F,0.75F}) floating(b,2,value);
    floating(b,0x82,-0.0F); scalar(b,3,13); scalar(b,3,1);
    floating(b,2,1.25F); scalar(b,3,14); scalar(b,0x83,7);
    scalar(b,3,5); scalar(b,3,6); scalar(b,3,99); b.push_back(std::byte{6});
    scalar(b,0x88,map(2)); scalar(b,0x0a,1); b.push_back(std::byte{6});
    scalar(b,3,1); scalar(b,0x0a,1); scalar(b,0x0a,0);
    for(std::uint32_t group=0;group<4;++group) {
      scalar(b,group==3?0x83:3,group);
      for(std::uint32_t field=1;field<4;++field) scalar(b,0x43,group*10+field);
    }
    floating(b,2,0.375F); scalar(b,3,1); b.push_back(std::byte{4}); text(b,"Independent caption key");
    b.push_back(std::byte{6}); scalar(b,0x83,1); b.push_back(std::byte{0x84});
    text(b,"IndependentProperty"); b.push_back(std::byte{6});
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
    std::vector<std::size_t> tail_record_offsets;
    std::vector<std::size_t> synthetic_fade_record_offsets;
    std::vector<std::size_t> synthetic_external_record_offsets;
    std::vector<std::size_t> synthetic_external_command_offsets;
    std::size_t tail_parent_control_offset{};
    explicit Fixture(bool include_sound=false,bool leading_group=false,bool language_group=false,bool include_events=true,bool camera_row=false,bool second_window=false,bool full_second_scope=false,bool following_visual_scope=false,bool room_animation_scope=false,std::size_t synthetic_tail_count=3,std::size_t component_slot=1,std::size_t synthetic_external_count=0,std::size_t synthetic_fade_count=0) : payload(1024) {
        if(language_group && !leading_group) throw std::runtime_error("language fixture requires leading group");
        if(camera_row && !language_group) throw std::runtime_error("camera row fixture requires language group");
        if(second_window && !camera_row) throw std::runtime_error("second Window fixture requires Camera row");
        if(full_second_scope && !second_window) throw std::runtime_error("full second scope requires second Window");
        if(following_visual_scope && !full_second_scope) throw std::runtime_error("following visual scope requires full second scope");
        if(room_animation_scope && !following_visual_scope) throw std::runtime_error("Room fixture requires preceding visual scope");
        if(room_animation_scope && synthetic_tail_count==0) throw std::runtime_error("synthetic tail fixture must contain at least one record");
        if(room_animation_scope && component_slot>=synthetic_tail_count) throw std::runtime_error("synthetic component slot must name a tail record");
        if((synthetic_external_count || synthetic_fade_count) && !leading_group)
            throw std::runtime_error("synthetic reader variants require an authored leading group fixture");
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
        // All nonnull references emitted below target original rows after the
        // first window, so both inserted rows precede those referenced owners.
        const FixtureReferenceMap bias{leading_group?(language_group?2U:1U):0U,language_group,camera_row,second_window,full_second_scope,following_visual_scope};
        const std::array<Bytes, 10> blocks{camera(), picture(false), controller(bias), picture(true), member(bias), list({8, 8},bias), list({4, 2, 4},bias), first_cut(bias,include_events), window(bias),sound_owner(bias)};
        for (std::size_t i = 0; i < node_count; ++i) {
            block_offsets[i] = payload.size(); set(payload, 512 + 48 * i + 32, static_cast<std::uint32_t>(payload.size()));
            payload.insert(payload.end(), blocks[i].begin(), blocks[i].end());
        }
        if(leading_group) {
            while(payload.size()%4) payload.push_back(std::byte{0});
            const auto record=payload.size();payload.resize(record+48);
            set(payload,record,static_cast<std::uint32_t>(names.size()));text(names,"IndependentLeadingGroup");
            set(payload,record+4,384);set(payload,record+8,420);
            set(payload,record+16,0x00100001U);set(payload,record+24,0x03200000U);
            const auto deferred=payload.size();
            set(payload,record+32,static_cast<std::uint32_t>(deferred));
            Bytes deferred_blob(4);scalar(deferred_blob,3,12345U);finish(deferred_blob);
            Bytes basic_group_blob(4);
            scalar(basic_group_blob,3,12345U);floating(basic_group_blob,2,1.0F);
            scalar(basic_group_blob,3,2U);scalar(basic_group_blob,3,3U);scalar(basic_group_blob,3,4U);
            basic_group_blob.push_back(std::byte{0x06});finish(basic_group_blob);
            payload.insert(payload.end(),basic_group_blob.begin(),basic_group_blob.end());
            while(payload.size()%4) payload.push_back(std::byte{0});
            const auto language_record=payload.size();
            if(language_group) {
                payload.resize(language_record+48);
                set(payload,language_record,static_cast<std::uint32_t>(names.size()));text(names,"IndependentLanguageGroup");
                set(payload,language_record+4,384);set(payload,language_record+8,420);
                set(payload,language_record+16,0x00101389U);set(payload,language_record+24,0x03000000U);
                set(payload,language_record+32,static_cast<std::uint32_t>(payload.size()));
                payload.insert(payload.end(),deferred_blob.begin(),deferred_blob.end());
                while(payload.size()%4) payload.push_back(std::byte{0});
                set(payload,512+48*8+24,0x03000000U);
                set(payload,512+48*3+24,0x00200400U);
                set(payload,512+48*1+24,0x00200000U);
                if(camera_row) set(payload,512+24,0x00200400U);
            }
            const auto second_window_record=payload.size();
            if(second_window) {
                payload.resize(payload.size()+96);
                for(std::size_t i=0;i<2;++i) {
                    const auto new_record=second_window_record+48*i;
                    set(payload,new_record,static_cast<std::uint32_t>(names.size()));
                    text(names,i==0?"IndependentSecondWindow":"IndependentSecondFade");
                    set(payload,new_record+4,384);set(payload,new_record+8,420);
                    set(payload,new_record+16,i==0?0x00100030U:0x00200046U);
                    set(payload,new_record+24,i==0?0x03000000U:0x00200000U);
                    set(payload,new_record+32,static_cast<std::uint32_t>(i==0?deferred:block_offsets[1]));
                }
                set(payload,second_window_record+48+20,static_cast<std::uint32_t>(attachment_offsets[1]));
            }
            const auto full_scope_records=payload.size();
            if(full_second_scope) {
                payload.resize(payload.size()+34*48);
                const auto add_attachment=[&](std::size_t at,std::string_view name,std::uint32_t argument,std::size_t count) {
                    const auto table=payload.size();word(payload,static_cast<std::uint32_t>(count));
                    payload.resize(payload.size()+count*8);
                    for(std::size_t entry=0;entry<count;++entry) {
                        set(payload,table+4+entry*8,static_cast<std::uint32_t>(payload.size()));
                        set(payload,table+8+entry*8,argument);text(payload,name);
                    }
                    set(payload,at+20,static_cast<std::uint32_t>(table));
                };
                for(std::size_t row=8;row<=41;++row) {
                    const auto at=full_scope_records+(row-8)*48;
                    const bool is_character=row==8 || row==12 || row==14 || (row>=17 && row<=35 && row%2==1);
                    const bool is_picture=row==9 || row==36;
                    const bool is_camera=row==10;
                    const auto type=is_character?0x0020002dU:is_picture?0x00200046U:is_camera?0x00400003U:0x0800001aU;
                    set(payload,at,static_cast<std::uint32_t>(names.size()));text(names,"IndependentScopeOwner"+std::to_string(row));
                    set(payload,at+4,384);set(payload,at+8,420);set(payload,at+16,type);
                    const bool hidden=row==8 || row==17 || row==18 || row==20 || row==21;
                    set(payload,at+24,0x00200000U|(hidden?0x400U:0U));
                    set(payload,at+32,static_cast<std::uint32_t>(is_picture?block_offsets[1]:is_camera?block_offsets[0]:deferred));
                    if(is_character) {
                        set(payload,at+8,static_cast<std::uint32_t>(payload.size()));
                        word(payload,std::bit_cast<std::uint32_t>(row==8?80.F:100.F));
                        word(payload,std::bit_cast<std::uint32_t>(row==8?150.F:300.F));word(payload,0);
                        if(row!=8) add_attachment(at,"ZCHAROBJ_CharFader",0,1);
                    } else if(is_picture) {
                        add_attachment(at,row==9?"ZWINPIC_FadeToBlack":"ZWINPIC_LogoFade",0,1);
                    } else if(!is_camera) {
                        add_attachment(at,"ZLIST_ExternCutSequenceCommand",0x3f800000U,
                            row==11 || row==37 || row==38 || row==41?1U:2U);
                    }
                }
                while(payload.size()%4) payload.push_back(std::byte{0});
            }
            const auto visual_scope_records=payload.size();
            if(following_visual_scope) {
                payload.resize(payload.size()+6*48);
                for(std::size_t slot=0;slot<6;++slot) {
                    const auto at=visual_scope_records+slot*48;
                    set(payload,at,static_cast<std::uint32_t>(names.size()));text(names,"IndependentVisualScope"+std::to_string(slot));
                    set(payload,at+4,384);set(payload,at+8,420);
                    set(payload,at+12,slot==0 || slot==2?0U:static_cast<std::uint32_t>(0x1000+slot*16));
                    set(payload,at+16,slot==0?0x00100001U:0x0020003aU);
                    set(payload,at+24,slot==0?0x03000000U:slot<=2?0x00200000U:0U);
                    set(payload,at+32,static_cast<std::uint32_t>(deferred));
                }
            }
            if(room_animation_scope) {
                while(payload.size()%4) payload.push_back(std::byte{0});
                for(std::size_t slot=0;slot<synthetic_tail_count;++slot) {
                    while(payload.size()%4) payload.push_back(std::byte{0});
                    const auto at=payload.size();
                    tail_record_offsets.push_back(at);
                    payload.resize(at+48);
                    set(payload,at,static_cast<std::uint32_t>(names.size()));
                    text(names,"SyntheticTail"+std::to_string(slot));
                    set(payload,at+4,384);set(payload,at+8,420);
                    set(payload,at+16,0x0020003aU);
                    set(payload,at+24,0x00200000U);
                    set(payload,at+32,static_cast<std::uint32_t>(block_offsets[5]));
                    if(slot==component_slot) {
                        const auto table=payload.size();word(payload,1);
                        word(payload,static_cast<std::uint32_t>(table+12));
                        word(payload,0);text(payload,"ZSTDOBJ_VertAnim");
                        set(payload,at+20,static_cast<std::uint32_t>(table));
                    }
                }
                while(payload.size()%4) payload.push_back(std::byte{0});
            }
            for(std::size_t slot=0;slot<synthetic_fade_count;++slot) {
                while(payload.size()%4) payload.push_back(std::byte{0});
                const auto at=payload.size();
                synthetic_fade_record_offsets.push_back(at);
                payload.resize(at+48U);
                set(payload,at,static_cast<std::uint32_t>(names.size()));text(names,"SyntheticFadeTarget"+std::to_string(slot));
                set(payload,at+4U,384U);set(payload,at+8U,420U);set(payload,at+16U,0x00200046U);
                set(payload,at+24U,0x00200000U);set(payload,at+32U,static_cast<std::uint32_t>(block_offsets[1]));
                const auto table=payload.size();word(payload,1U);
                word(payload,static_cast<std::uint32_t>(table+12U));
                word(payload,0U);text(payload,"ZWINPIC_FadeToBlack");
                set(payload,at+20U,static_cast<std::uint32_t>(table));
                while(payload.size()%4) payload.push_back(std::byte{0});
            }
            for(std::size_t slot=0;slot<synthetic_external_count;++slot) {
                while(payload.size()%4) payload.push_back(std::byte{0});
                const auto at=payload.size();
                synthetic_external_record_offsets.push_back(at);
                payload.resize(at+48U);
                set(payload,at,static_cast<std::uint32_t>(names.size()));text(names,"SyntheticExternalOwner"+std::to_string(slot));
                set(payload,at+4U,384U);set(payload,at+8U,420U);set(payload,at+16U,0x0800001aU);
                set(payload,at+24U,0x00200000U);
                Bytes command_block(4);
                scalar(command_block,0x09U,4U);command_block.push_back(std::byte{0x06});
                for(std::size_t command=0;command<2U;++command) {
                    scalar(command_block,0x83U,static_cast<std::uint32_t>(17U+command));
                    scalar(command_block,0x8aU,static_cast<std::uint32_t>(0x80000020U+command));
                    scalar(command_block,0x88U,0U);
                    scalar(command_block,0x83U,static_cast<std::uint32_t>(70U+command));
                    command_block.push_back(std::byte{0x04});text(command_block,"");
                    scalar(command_block,0x88U,0U);command_block.push_back(std::byte{0x06});
                }
                finish(command_block);
                const auto deferred=payload.size();
                synthetic_external_command_offsets.push_back(deferred);
                set(payload,at+32U,static_cast<std::uint32_t>(deferred));
                payload.insert(payload.end(),command_block.begin(),command_block.end());
                while(payload.size()%4) payload.push_back(std::byte{0});
                const auto table=payload.size();word(payload,2U);payload.resize(payload.size()+16U);
                for(std::size_t component=0;component<2U;++component) {
                    set(payload,table+4U+component*8U,static_cast<std::uint32_t>(payload.size()));
                    set(payload,table+8U+component*8U,std::bit_cast<std::uint32_t>(1.0F));
                    text(payload,"ZLIST_ExternCutSequenceCommand");
                }
                set(payload,at+20U,static_cast<std::uint32_t>(table));
                while(payload.size()%4) payload.push_back(std::byte{0});
            }
            const auto directory=payload.size();
            word(payload,0);
            word(payload,static_cast<std::uint32_t>(record/4));word(payload,0);
            const Bytes entries(payload.begin()+36,payload.begin()+static_cast<std::ptrdiff_t>(36+8*node_count));
            if(language_group) {
                payload.insert(payload.end(),entries.begin(),entries.begin()+8);
                word(payload,(1U<<24U)|static_cast<std::uint32_t>(language_record/4));word(payload,0);
                // Center is inside Language; Fade pops back to Window. Keep
                // the remaining source roles intact through explicit mapping.
                payload.insert(payload.end(),entries.begin()+24,entries.begin()+32);
                word(payload,(1U<<25U)|static_cast<std::uint32_t>((512+48)/4));word(payload,0);
                if(camera_row) payload.insert(payload.end(),entries.begin()+64,entries.begin()+72);
                if(second_window) {
                    word(payload,(1U<<25U)|(1U<<24U)|static_cast<std::uint32_t>(second_window_record/4));word(payload,0);
                    word(payload,static_cast<std::uint32_t>((second_window_record+48)/4));word(payload,0);
                }
                if(full_second_scope) {
                    for(std::size_t row=0;row<34;++row) {
                        word(payload,static_cast<std::uint32_t>((full_scope_records+row*48)/4));word(payload,0);
                    }
                    if(following_visual_scope) {
                        word(payload,(1U<<25U)|(1U<<24U)|static_cast<std::uint32_t>(visual_scope_records/4));word(payload,0);
                        for(std::size_t slot=1;slot<6;++slot) {
                            word(payload,static_cast<std::uint32_t>((visual_scope_records+slot*48)/4));word(payload,0);
                        }
                    }
                    if(room_animation_scope) {
                        for(std::size_t slot=0;slot<tail_record_offsets.size();++slot) {
                            const auto pop=slot==0?1U:0U;
                            if(slot==0) tail_parent_control_offset=payload.size();
                            word(payload,(pop<<25U)|static_cast<std::uint32_t>(tail_record_offsets[slot]/4));
                            word(payload,0);
                        }
                    } else {
                        word(payload,(1U<<25U)|static_cast<std::uint32_t>((512+48*2)/4));word(payload,0);
                    }
                } else payload.insert(payload.end(),entries.begin()+16,entries.begin()+24);
                if(camera_row) {
                    payload.insert(payload.end(),entries.begin()+32,entries.begin()+64);
                    payload.insert(payload.end(),entries.begin()+72,entries.end());
                } else if(!camera_row) payload.insert(payload.end(),entries.begin()+32,entries.end());
                if(full_second_scope && room_animation_scope)
                    payload.insert(payload.end(),entries.begin()+16,entries.begin()+24);
            } else payload.insert(payload.end(),entries.begin(),entries.end());
            for(const auto record_offset:synthetic_fade_record_offsets) {
                word(payload,static_cast<std::uint32_t>(record_offset/4U));word(payload,0U);
            }
            for(const auto record_offset:synthetic_external_record_offsets) {
                word(payload,static_cast<std::uint32_t>(record_offset/4U));word(payload,0U);
            }
            const auto directory_count=(payload.size()-directory-sizeof(std::uint32_t))/8U;
            if((payload.size()-directory-sizeof(std::uint32_t))%8U!=0U)
                throw std::runtime_error("independent fixture directory entries are not word-pair aligned");
            set(payload,directory,static_cast<std::uint32_t>(directory_count));
            set(payload,0,static_cast<std::uint32_t>(directory));
            const auto read_payload_word=[&](std::size_t offset) {
                std::uint32_t value{};
                for(unsigned shift=0;shift<32;shift+=8)
                    value|=static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload.at(offset++)))<<shift;
                return value;
            };
            const auto emitted_directory_count=read_payload_word(directory);
            if(emitted_directory_count!=directory_count)
                throw std::runtime_error("independent fixture directory count does not match emitted entries");
            const auto directory_start=directory+4U;
            const auto reference_for_record=[&](std::size_t record_offset) {
                for(std::size_t index=0;index<directory_count;++index) {
                    const auto packed=read_payload_word(directory_start+index*8U);
                    if(static_cast<std::size_t>(packed&0x00ffffffU)*4U==record_offset)
                        return static_cast<std::uint32_t>(index+1U);
                }
                throw std::runtime_error("independent fixture reference target is absent from its directory");
            };
            const auto bind_authored_reference=[&](std::size_t block,std::size_t field,
                                                   std::uint32_t reference) {
                if(reference==0U) { set(payload,block_offsets[block]+field,0U); return; }
                const auto row=static_cast<std::size_t>(reference-1U);
                if(row>=node_count)
                    throw std::runtime_error("independent fixture authored reference has no source row");
                const auto original_entry=36U+row*8U;
                const auto target_record=static_cast<std::size_t>(read_payload_word(original_entry)&0x00ffffffU)*4U;
                set(payload,block_offsets[block]+field,reference_for_record(target_record));
            };
            bind_authored_reference(2,11,6U);bind_authored_reference(2,16,7U);
            bind_authored_reference(4,9,9U);bind_authored_reference(4,13,4U);
            bind_authored_reference(4,17,0U);bind_authored_reference(4,21,2U);
            bind_authored_reference(4,25,0U);bind_authored_reference(4,29,0U);
            bind_authored_reference(5,9,8U);bind_authored_reference(5,13,8U);
            bind_authored_reference(6,9,4U);bind_authored_reference(6,13,2U);
            bind_authored_reference(6,17,4U);
            bind_authored_reference(7,9,5U);
            for(std::size_t command=0;command<5U;++command) {
                if(synthetic_fade_record_offsets.empty())
                    bind_authored_reference(7,66U+40U*command,2U);
                else
                    set(payload,block_offsets[7]+66U+40U*command,
                        reference_for_record(synthetic_fade_record_offsets[command%synthetic_fade_record_offsets.size()]));
            }
            for(const auto command_block:synthetic_external_command_offsets) {
                const auto list_reference=reference_for_record(512U+48U*5U);
                set(payload,command_block+33U,list_reference);
                set(payload,command_block+61U,list_reference);
            }
            bind_authored_reference(8,32,9U);bind_authored_reference(8,37,0U);
            bind_authored_reference(8,42,2U);
            std::size_t pool_count=1;
            for(std::size_t index=0;index<directory_count;++index) {
                const auto packed=read_payload_word(directory_start+index*8U);
                const auto record=static_cast<std::size_t>(packed&0x00ffffffU)*4U;
                if(record>payload.size() || 48U>payload.size()-record)
                    throw std::runtime_error("independent fixture directory record exceeds its image");
                const auto basis=read_payload_word(record+4U);
                const auto position=read_payload_word(record+8U);
                if(basis>payload.size() || 36U>payload.size()-basis ||
                   position>payload.size() || 12U>payload.size()-position)
                    throw std::runtime_error("independent fixture transform exceeds its image at directory entry "+
                                             std::to_string(index));
                if((read_payload_word(record+16U)&0x00100000U)!=0U) ++pool_count;
            }
            std::vector<std::array<std::uint32_t,24>> pool_counts(pool_count);
            std::vector<std::size_t> active_groups{0};
            std::size_t next_group=1;
            for(std::size_t index=0;index<directory_count;++index) {
                const auto packed=read_payload_word(directory_start+index*8U);
                const auto parent_steps=static_cast<std::size_t>(packed>>25U);
                if(parent_steps>=active_groups.size())
                    throw std::runtime_error("independent fixture parent stack underflows");
                active_groups.resize(active_groups.size()-parent_steps);
                const auto record=static_cast<std::size_t>(packed&0x00ffffffU)*4U;
                const auto source_type=read_payload_word(record+16U);
                const auto variant=std::to_integer<std::uint8_t>(payload.at(record+45U));
                std::uint32_t source_class{};
                if(source_type==0x04000022U) source_class=1U;
                else if((source_type&0x00200000U)!=0U)
                    source_class=(source_type==0x00200012U || source_type==0x00200015U || source_type==0x0020001cU)?3U:1U;
                else if((source_type&0x00100000U)!=0U) source_class=0U;
                else source_class=(source_type&0x00800000U)!=0U?2U:3U;
                source_class+=static_cast<std::uint32_t>(variant)*8U;
                if(source_class>=24U || active_groups.back()>=pool_counts.size())
                    throw std::runtime_error("independent fixture source selects an invalid pool partition");
                ++pool_counts[active_groups.back()][source_class];
                if((source_type&0x00100000U)!=0U) ++next_group;
                if((packed&0x01000000U)!=0U) {
                    if(next_group==0U || next_group>pool_count)
                        throw std::runtime_error("independent fixture enters an undeclared pool");
                    active_groups.push_back(next_group-1U);
                }
            }
            if(next_group!=pool_count)
                throw std::runtime_error("independent fixture leaves a pool unused");
            const auto pools=payload.size();
            word(payload,static_cast<std::uint32_t>(pool_count));
            for(const auto& group:pool_counts)
                for(const auto count:group) word(payload,count);
            set(payload,20,static_cast<std::uint32_t>(pools));
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
            snd.resize(16);
            text(snd,"Independent/Sound.asset"); snd.resize(144);
            set(snd,0,96); set(snd,4,144); set(snd,8,3); set(snd,12,4);
            set(snd,128,1); set(snd,132,16); set(snd,136,0x1234);
            set(snd,140,std::bit_cast<std::uint32_t>(12.375F));
        }
    }
    void event_names(std::initializer_list<std::string_view> entries) {
        // GMS header table pointers require four-byte alignment, while the
        // preceding synthetic compact block can end at any byte offset.
        while (payload.size()%4U) payload.push_back(std::byte{0});
        const auto table=payload.size();
        set(payload,4,static_cast<std::uint32_t>(table));
        word(payload,static_cast<std::uint32_t>(entries.size()));
        payload.resize(payload.size()+entries.size()*4);
        std::size_t index=0;
        for(const auto entry:entries) {
            set(payload,table+4+4*index++,static_cast<std::uint32_t>(payload.size()));
            text(payload,entry);
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

// These are deliberately named functions, rather than noinline lambdas.  MSVC
// emits each lambda body as one function, so the earlier lambda wrappers still
// left their large exception-cleanup frames intact on the Windows test runner.
static OFF_NOINLINE void test_named_global_envelopes() {
    using off::graphics::IntroPreparedResources;
    static_assert(!std::is_copy_constructible_v<IntroPreparedResources> && std::is_move_constructible_v<IntroPreparedResources>);
    {
      Bytes image(40,std::byte{0x6d});
      constexpr std::uint32_t section_offset=7;
      constexpr std::uint32_t next_section_offset=25;
      image[7]=std::byte{'G'}; image[8]=std::byte{'l'}; image[9]=std::byte{'o'};
      image[10]=std::byte{'b'}; image[11]=std::byte{'a'}; image[12]=std::byte{'l'};
      image[13]=std::byte{0};
      image[14]=std::byte{10}; image[15]=std::byte{0};
      image[16]=std::byte{0}; image[17]=std::byte{0};
      image[18]=std::byte{0x88}; image[19]=std::byte{0x12}; image[20]=std::byte{0x34};
      image[21]=std::byte{0x56}; image[22]=std::byte{0x78}; image[23]=std::byte{0xff};
      image[24]=std::byte{0x99};
      const auto envelope=off::graphics::parse_intro_named_global_section_envelope(
          image,section_offset,next_section_offset);
      check(envelope.label=="Global" &&
                envelope.label.data()==reinterpret_cast<const char*>(image.data()+section_offset),
            "named/global envelope exposes the bounded label");
      check(envelope.tagged_block.size()==10 && envelope.tagged_block[0]==std::byte{10} &&
                envelope.tagged_block[4]==std::byte{0x88} && envelope.tagged_block[9]==std::byte{0xff},
            "named/global envelope preserves exactly the declared header-bearing tagged block");
      const auto profile=off::graphics::profile_intro_named_global_section(envelope);
      check(profile.body_bytes==6U && profile.tagged_values.encoded_values==1U &&
                profile.tagged_values.value_kinds[static_cast<std::size_t>(
                    off::data::CompactTypedValueKind::signed32)]==1U &&
                profile.tagged_values.attachment_delimiters==0U &&
                profile.tagged_values.continuation_values==0U,
            "named/global profile retains only the tagged-body framing");
      rejects([&] { (void)off::graphics::read_intro_named_global_word_pair(envelope); });
      rejects([&] { (void)off::graphics::parse_intro_named_global_section_envelope(image,0,next_section_offset); });
      rejects([&] { (void)off::graphics::parse_intro_named_global_section_envelope(image,section_offset,0); });
      rejects([&] { (void)off::graphics::parse_intro_named_global_section_envelope(image,next_section_offset,section_offset); });
      rejects([&] { (void)off::graphics::parse_intro_named_global_section_envelope(image,section_offset,41); });
      Bytes unterminated=image;
      for(std::size_t index=section_offset;index<next_section_offset;++index) unterminated[index]=std::byte{'x'};
      rejects([&] { (void)off::graphics::parse_intro_named_global_section_envelope(
          unterminated,section_offset,next_section_offset); });
      Bytes short_block(12,std::byte{0x6d});
      short_block[7]=std::byte{'X'}; short_block[8]=std::byte{0};
      rejects([&] { (void)off::graphics::parse_intro_named_global_section_envelope(short_block,7,12); });
      Bytes oversized=image;
      set(oversized,14,12U);
      rejects([&] { (void)off::graphics::parse_intro_named_global_section_envelope(
          oversized,section_offset,next_section_offset); });
      Bytes header_only(13,std::byte{});
      header_only[7]=std::byte{'X'}; header_only[8]=std::byte{};
      header_only[9]=std::byte{4};
      const auto header_only_envelope=off::graphics::parse_intro_named_global_section_envelope(
          header_only,7,13);
      rejects([&] { (void)off::graphics::profile_intro_named_global_section(header_only_envelope); });
      Bytes word_pair(24,std::byte{});
      word_pair[7]=std::byte{'X'}; word_pair[8]=std::byte{};
      word_pair[9]=std::byte{16};
      word_pair[13]=std::byte{0x83}; word_pair[14]=std::byte{0x44};
      word_pair[15]=std::byte{0x33}; word_pair[16]=std::byte{0x22}; word_pair[17]=std::byte{0x11};
      word_pair[18]=std::byte{0x88}; word_pair[19]=std::byte{0x88};
      word_pair[20]=std::byte{0x77}; word_pair[21]=std::byte{0x66}; word_pair[22]=std::byte{0x55};
      word_pair[23]=std::byte{0x06};
      // The terminator lies at byte 24, so use a one-byte larger source boundary.
      word_pair.push_back(std::byte{0xff});
      const auto word_pair_envelope=off::graphics::parse_intro_named_global_section_envelope(
          word_pair,7,25);
      const auto pair=off::graphics::read_intro_named_global_word_pair(word_pair_envelope);
      check(pair.first_word==0x11223344U && pair.second_word==0x55667788U &&
                pair.first_tag==0x83U && pair.second_tag==0x88U,
            "named/global word-pair reader retains bounded opaque words and tags");
    }
}

static OFF_NOINLINE void test_prepared_runtime_scopes() {
    {
      Bytes image(32, std::byte{0x6d});
      constexpr std::uint32_t section_offset=12;
      set(image,section_offset,3);
      image[section_offset+4]=std::byte{0x11};
      image[section_offset+5]=std::byte{0x22};
      image[section_offset+6]=std::byte{0x33};
      bool called=false;
      const auto container=off::graphics::parse_intro_renderer_resource_envelope(
          image,section_offset,[&](std::span<const std::byte> payload) {
            called=true;
            check(payload.size()==3 && payload[0]==std::byte{0x11} &&
                      payload[1]==std::byte{0x22} && payload[2]==std::byte{0x33},
                  "renderer envelope passes exactly the framed payload");
            check(payload.data()!=image.data()+section_offset+sizeof(std::uint32_t),
                  "renderer envelope does not borrow the framed source payload");
            return off::graphics::IntroRendererResourceContainer{81};
          });
      check(called && container.identity==81,
            "renderer envelope invokes the required container parser exactly once");
      rejects([&] { (void)off::graphics::parse_intro_renderer_resource_envelope(image,0,{}); });
      rejects([&] { (void)off::graphics::parse_intro_renderer_resource_envelope(
          std::span<const std::byte>{image.data(),image.size()}.first(section_offset+3),section_offset,{}); });
      Bytes oversized=image;
      set(oversized,section_offset,static_cast<std::uint32_t>(oversized.size()));
      rejects([&] { (void)off::graphics::parse_intro_renderer_resource_envelope(
          oversized,section_offset,[](auto) { return off::graphics::IntroRendererResourceContainer{1}; }); });
      rejects([&] { (void)off::graphics::parse_intro_renderer_resource_envelope(image,section_offset,{}); });
    }
    Fixture fixture;
    {
      off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
      off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
      off::graphics::IntroRuntime host(fixture.build(),app,sequence);
      const auto snapshot=off::graphics::build_intro_preview(host,1,{1280,720});
      check(snapshot.target.width==1280 && snapshot.target.height==720 &&
                snapshot.draw.source_index==1 && !snapshot.draw.draw_plan.groups().empty() &&
                snapshot.images.size()==1 && !snapshot.images[0].mip_zero.pixels.empty(),
            "intro preview snapshots an exact retained picture and its image");
      rejects([&]{(void)off::graphics::build_intro_preview(
          host,1,{1280,720},
          off::graphics::IntroPreviewPolicy::admitted_first_cut_legal_picture);});
      rejects([&]{(void)off::graphics::build_intro_preview(
          host,1,{1280,720},
          off::graphics::IntroPreviewPolicy::admitted_first_cut_fade_picture);});
      rejects([&]{(void)off::graphics::build_incomplete_intro_fallback(
          host,{1280,720});});
      rejects([&]{(void)off::graphics::build_admitted_first_cut_fade_previews(
          host,{1280,720});});
      rejects([&]{(void)off::graphics::build_intro_preview(host,1,{0,720});});
      rejects([&]{(void)off::graphics::build_intro_preview(host,0,{1280,720});});
      rejects([&]{(void)off::graphics::build_intro_preview(host,999,{1280,720});});
      rejects([&]{(void)off::graphics::build_intro_preview(host,1,{1280,720},
          static_cast<off::graphics::IntroPreviewPolicy>(99));});
    }
    {
      off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
      off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
      bool missing_runtime_rejected = false;
      try {
        static_cast<void>(off::graphics::make_normal_intro_scene_session({}));
      } catch (const std::invalid_argument&) {
        missing_runtime_rejected = true;
      }
      check(missing_runtime_rejected,
            "normal scene session rejects a missing owned runtime");
      auto session = off::graphics::make_normal_intro_scene_session(
          std::make_unique<off::graphics::IntroRuntime>(fixture.build(), app, sequence));
      check(session->stage()==off::graphics::NormalIntroSceneSessionStage::postconstructed &&
                session->first_cut_player()==nullptr && session->first_cut_command_runner()==nullptr &&
                session->reader_bracket_observation().external_loader_values.empty() &&
                session->reader_bracket_observation().source_scripts.empty() &&
                session->reader_bracket_observation().pre_reader_calls==0U &&
                session->reader_bracket_observation().prepared_reader_calls==0U &&
                session->reader_bracket_observation().end_reader_calls==0U,
            "normal scene session starts without reader-bracket observations or a detached first-cut session");
      rejects([&] { session->prepare_supported_first_cut_player(); });
      rejects([&] { session->prepare_first_cut_command_runner(1.0F,{}); });
      check(session->stage()==off::graphics::NormalIntroSceneSessionStage::postconstructed &&
                session->first_cut_player()==nullptr && session->first_cut_command_runner()==nullptr,
            "first-cut preparation rejects before the owned reader bracket without changing session state");
    }
    {
      auto application=std::make_shared<off::runtime::ApplicationServices>(
          off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          off::runtime::ClockSamplingServices{[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
      auto sequence=std::make_shared<off::runtime::SceneComponentSequence>([application] {
        const auto sample=application->component_dispatch_time();
        if(!sample) throw std::runtime_error("synthetic component dispatch time unavailable");
        return *sample;
      });
      bool missing_application=false, missing_sequence=false;
      try { (void)off::graphics::OwnedNormalIntroScene::create(
          fixture.build(),{},sequence,"synthetic.gms",off::graphics::IntroSoundLoadPolicy::directory_construction); }
      catch(const std::invalid_argument&) { missing_application=true; }
      try { (void)off::graphics::OwnedNormalIntroScene::create(
          fixture.build(),application,{},"synthetic.gms",off::graphics::IntroSoundLoadPolicy::directory_construction); }
      catch(const std::invalid_argument&) { missing_sequence=true; }
      check(missing_application && missing_sequence,"owned scene rejects either absent dependency owner");
      auto owner=off::graphics::OwnedNormalIntroScene::create(
          fixture.build(),application,sequence,"synthetic.gms",off::graphics::IntroSoundLoadPolicy::directory_construction);
      check(&owner->session().runtime().application()==application.get() &&
            owner->session().stage()==off::graphics::NormalIntroSceneSessionStage::postconstructed,
            "owned scene constructs its session with the retained application and no activation");
      std::weak_ptr<off::graphics::OwnedNormalIntroScene> scene_lifetime=owner;
      std::weak_ptr<off::runtime::ApplicationServices> app_lifetime=application;
      std::weak_ptr<off::runtime::SceneComponentSequence> sequence_lifetime=sequence;
      auto lease=owner->lifetime_owner();
      owner.reset(); sequence.reset(); application.reset();
      check(!scene_lifetime.expired() && !sequence_lifetime.expired() && !app_lifetime.expired(),
            "scene lifetime owner retains session and both borrowed dependencies");
      lease.reset();
      check(scene_lifetime.expired() && sequence_lifetime.expired() && app_lifetime.expired(),
            "scene lifetime owner releases all dependencies without a cycle");
    }
    {
      off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
      const auto token=app.create_handle_collection();
      auto* retained=app.resolve_handle_collection(token);
      check(token!=0 && retained && retained->members.empty() && !app.resolve_handle_collection(0),
            "application object tokens resolve stable concrete collections, not scene resource handles");
      retained->members.push_back(123);
      for(int i=0;i<100;++i) static_cast<void>(app.create_handle_collection());
      check(app.resolve_handle_collection(token)==retained &&
            std::as_const(app).resolve_handle_collection(token)->members==std::vector<std::uint64_t>{123} &&
            !app.resolve_handle_collection(std::numeric_limits<std::uint64_t>::max()),
            "later object registrations preserve collection identity, contents and invalid-token distinction");
      check(!app.vert_anim_event() && !app.cut_sequence_lists(),"shared component class state starts absent");
      app.set_vert_anim_event(17);app.create_cut_sequence_list_collection();
      auto* shared=app.cut_sequence_lists();shared->members.push_back(456);
      rejects([&]{app.create_cut_sequence_list_collection();});
      app.set_vert_anim_event(29);
      check(app.vert_anim_event()==29 && app.cut_sequence_lists()==shared && shared->members==std::vector<std::uint64_t>{456},
            "shared event slot replaces its value while repeated collection creation never resets existing members");
      app.initialize_native_room_animation_scope_registration();
      static_cast<void>(app.register_class_instance(0x00100021U));
      rejects([&]{app.initialize_native_room_animation_scope_registration();});
      check(app.class_notification_sequence(0x00100021U)==1 && app.class_notification_sequence(0x00200002U)==0 &&
            app.component_class_notification_sequence("ZLIST_CutSequenceList")==0 && app.vert_anim_event()==29 &&
            app.cut_sequence_lists()==shared,"registration preserves unrelated application state and rejects duplicate reset");
    }
}

static OFF_NOINLINE void test_generic_directory_tail_lifecycle() {
    const auto initialize_app=[](off::runtime::ApplicationServices& target) {
        target.initialize_native_group_registration();
        target.initialize_native_window_language_registration();
        target.initialize_native_picture_registration();
        target.initialize_native_camera_registration();
        target.initialize_native_second_window_scope_registration();
        target.initialize_native_visual_registration();
        target.initialize_native_room_animation_scope_registration();
        target.initialize_native_lens_flare_animation_scope_registration();
        target.initialize_native_remaining_intro_scope_registration();
    };
    const auto construct_directory=[](off::graphics::IntroRuntime& target) {
        target.construct_root();
        target.begin_source_loading_without_engine_renderer();
        target.construct_source_directory_without_engine_renderer();
    };
    Fixture fixture(false,true);
    off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    initialize_app(app);
    off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
    auto prepared=fixture.build();
    const auto& authored_sources=prepared.sources();
    const auto source_index_for_node=[&](std::size_t node) {
        const auto record_offset=512U+48U*node;
        const auto found=std::ranges::find(authored_sources.directory(),record_offset,
            &off::data::GmsDirectoryEntry::record_offset);
        if(found==authored_sources.directory().end())
            throw std::runtime_error("required synthetic role is absent from the parsed directory");
        return static_cast<std::size_t>(std::distance(authored_sources.directory().begin(),found));
    };
    const auto camera_source=source_index_for_node(0);
    const auto member_source=source_index_for_node(4);
    const auto cut_source=source_index_for_node(7);
    const auto window_source=source_index_for_node(8);
    std::size_t controller_candidates=0;
    for(std::size_t source=0;source<authored_sources.directory().size();++source) {
        const auto& entry=authored_sources.directory()[source];
        for(std::size_t attachment=0;attachment<entry.attachments.size();++attachment)
            if(authored_sources.attachment_identifier(source,attachment)=="ZGEOM_MovieControl") {
                static_cast<void>(authored_sources.intro_movie_controller_source(source));
                ++controller_candidates;
            }
    }
    check(authored_sources.directory()[camera_source].source_type==0x00400003U &&
          authored_sources.directory()[member_source].source_type==0x0800001aU &&
          authored_sources.directory()[cut_source].source_type==0x0800001aU &&
          authored_sources.directory()[window_source].source_type==0x00100030U &&
          controller_candidates==1,
          "independent fixture resolves its camera, member, cut, window, and unique typed controller from source records");
    off::graphics::IntroRuntime host(std::move(prepared),app,sequence,"synthetic-scene",
        off::graphics::IntroSoundLoadPolicy::directory_construction);
    host.construct_root();
    host.begin_source_loading_without_engine_renderer();
    const auto& directory=host.resources().sources().directory();
    const auto unprocessed_begin=host.loaded_resource_handles().size();
    const auto unprocessed_count=directory.size()-unprocessed_begin;
    const auto readers_before=host.deferred_reader_work().size();
    const auto identities_before=sequence.live_count();
    host.construct_source_directory_without_engine_renderer();
    check(host.resource_load_stage()==off::graphics::IntroResourceLoadStage::directory_construction_complete &&
          host.loaded_resource_handles().size()==directory.size() &&
          host.directory_resource_mapping().size()==directory.size() &&
          std::ranges::all_of(std::span{host.directory_resource_mapping()}.subspan(unprocessed_begin),
                              [](const auto& resource){return resource.has_value();}),
          "whole-directory construction maps every supplied source record before completing its lifecycle");
    check(host.deferred_reader_work().size()==readers_before+unprocessed_count &&
          sequence.live_count()>identities_before,
          "remaining reader work and component ownership are derived from supplied fixture records");

    for(std::size_t source=0;source<directory.size();++source) {
        const auto owner=host.source_handle(source);
        const auto& record=directory[source];
        const auto resource=host.directory_resource_mapping()[source];
        const auto parent=host.resources().sources().hierarchy()[source].parent_directory_index;
        const auto expected_parent=parent?host.resource_handle(host.source_handle(*parent)):
                                          host.resource_handle(host.root_handle());
        check(resource && *resource==host.resource_handle(owner) &&
              host.resource_state(owner)->directory_auxiliary==record.auxiliary_value &&
              host.resource_parent(owner)==expected_parent,
              "constructed resource mapping retains its own source identity and record metadata");
        const auto found=std::ranges::find(host.deferred_reader_work(),source,
            &off::graphics::IntroDeferredReaderWork::source_directory_index);
        if(record.deferred_source_offset) {
            check(found!=host.deferred_reader_work().end() && found->resource==host.resource_handle(owner) &&
                  found->source_offset==record.deferred_source_offset,
                  "deferred work preserves owner, resource, source index, and source offset");
        } else check(found==host.deferred_reader_work().end(),
                     "sources without deferred blocks do not acquire fabricated reader work");
    }

    std::optional<std::size_t> component_source;
    for(std::size_t source=0;source<directory.size();++source)
        for(std::size_t slot=0;slot<directory[source].attachments.size();++slot)
            if(host.resources().sources().attachment_identifier(source,slot)=="ZWINPIC_FadeToBlack")
                component_source=source;
    check(component_source.has_value(),"the small fixture includes one typed fade picture role");
    const auto component_indices=host.owner_components(host.source_handle(*component_source));
    check(component_indices.size()==1 &&
          host.components().at(component_indices.front()).source().factory_name=="ZWINPIC_FadeToBlack" &&
          host.components().at(component_indices.front()).state().attached_owner==
              host.source_handle(*component_source).value,
          "a component remains attached to its canonical whole-directory owner");

    for(const auto& work:host.deferred_reader_work())
        check(!host.supports_external_cut_commands_deferred_reader(work),
              "an ordinary record does not qualify for the typed command reader");

    std::vector<std::size_t> prepared_sources,owner_sources,component_sources;
    std::vector<std::string> bracket_events;
    off::graphics::IntroPostconstructionReaderServices reader_services{
        [&](std::uint64_t){bracket_events.push_back("external");},
        [&](const auto&){bracket_events.push_back("script");},
        [&]{bracket_events.push_back("pre");},
        [&](const auto& work){
            check(work.processed,"reader boundary marks work before project-supplied parsing");
            prepared_sources.push_back(work.source_directory_index);
            bracket_events.push_back("prepare");
        },
        [&](const auto& work){owner_sources.push_back(work.source_directory_index);bracket_events.push_back("owner");},
        [&](const auto& work){component_sources.push_back(work.source_directory_index);bracket_events.push_back("component");},
        [&]{bracket_events.push_back("end");}};
    std::vector<std::size_t> expected_reader_sources;
    for(const auto& work:host.deferred_reader_work())
        expected_reader_sources.push_back(work.source_directory_index);
    host.run_postconstruction_reader_bracket(0x91U,reader_services);
    check(host.reader_bracket_stage()==off::graphics::IntroReaderBracketStage::ordinary_reader_boundary_complete &&
          prepared_sources==expected_reader_sources && owner_sources==expected_reader_sources &&
          component_sources==expected_reader_sources && !bracket_events.empty() &&
          bracket_events.front()=="external" && bracket_events.back()=="end",
          "reader services retain source order and explicit prepare/owner/component boundaries");
    rejects([&]{host.run_postconstruction_reader_bracket(0x91U,reader_services);});

    const auto lifecycle=host.preflight_global_lifecycle();
    check(!lifecycle.ready() && !host.components().phases_completed() &&
          host.registered_cameras().entries().empty(),
          "local source construction does not claim global lifecycle admission, camera registration, or presentation");
    rejects([&]{host.construct_source_directory_without_engine_renderer();});

    Fixture alternate(true,true);
    off::runtime::ApplicationServices alternate_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    initialize_app(alternate_app);
    off::runtime::SceneComponentSequence alternate_sequence{[]{return std::uint32_t{1};}};
    off::graphics::IntroRuntime alternate_host(alternate.build(),alternate_app,alternate_sequence,
        "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
    construct_directory(alternate_host);
    const auto alternate_count=alternate_host.resources().sources().directory().size();
    check(alternate_count>directory.size() && alternate_host.loaded_resource_handles().size()==alternate_count &&
          alternate_host.directory_resource_mapping().size()==alternate_count &&
          std::ranges::all_of(alternate_host.directory_resource_mapping(),[](const auto& row){return row.has_value();}),
          "a different small whole-directory variant maps every parsed source row");

    Fixture malformed(true,true);
    set(malformed.payload,512U+48U*9U+16U,0x0020001cU);
    off::runtime::ApplicationServices failing_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    initialize_app(failing_app);
    off::runtime::SceneComponentSequence failing_sequence{[]{return std::uint32_t{1};}};
    off::graphics::IntroRuntime failing_host(malformed.build(),failing_app,failing_sequence,"synthetic-scene",
        off::graphics::IntroSoundLoadPolicy::directory_construction);
    failing_host.construct_root();
    failing_host.begin_source_loading_without_engine_renderer();
    const auto& malformed_directory=failing_host.resources().sources().directory();
    const auto invalid_found=std::ranges::find(malformed_directory,512U+48U*9U,
        &off::data::GmsDirectoryEntry::record_offset);
    if(invalid_found==malformed_directory.end()) throw std::runtime_error("unsupported source is absent from fixture");
    const auto invalid_source=static_cast<std::size_t>(std::distance(malformed_directory.begin(),invalid_found));
    rejects([&]{failing_host.construct_source_directory_without_engine_renderer();});
    check(failing_host.resource_load_stage()==off::graphics::IntroResourceLoadStage::failed &&
          failing_host.loaded_resource_handles().size()==invalid_source &&
          !failing_host.directory_resource_mapping()[invalid_source].has_value(),
          "an unsupported source fails closed while retaining only the completed directory prefix");

    Fixture parent_pop(false,true);
    const auto directory_offset=[&] {
        std::uint32_t value{};
        for(unsigned shift=0;shift<32;shift+=8)
            value|=static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(parent_pop.payload.at(shift/8)))<<shift;
        return static_cast<std::size_t>(value);
    }();
    set(parent_pop.payload,directory_offset+4U,(127U<<25U)|
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(parent_pop.payload.at(directory_offset+4U))) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(parent_pop.payload.at(directory_offset+5U)))<<8U) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(parent_pop.payload.at(directory_offset+6U)))<<16U)));
    rejects([&]{parent_pop.image();});
}

static OFF_NOINLINE void test_visual_scope_lifecycle() {
    const auto initialize_app=[](off::runtime::ApplicationServices& target) {
        target.initialize_native_group_registration();
        target.initialize_native_window_language_registration();
        target.initialize_native_picture_registration();
        target.initialize_native_camera_registration();
        target.initialize_native_second_window_scope_registration();
        target.initialize_native_visual_registration();
        target.initialize_native_room_animation_scope_registration();
        target.initialize_native_lens_flare_animation_scope_registration();
        target.initialize_native_remaining_intro_scope_registration();
    };
    const auto construct_directory=[](off::graphics::IntroRuntime& target) {
        target.construct_root();
        target.begin_source_loading_without_engine_renderer();
        target.construct_source_directory_without_engine_renderer();
    };
    const auto find_record=[](const auto& directory,std::size_t offset) {
        const auto found=std::ranges::find(directory,offset,&off::data::GmsDirectoryEntry::record_offset);
        if(found==directory.end()) throw std::runtime_error("independent reader fixture record is absent");
        return static_cast<std::size_t>(std::distance(directory.begin(),found));
    };
    const auto run_readers=[](off::graphics::IntroRuntime& host,
                              const std::vector<std::size_t>& fade_targets,
                              std::optional<std::size_t> omitted_fade_component={}) {
        off::graphics::IntroPostconstructionReaderServices services{
            [](std::uint64_t){},[](const auto&){},[]{},[](const auto&){},
            [&](const auto& work) {
                if(host.supports_external_cut_commands_deferred_reader(work))
                    host.apply_supported_external_cut_commands_deferred_reader(work);
                if(std::ranges::find(fade_targets,work.source_directory_index)!=fade_targets.end())
                    host.apply_supported_first_cut_fade_picture_deferred_reader(work);
            },
            [&](const auto& work) {
                if(work.source_directory_index!=omitted_fade_component &&
                   std::ranges::find(fade_targets,work.source_directory_index)!=fade_targets.end())
                    host.apply_supported_first_cut_fade_picture_component_reader(work);
            },[]{}};
        host.run_postconstruction_reader_bracket(41U,services);
    };
    const auto fade_targets=[](const off::graphics::IntroRuntime& host) {
        std::vector<std::size_t> result;
        for(const auto& command:host.resources().first_cut().commands) {
            const auto source=host.resources().sources().local_source_for_authored_reference(command.target_reference);
            if(source && std::ranges::find(result,*source)==result.end()) result.push_back(*source);
        }
        return result;
    };
    const auto bind_fade=[](off::graphics::IntroRuntime& host) {
        host.bind_first_cut_fade_phase_one_services({[]{return std::array<std::int32_t,2>{640,480};},[](auto,auto){}});
    };

    {
        Fixture fixture(true,true);
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        initialize_app(app);
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        auto runtime=std::make_unique<off::graphics::IntroRuntime>(fixture.build(),app,sequence,
            "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
        construct_directory(*runtime);
        const auto& directory=runtime->resources().sources().directory();
        const auto row=std::ranges::find(directory,0x00200012U,&off::data::GmsDirectoryEntry::source_type);
        if(row==directory.end()) throw std::runtime_error("synthetic sound owner is absent");
        const auto source=static_cast<std::size_t>(std::distance(directory.begin(),row));
        const auto work=std::ranges::find(runtime->deferred_reader_work(),source,
            &off::graphics::IntroDeferredReaderWork::source_directory_index);
        if(work==runtime->deferred_reader_work().end())
            throw std::runtime_error("synthetic sound deferred work is absent");
        auto& sound=runtime->sound_for_source(source);
        auto& record=sound.record();
        const auto before=record;
        auto copied=*work;
        copied.processed=true;
        rejects([&]{runtime->apply_supported_sound_owner_deferred_reader(copied);});
        const auto coverage=runtime->reader_coverage_inventory();
        check(!sound.source_applied() && record.type==before.type &&
              record.active_source==before.active_source && record.flags==before.flags &&
              record.category==before.category && record.gain_multiplier==before.gain_multiplier &&
              record.source_scalars==before.source_scalars && record.range==before.range &&
              record.derived_range==before.derived_range && record.final_scalar==before.final_scalar &&
              app.sound_records().prepared().empty() &&
              std::ranges::any_of(coverage.entries,[](const auto& entry) {
                  return entry.family==off::graphics::IntroDeferredReaderFamily::sound_owner &&
                      entry.state==off::graphics::IntroDeferredReaderImplementationState::implemented_not_applied &&
                      entry.count==1U;
              }),"copied sound work cannot mutate the backend or publish reader admission");
        auto session=off::graphics::make_normal_intro_scene_session(std::move(runtime));
        session->complete_postconstruction_reader_bracket(41U);
        check(session->stage()==off::graphics::NormalIntroSceneSessionStage::reader_bracket_complete &&
              session->runtime().sound_for_source(source).source_applied(),
              "the real normal bracket applies the canonical sound work");
    }

    {
        Fixture fixture(false,true);
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        initialize_app(app);
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        auto runtime=std::make_unique<off::graphics::IntroRuntime>(fixture.build(),app,sequence,
            "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
        construct_directory(*runtime);
        const auto source=runtime->resources().window_index();
        const auto work=std::ranges::find(runtime->deferred_reader_work(),source,
            &off::graphics::IntroDeferredReaderWork::source_directory_index);
        if(work==runtime->deferred_reader_work().end())
            throw std::runtime_error("synthetic Window deferred work is absent");
        auto copied=*work;
        copied.processed=true;
        rejects([&]{runtime->apply_supported_window_deferred_reader(copied);});
        const auto coverage=runtime->reader_coverage_inventory();
        check(!runtime->window_camera_projection_applied(),
              "copied Window work cannot publish projection before its reader bracket");
        check(std::ranges::any_of(coverage.entries,[](const auto& entry) {
                  return entry.family==off::graphics::IntroDeferredReaderFamily::window_owner &&
                      entry.state==off::graphics::IntroDeferredReaderImplementationState::implemented_not_applied &&
                      entry.count==1U;
              }),"copied Window work does not publish a reader admission");
        auto session=off::graphics::make_normal_intro_scene_session(std::move(runtime));
        session->complete_postconstruction_reader_bracket(41U);
        check(session->runtime().window_camera_projection_applied() &&
              session->stage()==off::graphics::NormalIntroSceneSessionStage::reader_bracket_complete,
              "the canonical Window work remains readable by the normal bracket");
    }

    for(const bool variant_one : {true,false}) {
        Fixture fixture(false,true);
        const auto read_word=[&](std::size_t offset) {
            std::uint32_t value{};
            for(unsigned i=0;i<4;++i)
                value|=std::to_integer<std::uint32_t>(fixture.payload.at(offset+i))<<(8U*i);
            return value;
        };
        const auto directory=read_word(0U);
        const auto group_record=static_cast<std::size_t>(read_word(directory+4U)&0x00ffffffU)*4U;
        const auto pool=read_word(20U);
        if(read_word(group_record+16U)!=0x00100001U || read_word(pool+4U)==0U)
            throw std::runtime_error("synthetic leading group fixture is not available");
        if(variant_one) {
            fixture.payload.at(group_record+45U)=std::byte{1};
            set(fixture.payload,pool+4U,read_word(pool+4U)-1U);
            set(fixture.payload,pool+4U+8U*4U,read_word(pool+4U+8U*4U)+1U);
        } else {
            const auto block=read_word(group_record+32U);
            fixture.payload.at(block+9U)=std::byte{0x42}; // unsupported scalar tag
        }
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        initialize_app(app);
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        auto runtime=std::make_unique<off::graphics::IntroRuntime>(fixture.build(),app,sequence,
            "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
        construct_directory(*runtime);
        const auto source=find_record(runtime->resources().sources().directory(),group_record);
        check(runtime->resources().sources().directory()[source].source_variant==(variant_one?1U:0U),
              "synthetic group variant reaches the constructed normal scene");
        auto session=off::graphics::make_normal_intro_scene_session(std::move(runtime));
        session->complete_postconstruction_reader_bracket(41U);
        auto& host=session->runtime();
        const auto work=std::ranges::find(host.deferred_reader_work(),source,
            &off::graphics::IntroDeferredReaderWork::source_directory_index);
        check(work!=host.deferred_reader_work().end() && !host.is_basic_group_owner_reader_source(source),
              "unsupported source is not classified as a basic-group reader");
        if(work==host.deferred_reader_work().end())
            throw std::runtime_error("synthetic group deferred work is absent");
        rejects([&]{host.apply_supported_basic_group_owner_deferred_reader(*work);});
        const auto coverage=host.reader_coverage_inventory();
        const auto group_work_count=std::ranges::count_if(host.deferred_reader_work(),[&](const auto& entry) {
            return host.resources().sources().directory()[entry.source_directory_index].source_type==0x00100001U;
        });
        check(group_work_count==1U,"the fixture has exactly one basic-group work item");
        check(!host.basic_group_owner_reader_states().contains(source) &&
              std::ranges::any_of(coverage.entries,[](const auto& entry) {
                  return entry.source_type==0x00100001U &&
                      entry.family==off::graphics::IntroDeferredReaderFamily::unclassified &&
                      entry.state==off::graphics::IntroDeferredReaderImplementationState::unimplemented &&
                      entry.count==1U;
              }),"unsupported variant or malformed basic-group form does not receive reader coverage");
    }

    Fixture unique_external(false,true,false,true,false,false,false,false,false,3,1,1,0);
    off::runtime::ApplicationServices external_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    initialize_app(external_app);
    off::runtime::SceneComponentSequence external_sequence{[]{return std::uint32_t{1};}};
    off::graphics::IntroRuntime external_host(unique_external.build(),external_app,external_sequence,
        "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
    construct_directory(external_host);
    const auto external_source=find_record(external_host.resources().sources().directory(),
        unique_external.synthetic_external_record_offsets.front());
    auto external_work=std::ranges::find(external_host.deferred_reader_work(),external_source,
        &off::graphics::IntroDeferredReaderWork::source_directory_index);
    check(external_work!=external_host.deferred_reader_work().end() &&
          !external_host.supports_external_cut_commands_deferred_reader(*external_work),
          "typed external-command candidate remains unavailable until its ordinary reader boundary");
    run_readers(external_host,{});
    external_work=std::ranges::find(external_host.deferred_reader_work(),external_source,
        &off::graphics::IntroDeferredReaderWork::source_directory_index);
    check(external_work!=external_host.deferred_reader_work().end() &&
          external_host.supports_external_cut_commands_deferred_reader(*external_work),
          "the unique parser-valid typed command owner is selected at its dynamically resolved source index");
    const auto* external_state=external_host.external_cut_commands_reader_state();
    check(external_state && external_state->owner==external_host.source_handle(external_source) &&
          external_state->component_indices.size()==2 &&
          external_state->commands[0].timeline_position==17U &&
          external_state->commands[1].timeline_position==18U,
          "typed external reader preserves its two ordered commands and canonical owner");

    Fixture ambiguous_external(false,true,false,true,false,false,false,false,false,3,1,2,0);
    off::runtime::ApplicationServices ambiguous_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    initialize_app(ambiguous_app);
    off::runtime::SceneComponentSequence ambiguous_sequence{[]{return std::uint32_t{1};}};
    off::graphics::IntroRuntime ambiguous_host(ambiguous_external.build(),ambiguous_app,ambiguous_sequence,
        "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
    construct_directory(ambiguous_host);
    run_readers(ambiguous_host,{});
    std::size_t ambiguous_candidates=0;
    for(const auto& work:ambiguous_host.deferred_reader_work())
        if(ambiguous_host.resources().sources().directory()[work.source_directory_index].attachments.size()==2U &&
           ambiguous_host.resources().sources().attachment_identifier(work.source_directory_index,0)==
               "ZLIST_ExternCutSequenceCommand") {
            ++ambiguous_candidates;
            check(!ambiguous_host.supports_external_cut_commands_deferred_reader(work),
                  "two valid typed candidates make external-command selection ambiguous");
            rejects([&]{ambiguous_host.apply_supported_external_cut_commands_deferred_reader(work);});
        }
    check(ambiguous_candidates==2U && !ambiguous_host.external_cut_commands_reader_state(),
          "ambiguous candidates do not publish a reader state");

    const auto check_fade_fixture=[&](std::size_t unique_targets,bool omit_one_component) {
        Fixture fixture(false,true,false,true,false,false,false,false,false,3,1,0,unique_targets);
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        initialize_app(app);
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        off::graphics::IntroRuntime host(fixture.build(),app,sequence,"synthetic-scene",
            off::graphics::IntroSoundLoadPolicy::directory_construction);
        construct_directory(host);
        const auto targets=fade_targets(host);
        check(targets.size()==unique_targets,
              "fade target discovery counts unique parsed source identities, not command occurrences");
        run_readers(host,targets,omit_one_component && targets.size()>1U?
            std::optional<std::size_t>{targets.back()}:std::nullopt);
        check(host.fade_picture_reader_states().size()==targets.size() &&
              host.fade_picture_component_reader_states().size()==targets.size()-(omit_one_component?1U:0U),
              "owner and component fade readers retain the complete dynamic target set");
        if(omit_one_component) {
            rejects([&]{bind_fade(host);});
        } else {
            bind_fade(host);
        }
    };
    check_fade_fixture(1U,false);  // Five command occurrences alias one authored owner.
    check_fade_fixture(2U,false);  // Two distinct owners are complete and admissible.
    check_fade_fixture(2U,true);   // Missing one component boundary fails closed.

    for(const bool throw_callback : {false,true}) {
        Fixture display_fixture(false,true);
        // Synthetic scalar fields in the camera block: selector one, mode two.
        set(display_fixture.payload,display_fixture.block_offsets[0]+61U,1U);
        set(display_fixture.payload,display_fixture.block_offsets[0]+71U,2U);
        off::runtime::ApplicationServices display_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        initialize_app(display_app);
        off::runtime::SceneComponentSequence display_sequence{[]{return std::uint32_t{1};}};
        auto display_runtime=std::make_unique<off::graphics::IntroRuntime>(display_fixture.build(),display_app,
            display_sequence,"synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
        construct_directory(*display_runtime);
        auto& canonical=display_runtime->camera();
        check(!canonical.parameters().authored,"directory creates canonical fresh camera before source reader");
        canonical.set_enabled(false,false,{});
        canonical.begin_window_state_projection(true,false);
        const auto flags_before=canonical.flags();
        const auto context_before=display_runtime->camera_context();
        {
            off::graphics::FreshIntroCamera isolated;
            isolated.set_enabled(false,false,{});
            isolated.begin_window_state_projection(true,false);
            const auto prior=isolated.flags();
            isolated.apply_authored_parameters(display_runtime->resources().camera());
            check(isolated.flags()==(prior|0x10000U) && !isolated.enabled() && isolated.render_control()==5U,
                  "authored parameter application alone preserves runtime flags and render control");
        }
        auto display_session=off::graphics::make_normal_intro_scene_session(std::move(display_runtime));
        off::graphics::CameraRendererList renderers;
        const off::graphics::CameraDisplayDimensions dimensions{1600,900,false,false};
        unsigned callbacks=0;
        off::graphics::CameraRendererNode backend(renderers,dimensions,
            [&](auto& bound,float priority) {
                ++callbacks;
                check(&bound==&canonical && bound.parameters().authored &&
                      bound.parameters().authored->renderer_list_selector==1U && priority==4 &&
                      bound.parameters().viewport[1]==.122F && bound.render_control()==4U,
                      "reader applies authored state and display tuple to canonical camera before backend callback");
                check(!bound.enabled(),"camera reader preserves the disabled state");
                check(display_session->runtime().camera_context()==context_before,
                      "camera reader preserves canonical context");
                // Fixture directory order puts Window before Camera. Its two
                // true options clear 0x8000 and its completion ORs 0x210000;
                // the later camera source's option B additionally ORs 0x10000.
                check(display_session->runtime().window_camera_projection_applied() &&
                      bound.flags()==((flags_before&~0x8000U)|0x210000U|0x10000U),
                      "camera reader retains the preceding Window flag mutations and applies source option ORs");
                if(throw_callback) throw std::runtime_error("synthetic backend failure");
            },[](auto){});
        const off::graphics::CameraDisplayServices display_services{renderers,dimensions,[]{return false;}};
        if(throw_callback) {
            rejects([&]{display_session->complete_postconstruction_reader_bracket(0U,&display_services);});
            check(display_session->stage()==off::graphics::NormalIntroSceneSessionStage::failed &&
                  !display_session->runtime().first_cut_camera_reader_state(),
                  "throwing backend stops reader without camera admission receipt");
        } else {
            display_session->complete_postconstruction_reader_bracket(0U,&display_services);
            check(display_session->runtime().first_cut_camera_reader_state()!=nullptr,
                  "successful canonical backend callback precedes camera reader receipt");
        }
        check(callbacks==1U,"directory then reader invokes the actual selected backend exactly once");
    }

    Fixture handoff_fixture(false,true);
    off::runtime::ApplicationServices handoff_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    initialize_app(handoff_app);
    off::runtime::SceneComponentSequence handoff_sequence{[]{return std::uint32_t{1};}};
    auto handoff_runtime=std::make_unique<off::graphics::IntroRuntime>(handoff_fixture.build(),handoff_app,
        handoff_sequence,"synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
    construct_directory(*handoff_runtime);
    auto handoff_session=off::graphics::make_normal_intro_scene_session(std::move(handoff_runtime));
    handoff_session->complete_postconstruction_reader_bracket(0x52U);
    auto& handoff_host=handoff_session->runtime();
    check(handoff_host.camera().parameters().authored.has_value() &&
          handoff_host.camera().parameters().near_distance==2.5F &&
          handoff_host.camera().priority()==4,
          "CPU reader applies source parameters to canonical camera without backend services");
    const auto* keys_registry=handoff_host.scene_lifetime_keys_registry();
    check(keys_registry && keys_registry->size()==0U &&
          !keys_registry->resolve_required_keys(
              handoff_host.source_handle(handoff_host.resources().controller_index()).value,
              off::data::matpos_owner_child_selector),
          "complete reader bracket retains a valid empty KEYS registry when no MatPos owner exists");
    handoff_session->prepare_supported_first_cut_player();
    auto* live_player=handoff_session->first_cut_player();
    check(live_player && !live_player->initialization().phase_one_complete() &&
          !live_player->initialization().phase_two_complete() && !live_player->receiver().open(),
          "source-backed MovieControl handoff begins with the prepared first-cut session cold");
    auto handoff=handoff_session->make_first_cut_handoff();
    off::graphics::MovieControlFirstUpdate movie_control(
        handoff_host.component_handle(handoff_host.controller_component_index()),
        handoff_host.source_handle(handoff_host.resources().controller_index()).value,0);
    std::int32_t scene_clock=40;
    movie_control.run_phase_two({
        []{return false;},[]{},[](bool){},[]{},[&]{return scene_clock;},[] {}});
    std::vector<std::string> delivery_order;
    off::graphics::MovieControlEvent16Services event16{
        []{return true;},[]{return true;},[]{return false;},[]{return std::optional<std::uint64_t>{};},
        []{return true;},[&]{return scene_clock;},[&]{delivery_order.push_back("prepare");},
        [](std::uint64_t){},[&](std::uint64_t){delivery_order.push_back("group-state");}};
    const off::cutscene::FirstCutPlayerPhaseOneServices phase_one{
        [&](std::size_t,const auto&){delivery_order.push_back("player-phase-one");},
        []{},[]{},[]{return std::size_t{1};},[](std::uint64_t){},[]{}};
    const off::cutscene::FirstCutPlayerSessionPhaseTwoServices phase_two{
        [&](std::size_t,const auto&){delivery_order.push_back("player-phase-two");},
        [](std::string_view name)->std::optional<std::uint64_t> {
          return name=="rActiveCameraList"?std::optional<std::uint64_t>{71U}:
              name=="rCutSequenceObject"?std::optional<std::uint64_t>{72U}:std::nullopt;
        },[](std::size_t index)->std::optional<std::uint64_t> {
          return index==0U?std::optional<std::uint64_t>{73U}:std::nullopt;
        },[](std::uint64_t)->std::optional<off::cutscene::FirstCutMemberInfo> {
          return off::cutscene::FirstCutMemberInfo{4.0F};
        },[](std::uint64_t){return std::string_view{"SyntheticMember"};},
        [](std::uint64_t object)->std::optional<std::uint64_t>{return object;}};
    check(handoff.deliver(movie_control,event16,phase_one,phase_two)==
              off::graphics::MovieControlEvent16Result::waiting_for_deadline &&
          delivery_order.empty() && !live_player->initialization().phase_one_complete(),
          "source-backed MovieControl deadline wait leaves first-cut lifecycle untouched");
    ++scene_clock;
    check(handoff.deliver(movie_control,event16,phase_one,phase_two)==
              off::graphics::MovieControlEvent16Result::activated &&
          live_player->initialization().phase_one_complete() &&
          live_player->initialization().phase_two_complete() && live_player->receiver().closed() &&
          delivery_order.front()=="prepare" && delivery_order.back()=="group-state" &&
          std::ranges::find(delivery_order,"player-phase-one")!=delivery_order.end() &&
          std::ranges::find(delivery_order,"player-phase-two")!=delivery_order.end(),
          "live MovieControl activation delivers both first-cut lifecycle phases before group-state requests");
}

static OFF_NOINLINE void test_particle_emitter_runtime() {
    for(unsigned scenario=0;scenario<13U;++scenario) {
        Fixture fixture(false,true,false,true,false,false,false,false,false,3U,1U,3U);
        const auto original=fixture.image();
        std::uint32_t table{};for(unsigned i=0;i<4U;++i)table|=std::to_integer<std::uint32_t>(fixture.payload[20U+i])<<(8U*i);
        const auto record=fixture.synthetic_external_record_offsets[0];
        const auto entry=std::ranges::find(original.directory(),record,&off::data::GmsDirectoryEntry::record_offset);
        const auto counts=table+4U+entry->pool_group*96U;
        const auto& old=original.pool_groups()[entry->pool_group].class_counts;
        set(fixture.payload,counts+entry->pool_class*4U,old[entry->pool_class]-3U);
        set(fixture.payload,counts+4U,old[1]+3U);
        for(unsigned i=0;i<3U;++i) {
            const auto at=fixture.synthetic_external_record_offsets[i];
            set(fixture.payload,at+16U,i?0x002000e5U:0x00200002U);
            set(fixture.payload,at+20U,0U);set(fixture.payload,at+32U,0U);
        }
        // Both template rows retain the same exact constructed name.
        for(unsigned i=0;i<4U;++i) fixture.payload[fixture.synthetic_external_record_offsets[2]+i]=fixture.payload[fixture.synthetic_external_record_offsets[1]+i];
        const auto attachments=fixture.payload.size();word(fixture.payload,1U);
        word(fixture.payload,static_cast<std::uint32_t>(attachments+12U));word(fixture.payload,0U);text(fixture.payload,"ZGEOM_ParticleEmitter");
        set(fixture.payload,record+20U,static_cast<std::uint32_t>(attachments));
        const auto template_it=std::ranges::find(original.directory(),fixture.synthetic_external_record_offsets[2],&off::data::GmsDirectoryEntry::record_offset);
        const auto template_row=static_cast<std::size_t>(template_it-original.directory().begin());
        Bytes block(4U);
        scalar(block,scenario==8U?10U:8U,scenario==0U?static_cast<std::uint32_t>(template_row+1U)|0x80000000U:scenario==7U?0xffffffffU:scenario==8U?1U:0U);
        scalar(block,scenario==12U?8U:3U,1U);scalar(block,3U,0U);floating(block,2U,2.0F);floating(block,2U,3.0F);
        for(unsigned group=0;group<2U;++group) {
            floating(block,0x82U,1.0F);floating(block,0x42U,2.0F);floating(block,0xc2U,3.0F);
            if(group==0U){floating(block,2U,-90.0F);floating(block,2U,180.0F);}
        }
        floating(block,2U,4.0F);scalar(block,10U,scenario==9U?0x10001U:1U);
        scalar(block,3U,0U);scalar(block,3U,1U);floating(block,2U,5.0F);
        block.push_back(std::byte{4});text(block,scenario==4U?"syntheticExternalOwner1":"SyntheticExternalOwner1");
        block.push_back(std::byte{6});finish(block);
        if(scenario==6U){block.push_back(std::byte{0});set(block,0U,static_cast<std::uint32_t>(block.size()));}
        set(fixture.payload,record+32U,static_cast<std::uint32_t>(fixture.payload.size()));fixture.payload.insert(fixture.payload.end(),block.begin(),block.end());
        fixture.event_names({"EmitterEvent"});
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,{[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        app.initialize_native_group_registration();app.initialize_native_window_language_registration();app.initialize_native_picture_registration();app.initialize_native_camera_registration();
        app.initialize_native_second_window_scope_registration();app.initialize_native_visual_registration();app.initialize_native_room_animation_scope_registration();app.initialize_native_lens_flare_animation_scope_registration();app.initialize_native_remaining_intro_scope_registration();
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        auto runtime=std::make_unique<off::graphics::IntroRuntime>(fixture.build(),app,sequence,"synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
        static_cast<void>(runtime->declare_scene_event_name("EmitterEvent",0x345U));runtime->prepare_source_event_names();runtime->construct_root();runtime->begin_source_loading_without_engine_renderer();runtime->construct_source_directory_without_engine_renderer();
        const auto& directory=runtime->resources().sources().directory();
        const auto row=static_cast<std::size_t>(std::ranges::find(directory,record,&off::data::GmsDirectoryEntry::record_offset)-directory.begin());
        const auto component=runtime->owner_components(runtime->source_handle(row)).front();
        const auto property=runtime->scene_resource_property("ParticleTemplates");
        auto* registry=app.resolve_handle_collection(*property->object_token);
        const auto first=registry->members[0],second=registry->members[1];
        if(scenario==2U) registry->members.insert(registry->members.begin(),0xffffffffffffffffULL);
        if(scenario==3U) registry->members={second,first};
        if(scenario==5U || scenario==6U || scenario==10U || scenario==0U) runtime->set_scene_object_property_native("ParticleTemplates",app.create_handle_collection());
        if(scenario==0U || scenario==6U || scenario==10U) runtime->set_scene_owner_property_native("ParticleTemplates",runtime->source_handle(row),0U);
        if(scenario==11U) registry->members={0xffffffffffffffffULL};
        auto session=off::graphics::make_normal_intro_scene_session(std::move(runtime));auto& host=session->runtime();
        session->complete_postconstruction_reader_bracket(13U);
        const auto& state=host.constructed_attachment(component)->particle_emitter_reader;
        if(scenario==0U || scenario==6U) check(!host.scene_resource_property("ParticleTemplates")->object_token,"numeric success and malformed input do not invoke fallback registry mutation");
        if(scenario==10U) {
            const auto created=host.scene_resource_property("ParticleTemplates")->object_token;
            check(created && app.resolve_handle_collection(*created) && app.resolve_handle_collection(*created)->members.empty(),"valid fallback miss lazily publishes a persistent empty registry");
        }
        const bool valid=scenario!=6U && scenario!=7U && scenario!=8U && scenario!=9U && scenario!=12U;
        check(state.has_value()==valid,"ParticleEmitter publishes only fully valid runtime fields");
        if(valid) {
            const auto expected=scenario==0U||scenario==3U?second:scenario==4U||scenario==5U||scenario==10U||scenario==11U?0U:first;
            check(state->template_owner==(expected?std::optional<off::graphics::IntroRuntimeHandle>{{expected}}:std::nullopt),"ParticleEmitter uses numeric mapping and ordered exact registry lookup");
            check(state->template_resource==(expected?host.resource_handle({expected}):off::graphics::IntroRuntimeResourceHandle{}),"ParticleEmitter retains resource handle rather than owner token");
            check(state->event_id==0x345U && state->values.event_reference.bits==1U,"ParticleEmitter maps full event index before narrowing ID");
            check(host.constructed_attachment(component)->particle_emitter->local_reference==0U,"opaque constructor word remains untouched");
        }
        const auto work=std::ranges::find(host.deferred_reader_work(),row,&off::graphics::IntroDeferredReaderWork::source_directory_index);
        auto copy=*work;check(!host.supports_particle_emitter_deferred_reader(copy),"ParticleEmitter rejects foreign work identity");
        rejects([&]{host.apply_supported_particle_emitter_deferred_reader(copy);});
    }
}

static OFF_NOINLINE void test_param_anim_light_runtime() {
    for (unsigned scenario{}; scenario < 12U; ++scenario) {
        Fixture fixture(false,true,false,true,false,false,false,false,false,3U,1U,1U);
        const auto record = fixture.synthetic_external_record_offsets.front();
        // Repartition the original list owner's declared slot into the light
        // class in that same parsed parent pool; total allocation is unchanged.
        const auto original_image = fixture.image();
        const auto original = std::ranges::find(original_image.directory(),record,
            &off::data::GmsDirectoryEntry::record_offset);
        if (original == original_image.directory().end())
            throw std::runtime_error("synthetic light replacement has no original owner");
        std::uint32_t pool_table{};
        for (unsigned byte{}; byte<4U; ++byte)
            pool_table |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(fixture.payload[20U+byte])) << (8U*byte);
        const auto counts = pool_table + 4U + original->pool_group*24U*4U;
        const auto& old_counts = original_image.pool_groups().at(original->pool_group).class_counts;
        if (old_counts[original->pool_class] == 0U)
            throw std::runtime_error("synthetic light replacement has no declared slot");
        set(fixture.payload,counts+original->pool_class*4U,old_counts[original->pool_class]-1U);
        set(fixture.payload,counts+2U*4U,old_counts[2U]+1U);
        constexpr std::array<std::uint32_t,3> types{0x00800023U,0x00800020U,0x00800024U};
        set(fixture.payload,record+16U,types[scenario%3U]);
        const auto table = fixture.payload.size();
        const auto count = scenario == 5U ? 2U : 1U;
        word(fixture.payload,count); fixture.payload.resize(fixture.payload.size()+count*8U);
        for (unsigned slot{}; slot<count; ++slot) {
            set(fixture.payload,table+4U+slot*8U,static_cast<std::uint32_t>(fixture.payload.size()));
            set(fixture.payload,table+8U+slot*8U,0U);
            text(fixture.payload,slot || scenario==6U ? "ZGEOM_ParticleEmitter" : "ZGEOM_ParamAnim");
        }
        set(fixture.payload,record+20U,static_cast<std::uint32_t>(table));
        Bytes block(4U);
        scalar(block,3U,7U);scalar(block,8U,9U);scalar(block,11U,scenario==1U?0U:1U);
        block.push_back(std::byte{0x46});block.push_back(std::byte{0x86});
        scalar(block,3U,1U);floating(block,2U,12.5F);scalar(block,3U,0U);
        if (scenario >= 7U) {
            constexpr std::array<std::uint32_t,5> references{1U, 0U, 3U, 0x10001U, 0xffffffffU};
            scalar(block,0xcaU,references[scenario-7U]);
        }
        scalar(block,3U,55U); // valid unconsumed component tail
        block.push_back(std::byte{6});finish(block);
        if (scenario==3U) { block.push_back(std::byte{0});set(block,0U,static_cast<std::uint32_t>(block.size())); }
        if (scenario==4U) { block[block.size()-2U]=std::byte{0xff}; }
        set(fixture.payload,record+32U,static_cast<std::uint32_t>(fixture.payload.size()));
        fixture.payload.insert(fixture.payload.end(),block.begin(),block.end());
        fixture.event_names({"SyntheticParamEvent", "OtherSyntheticParamEvent"});
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        app.initialize_native_group_registration();app.initialize_native_window_language_registration();
        app.initialize_native_picture_registration();app.initialize_native_camera_registration();
        app.initialize_native_second_window_scope_registration();app.initialize_native_visual_registration();
        app.initialize_native_room_animation_scope_registration();app.initialize_native_lens_flare_animation_scope_registration();
        app.initialize_native_remaining_intro_scope_registration();
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        auto runtime=std::make_unique<off::graphics::IntroRuntime>(fixture.build(),app,sequence,
            "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
        check(runtime->declare_scene_event_name("SyntheticParamEvent",321U)==321U,
              "synthetic registry reserves an event ID distinct from the authored index");
        runtime->prepare_source_event_names();
        check(runtime->source_event_name_mapping()[1U]==321U,
              "production source event provider maps the authored name through the scene registry");
        runtime->construct_root();runtime->begin_source_loading_without_engine_renderer();
        runtime->construct_source_directory_without_engine_renderer();
        const auto& directory=runtime->resources().sources().directory();
        const auto found=std::ranges::find(directory,record,&off::data::GmsDirectoryEntry::record_offset);
        const auto row=static_cast<std::size_t>(std::distance(directory.begin(),found));
        const auto owner=runtime->source_handle(row);
        const auto component=runtime->owner_components(owner).front();
        check(!runtime->constructed_attachment(component)->param_animation ||
              runtime->constructed_attachment(component)->param_animation->event_id==0U,
              "fresh replacement ParamAnim explicitly initializes its event state to zero");
        const auto initial_flags=runtime->constructed_object_owner(row)->flags;
        const auto initial_admitted=runtime->components().at(component).state().admitted;
        auto session=off::graphics::make_normal_intro_scene_session(std::move(runtime));
        auto& host=session->runtime();
        if (scenario==3U || scenario==4U || scenario>=9U) rejects([&]{session->complete_postconstruction_reader_bracket(13U);});
        else session->complete_postconstruction_reader_bracket(13U);
        if (scenario<3U || scenario==7U || scenario==8U) {
            const auto& state=host.param_anim_deferred_reader_states().at(row);
            check(state.owner==owner && state.component_index==component && state.source_directory_index==row &&
                  state.resource==host.resource_handle(owner) && state.values.scalar_c==12.5F,
                  "normal scene dispatch binds ParamAnim typed fields to canonical light identity");
            check(host.constructed_attachment(component)->param_animation_reader.has_value() &&
                  host.constructed_object_owner(row)->flags==((initial_flags&~4U)|(scenario==1U?0U:4U)),
                  "light flag and ParamAnim typed state publish after complete dispatch");
            check(state.event_id==(scenario==7U?321U:0U) &&
                  host.constructed_attachment(component)->param_animation->event_id==state.event_id &&
                  (scenario>=7U ? state.values.event_reference==std::optional<std::uint32_t>{scenario==7U?1U:0U}
                                : !state.values.event_reference),
                  "production reader retains authored presence separately from registry-resolved event state");
        } else {
            check(host.param_anim_deferred_reader_states().empty() &&
                  !host.constructed_attachment(component)->param_animation_reader &&
                  host.constructed_object_owner(row)->flags==initial_flags &&
                  (!host.constructed_attachment(component)->param_animation ||
                   host.constructed_attachment(component)->param_animation->event_id==0U),
                  "late malformed dispatch and unsupported siblings leave light and component state untouched");
        }
        const auto work=std::ranges::find(host.deferred_reader_work(),row,
            &off::graphics::IntroDeferredReaderWork::source_directory_index);
        auto foreign=*work;
        check(!host.supports_param_anim_deferred_reader(foreign),"ParamAnim rejects copied noncanonical work");
        rejects([&]{host.apply_supported_param_anim_deferred_reader(foreign);});
        check(host.components().at(component).state().admitted==initial_admitted,
              "ParamAnim reader completion does not admit component lifecycle");
        const auto coverage=host.reader_coverage_inventory();
        bool covered{};
        for (const auto& entry:coverage.entries)
            if (entry.family==off::graphics::IntroDeferredReaderFamily::param_anim_component &&
                entry.state==off::graphics::IntroDeferredReaderImplementationState::applied) covered=true;
        check(covered==(scenario<3U || scenario==7U || scenario==8U),"ParamAnim coverage reports applied only for committed readers");
    }
}

static OFF_NOINLINE void test_basic_light_owner_runtime() {
    // Every family: set, clear, terminal tail, unexpected attachment delimiter.
    for (unsigned scenario{}; scenario < 12U; ++scenario) {
        Fixture fixture(false,true,false,true,false,false,false,false,false,3U,1U,1U);
        const auto record = fixture.synthetic_external_record_offsets.front();
        // Repartition the original list owner's declared slot into the light
        // class in that same parsed parent pool; total allocation is unchanged.
        const auto original_image = fixture.image();
        const auto original = std::ranges::find(original_image.directory(),record,
            &off::data::GmsDirectoryEntry::record_offset);
        if (original == original_image.directory().end())
            throw std::runtime_error("synthetic light replacement has no original owner");
        std::uint32_t pool_table{};
        for (unsigned byte{}; byte<4U; ++byte)
            pool_table |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(fixture.payload[20U+byte])) << (8U*byte);
        const auto counts = pool_table + 4U + original->pool_group*24U*4U;
        const auto& old_counts = original_image.pool_groups().at(original->pool_group).class_counts;
        if (old_counts[original->pool_class] == 0U)
            throw std::runtime_error("synthetic light replacement has no declared slot");
        set(fixture.payload,counts+original->pool_class*4U,old_counts[original->pool_class]-1U);
        set(fixture.payload,counts+2U*4U,old_counts[2U]+1U);
        constexpr std::array<std::uint32_t,3> types{0x00800023U,0x00800020U,0x00800024U};
        set(fixture.payload,record+16U,types[scenario%3U]);
        set(fixture.payload,record+20U,0U); // genuinely absent attachment list
        Bytes block(4U);
        scalar(block,3U,7U);scalar(block,8U,9U);scalar(block,11U,scenario<3U?1U:0U);
        block.push_back(std::byte{0x46});block.push_back(std::byte{0x86});
        if (scenario>=9U) block.push_back(std::byte{0xc6});
        finish(block);
        if (scenario>=6U && scenario<9U) {
            block.push_back(std::byte{0});set(block,0U,static_cast<std::uint32_t>(block.size()));
        }
        set(fixture.payload,record+32U,static_cast<std::uint32_t>(fixture.payload.size()));
        fixture.payload.insert(fixture.payload.end(),block.begin(),block.end());
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        app.initialize_native_group_registration();app.initialize_native_window_language_registration();
        app.initialize_native_picture_registration();app.initialize_native_camera_registration();
        app.initialize_native_second_window_scope_registration();app.initialize_native_visual_registration();
        app.initialize_native_room_animation_scope_registration();app.initialize_native_lens_flare_animation_scope_registration();
        app.initialize_native_remaining_intro_scope_registration();
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        auto runtime=std::make_unique<off::graphics::IntroRuntime>(fixture.build(),app,sequence,
            "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
        runtime->construct_root();runtime->begin_source_loading_without_engine_renderer();
        runtime->construct_source_directory_without_engine_renderer();
        const auto& directory=runtime->resources().sources().directory();
        const auto found=std::ranges::find(directory,record,&off::data::GmsDirectoryEntry::record_offset);
        const auto row=static_cast<std::size_t>(std::distance(directory.begin(),found));
        const auto owner=runtime->source_handle(row);
        // Seed live flags explicitly in this synthetic test: the public
        // inspection view is const, but its runtime-owned object is mutable.
        // Opposite option bits prove both set and clear while preserving peers.
        auto& seeded=const_cast<off::graphics::IntroConstructedObjectOwner&>(
            *runtime->constructed_object_owner(row));
        seeded.flags |= 0x80U;
        if (scenario>=3U) seeded.flags |= 4U;
        const auto initial_flags=seeded.flags;
        const auto component_count=runtime->components().size();
        const auto initial_lifecycle=runtime->preflight_global_lifecycle();
        auto session=off::graphics::make_normal_intro_scene_session(std::move(runtime));
        auto& host=session->runtime();
        if (scenario>=6U) rejects([&]{session->complete_postconstruction_reader_bracket(13U);});
        else session->complete_postconstruction_reader_bracket(13U);
        if (scenario<6U) {
            const auto& state=host.basic_light_owner_deferred_reader_states().at(row);
            check(state.owner==owner && state.source_directory_index==row &&
                  state.resource==host.resource_handle(owner) &&
                  state.source_offset==host.resources().sources().directory().at(row).deferred_source_offset &&
                  state.owner_flag_enabled==(scenario<3U),
                  "normal scene binds empty-attachment light reader to canonical owner identity");
            check(host.constructed_object_owner(row)->flags==((initial_flags&~4U)|(scenario<3U?4U:0U)),
                  "basic light reader sets or clears only its option bit");
        } else {
            check(host.basic_light_owner_deferred_reader_states().empty() &&
                  host.constructed_object_owner(row)->flags==initial_flags,
                  "invalid empty-attachment light continuation rolls back flag and receipt");
        }
        check(host.param_anim_deferred_reader_states().empty() && host.owner_components(owner).empty() &&
              host.components().size()==component_count && !host.constructed_picture_component(row),
              "empty light reader never synthesizes a component or ParamAnim state");
        const auto work=std::ranges::find(host.deferred_reader_work(),row,
            &off::graphics::IntroDeferredReaderWork::source_directory_index);
        auto foreign=*work;
        check(!host.supports_basic_light_owner_deferred_reader(foreign),
              "basic light reader rejects copied noncanonical work");
        rejects([&]{host.apply_supported_basic_light_owner_deferred_reader(foreign);});
        check(!host.supports_param_anim_deferred_reader(*work),
              "empty light owner cannot enter the ParamAnim component path");
        const auto lifecycle=host.preflight_global_lifecycle();
        check(lifecycle.covered_owners==initial_lifecycle.covered_owners &&
              lifecycle.covered_components==initial_lifecycle.covered_components && !lifecycle.ready(),
              "basic light reader receipt does not supply owner or component lifecycle admission");
        bool covered{};
        for (const auto& entry:host.reader_coverage_inventory().entries)
            if (entry.family==off::graphics::IntroDeferredReaderFamily::basic_light_owner &&
                entry.state==off::graphics::IntroDeferredReaderImplementationState::applied) covered=true;
        check(covered==(scenario<6U),"basic light coverage reports only committed owner readers");
    }
}

static OFF_NOINLINE void test_prepared_resource_variants() {
    const auto initialize_app=[](off::runtime::ApplicationServices& target) {
        target.initialize_native_group_registration();
        target.initialize_native_window_language_registration();
        target.initialize_native_picture_registration();
        target.initialize_native_camera_registration();
        target.initialize_native_second_window_scope_registration();
        target.initialize_native_visual_registration();
        target.initialize_native_room_animation_scope_registration();
        target.initialize_native_lens_flare_animation_scope_registration();
        target.initialize_native_remaining_intro_scope_registration();
    };
    const auto source_for_record=[](const auto& sources,std::size_t offset) {
        const auto found=std::ranges::find(sources.directory(),offset,&off::data::GmsDirectoryEntry::record_offset);
        if(found==sources.directory().end()) throw std::runtime_error("independent role record is absent");
        return static_cast<std::size_t>(std::distance(sources.directory().begin(),found));
    };
    Fixture base(false,true);
    auto prepared=base.build();
    const auto& sources=prepared.sources();
    const auto controller=source_for_record(sources,512U+48U*2U);
    const auto camera=source_for_record(sources,512U);
    const auto window=source_for_record(sources,512U+48U*8U);
    check(prepared.controller_index()==controller && prepared.camera_index()==camera &&
          prepared.window_index()==window && sources.directory()[controller].source_type==0x0800001aU &&
          sources.directory()[camera].source_type==0x00400003U &&
          sources.directory()[window].source_type==0x00100030U,
          "prepared roles resolve through fixture record identity and parsed controller references");
    const auto parsed_controller=sources.intro_movie_controller_source(controller);
    check(sources.local_source_for_authored_reference(parsed_controller.sequence_reference).has_value() &&
          sources.local_source_for_authored_reference(parsed_controller.group_reference).has_value(),
          "controller list references resolve against the emitted local directory");

    off::runtime::ApplicationServices missing_registration(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    off::runtime::SceneComponentSequence missing_sequence{[]{return std::uint32_t{1};}};
    off::graphics::IntroRuntime missing(base.build(),missing_registration,missing_sequence,
        "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
    rejects([&]{missing.construct_source_directory_without_engine_renderer();});
    missing.construct_root();
    check(!missing.loading_progress(),"root construction alone does not start source loading");
    missing.begin_source_loading_without_engine_renderer();
    check(missing.loading_progress().has_value(),"source loading begins only after the fresh root exists");
    rejects([&]{missing.construct_source_directory_without_engine_renderer();});
    check(missing.resource_load_stage()==off::graphics::IntroResourceLoadStage::failed &&
          missing.loaded_resource_handles().empty(),
          "missing row factory registration fails before any owner is published");

    Fixture with_sound(true,true);
    auto sound_prepared=with_sound.build();
    const auto sound_source=std::ranges::find(sound_prepared.sources().directory(),0x00200012U,
        &off::data::GmsDirectoryEntry::source_type);
    check(sound_source!=sound_prepared.sources().directory().end() && sound_prepared.sounds().size()==1U,
          "a second independently authored directory variant retains its optional sound owner");
    const auto sound_index=static_cast<std::size_t>(std::distance(sound_prepared.sources().directory().begin(),sound_source));
    off::runtime::ApplicationServices sound_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    initialize_app(sound_app);
    off::runtime::SceneComponentSequence sound_sequence{[]{return std::uint32_t{1};}};
    off::graphics::IntroRuntime sound_host(std::move(sound_prepared),sound_app,sound_sequence,
        "synthetic-scene",off::graphics::IntroSoundLoadPolicy::directory_construction);
    sound_host.construct_root();
    sound_host.begin_source_loading_without_engine_renderer();
    sound_host.construct_source_directory_without_engine_renderer();
    check(sound_host.loaded_resource_handles().size()==sound_host.resources().sources().directory().size() &&
          sound_host.resource_owner(sound_host.resource_handle(sound_host.source_handle(sound_index)))==
              sound_host.source_handle(sound_index) && sound_host.sounds().size()==1U,
          "optional sound rows use the same parsed-parent and canonical-owner construction path");
    rejects([&]{sound_host.construct_source_directory_without_engine_renderer();});
}

static OFF_NOINLINE void test_controller_reader_source_consistency() {
    {
        off::graphics::SceneReferenceRegistry registry;
        registry.append(std::uint64_t{0x100000001ULL});
        registry.append(std::nullopt);
        registry.append(std::uint64_t{0x200000002ULL});
        const auto lease=registry.seal();
        const auto first=registry.relocate(1U,lease);
        const auto last=registry.relocate(0x80000003U,lease);
        check(first!=1U && first!=0U && !(first&0x80000000U) &&
              last!=first && registry.resolve(first,lease)==0x100000001ULL &&
              registry.resolve(last,lease)==0x200000002ULL,
              "project reference tokens resolve independent 64-bit owners in row order");
        check(registry.relocate(0U,lease)==0U && registry.relocate(0x80000000U,lease)==0x80000000U &&
              registry.relocate(2U,lease)==0U && !registry.resolve(0U,lease),
              "project references distinguish mask-zero, null row and null token");
        rejects([&]{(void)registry.relocate(4U,lease);});
        rejects([&]{(void)registry.resolve(0x80000000U,lease);});
        rejects([&]{registry.append(std::uint64_t{3});});
        off::graphics::SceneReferenceRegistry foreign;
        foreign.append(std::uint64_t{0x100000001ULL});
        const auto other=foreign.seal();
        rejects([&]{(void)registry.resolve(first,other);});
        rejects([&]{(void)foreign.resolve(first,other);});
        registry.retire();
        rejects([&]{(void)registry.resolve(first,lease);});
        rejects([&]{(void)registry.relocate(1U,lease);});
        std::atomic<std::uint32_t> tiny{1U};
        check(off::graphics::detail::claim_scene_reference_token(tiny)==1U,
              "final available token can be claimed once");
        rejects([&]{(void)off::graphics::detail::claim_scene_reference_token(tiny);});
        off::graphics::SceneReferenceRegistry concurrent;
        concurrent.append(std::uint64_t{42});
        const auto concurrent_lease=concurrent.seal();
        const auto token=concurrent.relocate(1U,concurrent_lease);
        std::atomic<bool> started{false}, bad{false};
        // This checks allowed outcomes around retirement, but scheduling can
        // serialize the operations; it does not prove actual overlap.
        std::thread reader([&] {
            started.store(true,std::memory_order_release);
            for(unsigned i=0;i<10000U;++i) {
                try { if(concurrent.resolve(token,concurrent_lease)!=42U) bad.store(true); }
                catch(const std::runtime_error&) { break; }
            }
        });
        while(!started.load(std::memory_order_acquire)) std::this_thread::yield();
        concurrent.retire();
        reader.join();
        check(!bad.load(),"concurrent resolution cannot observe a different owner during retirement");
        rejects([&]{(void)concurrent.resolve(token,concurrent_lease);});
    }
    for (unsigned scenario=0; scenario<18; ++scenario) {
        Fixture fixture(false,true);
        if(scenario==14 || scenario==17)
            set(fixture.payload,fixture.block_offsets[6]+9U,scenario==14 ? 0x80000000U : 0U);
        if(scenario==15) {
            const auto image=fixture.image();
            std::optional<std::size_t> row;
            for(std::size_t i=0;i<image.directory().size();++i)
                for(std::size_t j=0;j<image.directory()[i].attachments.size();++j)
                    if(image.attachment_identifier(i,j)=="ZGEOM_MovieControl") row=i;
            if(!row) throw std::runtime_error("synthetic controller fixture is absent");
            const auto authored=image.intro_movie_controller_source(*row);
            set(fixture.payload,fixture.block_offsets[2]+16U,authored.sequence_reference);
        }
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
        app.initialize_native_group_registration();
        app.initialize_native_window_language_registration();
        app.initialize_native_picture_registration();
        app.initialize_native_camera_registration();
        app.initialize_native_second_window_scope_registration();
        app.initialize_native_visual_registration();
        app.initialize_native_room_animation_scope_registration();
        app.initialize_native_lens_flare_animation_scope_registration();
        app.initialize_native_remaining_intro_scope_registration();
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
        off::graphics::IntroRuntime host(fixture.build(),app,sequence,"synthetic-scene",
            off::graphics::IntroSoundLoadPolicy::directory_construction);
        host.construct_root();
        host.begin_source_loading_without_engine_renderer();
        host.construct_source_directory_without_engine_renderer();
        // Synthetic fault injection: the underlying prepared snapshot is not
        // const, although its public accessor is read-only.
        auto& prepared=const_cast<off::data::GmsIntroMovieControllerSource&>(host.resources().controller());
        switch(scenario) {
        case 1: prepared.sequence_reference ^= 1U; break;
        case 2: prepared.group_reference ^= 1U; break;
        case 3: prepared.additional_reference ^= 1U; break;
        case 4: prepared.destination.push_back('x'); break;
        case 5: prepared.authored_option ^= 1U; break;
        case 6: prepared.first_optional_reference = prepared.first_optional_reference ? std::nullopt : std::optional<std::uint32_t>{1U}; break;
        case 7: prepared.second_optional_reference = prepared.second_optional_reference ? std::nullopt : std::optional<std::uint32_t>{1U}; break;
        case 8: {
            const auto block=host.resources().sources().deferred_source_block(host.resources().controller_index());
            // Corrupt the tag in the non-const owned synthetic payload after preparation.
            const_cast<std::byte*>(block.data())[4U]=std::byte{0};
            break;
        }
        case 9: {
            const auto block=host.resources().sources().deferred_source_block(host.resources().controller_index());
            const auto parsed=host.resources().sources().intro_movie_controller_block(host.resources().controller_index(),block);
            const_cast<std::byte*>(block.data())[parsed.reference_offsets.front()] ^= std::byte{1};
            break;
        }
        case 10:
        case 11: {
            const auto block=host.resources().sources().deferred_source_block(host.resources().controller_index());
            const auto parsed=host.resources().sources().intro_movie_controller_block(host.resources().controller_index(),block);
            const auto invalid=scenario==10 ? 0x7fffffffU : 0x80000000U;
            prepared.second_optional_reference=invalid;
            auto* bytes=const_cast<std::byte*>(block.data());
            const auto offset=parsed.reference_offsets.back();
            for(unsigned i=0;i<4;++i) bytes[offset+i]=static_cast<std::byte>((invalid>>(8U*i))&0xffU);
            break;
        }
        case 13: {
            const auto row=host.resources().sources().local_source_for_authored_reference(prepared.sequence_reference);
            const auto block=host.resources().sources().deferred_source_block(*row);
            const auto parsed=host.resources().sources().intro_source_reference_list_block(*row,block);
            const_cast<std::byte*>(block.data())[parsed.reference_offsets.front()] ^= std::byte{1};
            break;
        }
        }
        bool injected_list_error=false;
        bool controller_was_published=false;
        const auto run=[&] {
            host.run_postconstruction_reader_bracket(73U,{
                [](auto){},[](const auto&){},[]{},[](const auto&){},
                [&](const auto& work) {
                    if(work.source_directory_index==host.resources().controller_index())
                        host.apply_supported_movie_control_deferred_reader(work);
                },[&](const auto& work) {
                    if((scenario==0 || scenario==12 || scenario==14 || scenario==15 || scenario==17) && work.source_directory_index==host.resources().controller_index()) {
                        host.apply_supported_movie_control_component_reader(work);
                        if(scenario==12) throw std::runtime_error("synthetic later controller callback failure");
                    }
                    if(scenario==16 && work.source_directory_index==host.resources().controller_index()) {
                        injected_list_error=true;
                        controller_was_published=host.movie_controller_reader_state()!=nullptr;
                        // Keep the synthetic prepared list and source block in
                        // agreement while injecting an out-of-range index after
                        // controller publication but before its list work.
                        auto raw=host.resources().cut_references();
                        const auto row=host.resources().sources().local_source_for_authored_reference(prepared.sequence_reference);
                        const auto block=host.resources().sources().deferred_source_block(*row);
                        const auto parsed=host.resources().sources().intro_source_reference_list_block(*row,block);
                        const_cast<std::uint32_t*>(raw.data())[0]=0x7fffffffU;
                        auto* bytes=const_cast<std::byte*>(block.data());
                        for(unsigned i=0;i<4;++i)
                            bytes[parsed.reference_offsets.front()+i]=static_cast<std::byte>((0x7fffffffU>>(8U*i))&0xffU);
                    }
                },[]{}});
        };
        if(scenario==0 || scenario==14 || scenario==15 || scenario==17) {
            run();
            const auto* state=host.movie_controller_reader_state();
            const auto sequence_row=host.resources().sources().local_source_for_authored_reference(prepared.sequence_reference);
            const auto group_row=host.resources().sources().local_source_for_authored_reference(prepared.group_reference);
            check(state && state->authored==prepared &&
                  sequence_row && group_row &&
                  state->sequence_list_resource==host.resource_handle(host.source_handle(*sequence_row)) &&
                  state->group_list_resource==host.resource_handle(host.source_handle(*group_row)) &&
                  host.reader_bracket_stage()==off::graphics::IntroReaderBracketStage::ordinary_reader_boundary_complete,
                  "controller reader resolves project-local tokens to the two live owners in the actual bracket");
            if(state && sequence_row && group_row) {
                const auto seq=std::ranges::find(host.deferred_reader_work(),*sequence_row,
                    &off::graphics::IntroDeferredReaderWork::source_directory_index);
                const auto group=std::ranges::find(host.deferred_reader_work(),*group_row,
                    &off::graphics::IntroDeferredReaderWork::source_directory_index);
                check(seq!=host.deferred_reader_work().end() && group!=host.deferred_reader_work().end() &&
                      seq->translated_references==state->sequence_members &&
                      group->translated_references==state->group_members,
                      "both prepared lists agree with independently retained controller members");
                if(scenario==0)
                    check(state->sequence_members.size()>=2U &&
                          state->sequence_members[0] &&
                          state->sequence_members[0]==state->sequence_members[1],
                          "duplicate authored list members retain the same resolved owner through the actual gate");
                if(scenario==14 || scenario==17)
                    check(!state->group_members.empty() && !state->group_members.front() &&
                          group!=host.deferred_reader_work().end() &&
                          !group->translated_references.empty() && !group->translated_references.front(),
                          "zero-index authored list word reaches the actual controller gate as a null member");
                if(scenario==15)
                    check(*sequence_row==*group_row && state->sequence_members==state->group_members,
                          "aliased controller lists consume the same validated list work");
                const auto bind_failure=[&] {
                    try { host.bind_movie_control_phase_two_services({}); }
                    catch(const std::runtime_error& error) { return std::string(error.what()); }
                    return std::string{};
                };
                check(bind_failure()=="intro phase two requires every named service",
                      "matched controller lists pass the reader gate before service validation");
                if(seq!=host.deferred_reader_work().end() && !seq->translated_references.empty()) {
                    // Simulate a corrupted project work result; the public
                    // view is read-only but the underlying test object is not.
                    auto& altered=const_cast<off::graphics::IntroDeferredReaderWork&>(*seq);
                    altered.translated_references.front()=std::nullopt;
                    check(bind_failure()=="MovieControl phase two requires its complete live readers and unbound scene services",
                          "controller gate rejects a changed prepared list after the bracket");
                }
            }
        } else {
            if(scenario==16) {
                std::string error;
                try { run(); }
                catch(const std::out_of_range& failure) { error=failure.what(); }
                catch(const std::exception& failure) { error=failure.what(); }
                check(injected_list_error && controller_was_published &&
                      error=="intro list reference index is out of range",
                      "list relocation rejects the injected out-of-range index after controller publication");
            } else rejects(run);
            check(host.movie_controller_reader_state()==nullptr &&
                  host.movie_controller_component_reader_state()==nullptr &&
                  host.reader_bracket_stage()==off::graphics::IntroReaderBracketStage::failed,
                  "controller reader rejects each altered prepared field without publication");
            if(scenario==12)
                check(host.reader_bracket_retained_saved_value()==73U,
                      "later controller failure does not imply a complete bracket rollback");
        }
    }
}

static OFF_NOINLINE void test_sound_source_transforms() {
    using namespace off::graphics;
    const auto initialize=[](off::runtime::ApplicationServices& app) {
        app.initialize_native_group_registration();app.initialize_native_window_language_registration();
        app.initialize_native_picture_registration();app.initialize_native_camera_registration();
        app.initialize_native_second_window_scope_registration();app.initialize_native_visual_registration();
        app.initialize_native_room_animation_scope_registration();app.initialize_native_lens_flare_animation_scope_registration();
        app.initialize_native_remaining_intro_scope_registration();
    };
    const auto construct=[](IntroRuntime& host) {
        host.construct_root();host.begin_source_loading_without_engine_renderer();
        host.construct_source_directory_without_engine_renderer();
    };
    Fixture fixture(true,true);
    constexpr std::array<float,9> basis{2,3,4,5,6,7,8,9,10};
    constexpr std::array<float,3> position{11,12,13};
    set(fixture.payload,512U+48U*9U+4U,static_cast<std::uint32_t>(fixture.payload.size()));
    for(float value:basis) word(fixture.payload,std::bit_cast<std::uint32_t>(value));
    set(fixture.payload,512U+48U*9U+8U,static_cast<std::uint32_t>(fixture.payload.size()));
    for(float value:position) word(fixture.payload,std::bit_cast<std::uint32_t>(value));
    set(fixture.payload,fixture.block_offsets[9]+40U,0U);
    off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{100};}});
    initialize(app);app.reset_clock();app.clock().assign_crt_mode(true);
    off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{1};}};
    IntroRuntime host(fixture.build(),app,sequence,"synthetic-scene",IntroSoundLoadPolicy::directory_construction);
    construct(host);
    const auto& directory=host.resources().sources().directory();
    const auto row=static_cast<std::size_t>(std::distance(directory.begin(),
        std::ranges::find(directory,0x00200012U,&off::data::GmsDirectoryEntry::source_type)));
    const auto owner=host.source_handle(row);
    const auto resource=host.resource_handle(owner);
    const auto initialized=host.source_transform(owner);
    check(initialized.owner==owner && initialized.resource==resource && initialized.basis==basis && initialized.position==position,
          "normal directory allocation initializes canonical nonidentity sound transform");
    host.run_postconstruction_reader_bracket(1U,{
        [](auto){},[](const auto&){},[]{},[](const auto&){},
        [&](const auto& work){if(work.source_directory_index==row)host.apply_supported_sound_owner_deferred_reader(work);},
        [](const auto&){},[]{}});
    auto& sound=host.sound_for_source(row);
    auto& record=sound.record();
    const auto parent=host.resource_owner(host.resource_parent(owner));
    host.set_local_transform(parent,{11,0,0,0,13,0,0,0,17},{200,300,400});
    const auto parent_pose=host.hierarchy().at(host.hierarchy_index(parent));
    auto& cache=host.picture_for_source(host.resources().pictures().front().directory_index).submission_cache();
    const auto clean_cache=[&] {
        PictureCacheTransformInput input{.virtual_window_scale={0,0,1,1},
            .cached_basis={0,0,1,0,1,0,1,0,0},.object_matrix={0,0,1,0,1,0,1,0,0},
            .viewport_width=100,.viewport_height=100,.picture_width=10,.picture_height=10,
            .owner_projection_scalar=1,.external_y_basis_scale=1};
        cache.submit(std::span<const off::data::BoundPictureDrawGroup>{},input,0,[](auto,const auto&,const auto&,auto){});
        check(!cache.dirty(),"source transform test prepares a real clean picture cache");
    };
    auto state=*host.resource_state(owner);state.flags=0x09000080U;host.assign_resource_state(owner,state);
    clean_cache();
    host.update_source_transform(owner,basis,position);
    auto last_only=basis;last_only[6]=99.F;
    host.update_source_transform(owner,last_only,position);
    check(host.source_transform(owner).basis==basis && host.resource_state(owner)->flags==state.flags && !cache.dirty(),
          "bitwise equal compared fields ignore isolated final-basis changes without dirtying resources or cache");
    auto changed=basis;changed[0]=-4.F;changed[1]=8.F;changed[2]=16.F;changed[8]=27.F;
    const std::array<float,3> changed_position{0,24,25};
    host.update_source_transform(owner,changed,changed_position);
    check(host.source_transform(owner).basis==changed && host.source_transform(owner).position==changed_position &&
          host.resource_state(owner)->flags==(state.flags|0x00100000U) && cache.dirty() &&
          host.position_updates().pending_count()==0U && !host.position_updates().failed() &&
          !host.directory_position_mode().collection_enabled,
          "checked update copies complete raw pose, preserves flags and uses disabled collection without backend work");
    check(host.hierarchy().at(host.hierarchy_index(parent)).matrix==parent_pose.matrix &&
          host.hierarchy().at(host.hierarchy_index(parent)).position==parent_pose.position &&
          host.resource_parent(owner)==host.resource_handle(parent),
          "source updates preserve transformed parent and parent identity");
    state=*host.resource_state(owner);state.flags&=~0x00100000U;host.assign_resource_state(owner,state);
    auto signed_zero=changed_position;signed_zero[0]=-0.F;
    host.update_source_transform(owner,changed,signed_zero);
    check(std::signbit(host.source_transform(owner).position[0]) &&
          (host.resource_state(owner)->flags&0x00100000U),"signed zero is a changed source position bit pattern");
    const auto before_bad=host.source_transform(owner);const auto before_flags=host.resource_state(owner)->flags;
    auto bad_basis=changed;bad_basis[8]=std::numeric_limits<float>::infinity();
    rejects([&]{host.update_source_transform(owner,bad_basis,signed_zero);});
    auto bad_position=signed_zero;bad_position[1]=std::numeric_limits<float>::quiet_NaN();
    rejects([&]{host.update_source_transform(owner,changed,bad_position);});
    check(host.source_transform(owner).basis==before_bad.basis && host.resource_state(owner)->flags==before_flags,
          "nonfinite inputs including ignored basis fields reject before mutation");
    IntroRuntime foreign(fixture.build(),app,sequence);
    rejects([&]{(void)host.source_transform(foreign.source_handle(row));});
    rejects([&]{host.update_source_transform(foreign.source_handle(row),basis,position);});
    const auto stable=host.source_transform(owner);
    const auto size_before=host.hierarchy().size();
    check(host.create_default_camera_resource(false,[](auto){}).has_value() && host.hierarchy().size()==size_before+1U &&
          host.source_transform(owner).basis==stable.basis && host.source_transform(owner).resource==stable.resource,
          "source snapshot identity survives synthesized camera insertion and vector growth");
    bool enable_requested=false;unsigned enabled{};
    IntroSoundPreparationServices services{
        [&](auto h){check(h==owner,"sound flags uses canonical source owner");return host.resource_state(h)->flags;},
        [&](auto h){return host.resource_owner(host.resource_parent(h));},
        [&]{return enable_requested;},[&](auto h){check(h==owner,"enable receives sound owner");++enabled;}};
    state=*host.resource_state(owner);state.flags|=0x400U;host.assign_resource_state(owner,state);
    record.position={71,72,73};record.direction={81,82,83};
    host.prepare_sound_owner(row,services);
    check(record.position==std::array<float,3>{71,72,73} && record.direction==std::array<float,3>{81,82,83} &&
          !sound.active() && enabled==0U,"disabled resource skips spatial snapshot and owner enable");
    state.flags&=~0x400U;host.assign_resource_state(owner,state);
    record.active_source=0U;record.flags=0U;
    host.prepare_sound_owner(row,services);
    check(record.position==std::array<float,3>{71,72,73} && !sound.active() && enabled==0U,
          "backend failure does not consume source transform despite a valid canonical provider");
    record.active_source=128U;
    host.prepare_sound_owner(row,services);
    check(sound.active() && enabled==0U && record.position==signed_zero &&
          record.direction==std::array<float,3>{-4,8,16} && record.parent==parent.value,
          "successful preparation with owner enable false copies raw unnormalized direction and position without parent composition");
    const auto binding=sound.owner_binding();
    const auto first_coverage=host.preflight_global_lifecycle().covered_owners;
    rejects([&]{host.prepare_sound_owner(row,services);});
    check(sound.active() && !sound.failed() && sound.owner_binding()==binding &&
          std::ranges::count(app.sound_records().prepared(),binding)==1 &&
          host.preflight_global_lifecycle().covered_owners==first_coverage,
          "preparing an active owner rejects before changing playback or coverage");
    host.stop_sound_owner(row);
    check(!sound.active() && host.preflight_global_lifecycle().covered_owners+1U==first_coverage,
          "stopping the owner removes its live coverage without destroying its binding");
    check(std::ranges::count(app.sound_records().prepared(),binding)==0,
          "a single stop removes the only prepared backend occurrence");
    record.active_source=0U;record.flags=0U;
    host.prepare_sound_owner(row,services);
    check(!sound.active() && !sound.failed() && sound.owner_binding()==binding &&
          std::ranges::count(app.sound_records().prepared(),binding)==0 &&
          host.preflight_global_lifecycle().covered_owners+1U==first_coverage,
          "backend preparation failure after stop cannot revive owner coverage");
    record.active_source=128U;
    host.prepare_sound_owner(row,services);
    check(sound.active() && !sound.failed() && sound.owner_binding()==binding &&
          host.preflight_global_lifecycle().covered_owners==first_coverage &&
          std::ranges::count(app.sound_records().prepared(),binding)==1,
          "the same live sound owner can prepare again after stop without duplicating coverage");
    host.update_source_transform(owner,basis,{101,102,103});
    check(record.position==signed_zero && record.direction==std::array<float,3>{-4,8,16} &&
          initialized.basis==basis && initialized.position==position,
          "audio and caller snapshots remain values after later source updates");
    check(!host.preflight_global_lifecycle().ready(),"individual sound preparation does not admit global lifecycle");
    // Missing association is produced by actual root construction, not a fake
    // lookup service. A later load failure must not restore parsed-row access.
    foreign.construct_root();
    rejects([&]{(void)foreign.source_transform(foreign.source_handle(row));});
    rejects([&]{foreign.update_source_transform(foreign.source_handle(row),basis,position);});
    auto& unassociated=foreign.sound_for_source(row);
    auto& unassociated_record=unassociated.record();
    unassociated_record.position={31,32,33};unassociated_record.direction={41,42,43};
    rejects([&]{foreign.prepare_sound_owner(row,{
        [](auto){return 0U;},[&](auto){return foreign.root_handle();},
        []{return false;},[](auto){throw std::runtime_error("unassociated sound must not enable");}});});
    check(unassociated.failed() && !unassociated.active() &&
          unassociated_record.position==std::array<float,3>{31,32,33} &&
          unassociated_record.direction==std::array<float,3>{41,42,43},
          "sound preparation identity failure preserves spatial fields and fails its owner without borrowing another source");
    off::runtime::ApplicationServices missing_app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
    off::runtime::SceneComponentSequence missing_sequence{[]{return std::uint32_t{1};}};
    IntroRuntime failed(fixture.build(),missing_app,missing_sequence,"synthetic-scene",IntroSoundLoadPolicy::directory_construction);
    failed.construct_root();failed.begin_source_loading_without_engine_renderer();
    rejects([&]{failed.construct_source_directory_without_engine_renderer();});
    rejects([&]{(void)failed.source_transform(failed.source_handle(row));});
    rejects([&]{failed.update_source_transform(failed.source_handle(row),basis,position);});
    Fixture plain(false,true);
    IntroRuntime immediate(plain.build(),app,sequence,"synthetic.wld",IntroSoundLoadPolicy::directory_construction);
    construct(immediate);
    const auto immediate_owner=immediate.source_handle(immediate.resources().camera_index());
    const auto prior=immediate.source_transform(immediate_owner);
    const auto prior_flags=immediate.resource_state(immediate_owner)->flags;
    rejects([&]{immediate.update_source_transform(immediate_owner,basis,position);});
    check(immediate.source_transform(immediate_owner).basis==prior.basis &&
          immediate.source_transform(immediate_owner).position==prior.position &&
          immediate.resource_state(immediate_owner)->flags==prior_flags,
          "unsupported retained immediate mode rejects source update before state changes");
}

static OFF_NOINLINE void test_runtime_and_archive_lifecycle() {
    Fixture fixture;
    std::int32_t clock_sample=1000;
    off::runtime::SceneComponentSequence component_sequence{[]{return std::uint32_t{0};}};
    off::runtime::ApplicationServices application(
        off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[] { return std::int64_t{99}; },[&] { return clock_sample; }},
        []() -> off::audio::SoundVolumeBackend* { return nullptr; });
    {
      off::runtime::ApplicationServices sound_application(
          off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          {[] { return std::int64_t{1}; },[&] { return clock_sample; }});
      sound_application.reset_clock();
      sound_application.clock().assign_crt_mode(true);
      Fixture input(true);
      const auto block=input.block_offsets[9];
      set(input.payload,block+40,0); // Independent non-looping source.
      set(input.payload,block+55,0); // Same category targeted by sound preferences.
      set(input.payload,block+60,1);
      std::uint64_t stale{};
      {
        off::graphics::IntroRuntime sound_host(input.build(),sound_application,component_sequence);
        auto& owner=sound_host.sound_for_source(9);
        auto& record=owner.record();
        stale=record.binding;
        sound_host.apply_sound_extension(9);
        check(record.gain_multiplier==66,"unpublished binding skips extension even with unsupported source values");
        check(sound_host.sounds().size()==1 && sound_application.sound_records().size()==1 &&
              sound_application.sound_records().resolve(stale)==&record &&
              record.active_source==128 && record.alternate_source==0 &&
              owner.owner_binding()==0 && !owner.active() && record.duration==0 && record.progress==0,
              "source-backed host owns one canonical record without invented preinit/playback");
        check(record.range==50 && record.gain_multiplier==66 &&
              record.derived_range==static_cast<float>((66.0F/0.78125F)*50.0F),
              "authored range input is not a pitch setter");
        sound_host.set_local_transform(owner.handle(),{1,0,0,0,1,0,0,0,1},{3,4,5});
        std::vector<std::string> calls;
        std::uint32_t owner_status=0x80, flags=0x400;
        off::graphics::IntroSoundPreparationServices services{
          [&](auto handle) {
            check(handle==owner.handle(),"live sound flags owner"); calls.push_back("flags");
            rejects([&] { sound_host.stop_sound_owner(9); });
            rejects([&] { sound_host.apply_sound_extension(9); });
            return flags;
          },
          [&](auto) { calls.push_back("parent"); return sound_host.root_handle(); },
          [&] { calls.push_back("gate"); return true; },
          [&](auto) { calls.push_back("enable"); owner_status|=1; }
        };
        sound_host.prepare_sound_owner(9,services);
        check(calls==std::vector<std::string>{"parent","flags"} && owner.owner_binding()==stale &&
              record.parent==sound_host.root_handle().value && record.alternate_source==128 &&
              !owner.active() && record.duration==0 && sound_application.sound_records().prepared().empty(),
              "hidden owner retains ordered prehook prefix but skips backend preparation");
        rejects([&] { sound_host.apply_sound_extension(9); });
        check(record.gain_multiplier==66 && record.category==0 && record.progress==0,
              "unsupported extension branches reject before canonical writes");
        flags=0; calls.clear(); clock_sample=1250;
        sound_application.advance_crt();
        sound_host.prepare_sound_owner(9,services);
        check(calls==std::vector<std::string>{"parent","flags","gate","enable"} &&
              owner.active() && owner_status==0x81 && record.start_time==256 &&
              record.duration==12.375F && record.progress==0 && record.playback_state==7 &&
              record.direction==std::array<float,3>{1,0,0},
              "preparation preserves SND float and uses canonical raw clock/live resources without readiness");
        sound_application.sound().set_volume(91);
        check(record.playback_state==5 && sound_application.sound_records().categories()[0].selected &&
              sound_application.sound_records().pending_volume_update() && record.progress==0,
              "default application preferences mutate the SAME sound record backend");
        const auto& const_owner=owner;
        check(&const_owner.record()==&record,"const consumers share canonical record, not metadata copies");
        {
          auto supported=input;
          const auto extend=supported.block_offsets[9]+70;
          for(std::size_t field=0;field<15;++field) set(supported.payload,extend+field*5+1,0);
          set(supported.payload,extend+1,std::bit_cast<std::uint32_t>(-1.0F));
          set(supported.payload,extend+11*5+1,2);
          set(supported.payload,extend+12*5+1,1); set(supported.payload,extend+13*5+1,1);
          off::graphics::IntroRuntime second(supported.build(),sound_application,component_sequence);
          check(second.sound_for_source(9).record().binding!=stale && sound_application.sound_records().size()==2,
                "simultaneous scenes have distinct sound bindings despite equal local owner handles");
          second.prepare_sound_owner(9,{
            [](auto) { return 0x400U; },[&](auto) { return second.root_handle(); },
            [] { return false; },[](auto) { throw std::runtime_error("unexpected owner enable"); }
          });
          auto& other=second.sound_for_source(9).record();
          volatile float exponent=-1.0F/-20.0F;
          const float expected=static_cast<float>(100.0/std::pow(10.0,static_cast<double>(exponent)));
          for(const auto bits:{0U,2U,4U,6U}) {
            other.flags=0x100U|bits; other.output_mode=99;
            second.apply_sound_extension(9);
            check(other.gain_multiplier==expected && other.flags==(0x180U|bits) &&
                  other.category==2 && other.output_mode==2 && other.progress==0 && other.duration==0 &&
                  !second.sound_for_source(9).active() && record.gain_multiplier==66 && record.category==0,
                  "source-backed extension preserves current option bits and changes only its canonical record");
          }
        }
        check(sound_application.sound_records().size()==1,"second scene releases only its own sound lease");
        sound_host.stop_sound_owner(9);
        check(!owner.active() && record.playback_state==3 && record.active_source==128 &&
              sound_application.sound_records().pending_stops().size()==1 &&
              sound_application.sound_records().resolve(stale)==&record,
              "binding stop changes canonical membership/state but does not dispose the owner");
        auto broken=services;
        broken.enable_owner=[](auto) { throw std::runtime_error("live owner enable unavailable"); };
        rejects([&] { sound_host.prepare_sound_owner(9,broken); });
        check(owner.failed(),"failed callback stops further owner use without fake disposal");
        rejects([&] { (void)owner.record(); });
      }
      check(sound_application.sound_records().size()==0 && !sound_application.sound_records().resolve(stale) &&
            sound_application.sound_records().prepared().empty() && sound_application.sound_records().pending_stops().empty(),
            "whole-scene teardown invalidates bindings and removes pending memberships");
      sound_application.sound_records().acknowledge_started(stale,512);
      {
        off::graphics::IntroRuntime missing(input.build(),sound_application,component_sequence);
        auto& owner=missing.sound_for_source(9);
        auto& record=owner.record();
        record.active_source=0;
        off::graphics::IntroSoundPreparationServices services{
          [](auto) { return 0U; }, [&](auto) { return missing.root_handle(); },
          [] { return false; }, [](auto) { throw std::runtime_error("must not enable failed owner"); }
        };
        auto incomplete=services; incomplete.parent_owner={};
        rejects([&] { missing.prepare_sound_owner(9,incomplete); });
        check(!owner.failed() && owner.owner_binding()==0,"missing service rejects before prehook mutations");
        rejects([&] { missing.prepare_sound_owner(9,services); });
        check(owner.failed() && record.flags==2 && !owner.active(),
              "failed prepare preserves destructive-branch flag mutation and halts owner execution");
      }
      check(sound_application.sound_records().size()==0,"failed owner lease is released by scene teardown");
      clock_sample=1000;
    }
    {
      off::graphics::IntroRuntime host(fixture.build(),application,component_sequence);
      application.sound_records().clear_scene_listener();
      check(!host.sound_listener(),"unregistered scene camera cannot manufacture a sound listener");
      std::vector<int> camera_calls;
      const auto camera_owner=host.source_handle(host.resources().camera_index());
      host.register_camera(0,{
        [&]{camera_calls.push_back(1);return 1920;},
        [&]{camera_calls.push_back(2);return 1080;},
        [&]{camera_calls.push_back(3);return false;},
        [&](auto){throw std::runtime_error("unready backend must not admit a view");}
      });
      check(camera_calls==std::vector<int>{1,2,3} && host.camera().renderer_width()==1920 &&
        host.camera().renderer_height()==1080,"registration uses canonical camera dimensions before backend gate");
      host.register_camera(9,{});
      check(camera_calls.size()==3,"duplicate registration skips dimensions and backend");
      const auto listener=host.sound_listener();
      check(listener && listener->owner==camera_owner && listener->context==host.root_handle() &&
        application.sound_records().listener_handle()==0,"registry fallback selects actual camera/root without setting explicit listener");
      host.camera().set_enabled(false,false,{});
      check(host.sound_listener()->owner==camera_owner,"disabled camera remains a valid sound listener");
      host.camera().set_enabled(true,false,{});
      host.set_camera_context(host.source_handle(0));
      check(host.sound_listener()->context==host.source_handle(0),"live camera context replaces root independently of resource parent");
      host.set_camera_context({});
      check(host.sound_listener()->context==host.root_handle(),"null camera context falls back to actual root");
      host.set_sound_listener(camera_owner);
      check(application.sound_records().listener_handle()==camera_owner.value &&
        *application.sound_records().listener_offsets()==std::array<float,3>{0,0,0},"explicit listener setter reaches application backend");
      application.sound_records().clear_scene_listener();
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
}

static OFF_NOINLINE void test_runtime_scene_identity_and_later() {
    Fixture fixture;
    std::int32_t clock_sample=1000;
    off::runtime::SceneComponentSequence component_sequence{[]{return std::uint32_t{0};}};
    off::runtime::ApplicationServices application(
        off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[] { return std::int64_t{99}; },[&] { return clock_sample; }},
        []() -> off::audio::SoundVolumeBackend* { return nullptr; });
    {
      off::graphics::IntroRuntimeHandle expired_camera;
      {
        off::graphics::IntroRuntime old_scene(fixture.build(),application,component_sequence);
        expired_camera=old_scene.source_handle(old_scene.resources().camera_index());
        old_scene.set_sound_listener(expired_camera);
        check(old_scene.sound_listener()->owner==expired_camera,"old scene explicit listener resolves while owner lives");
      }
      off::graphics::IntroRuntime next_scene(fixture.build(),application,component_sequence);
      const auto next_camera=next_scene.source_handle(next_scene.resources().camera_index());
      check(next_camera!=expired_camera && !next_scene.sound_listener() &&
            application.sound_records().listener_handle()==expired_camera.value,
            "expired explicit camera cannot alias same source in a later scene");
      rejects([&]{(void)next_scene.source_index(expired_camera);});
      rejects([&]{(void)next_scene.hierarchy_index(expired_camera);});
      rejects([&]{next_scene.set_sound_listener(expired_camera);});
      next_scene.register_camera(0,{[]{return 640;},[]{return 480;},[]{return false;},{}});
      check(next_scene.sound_listener()->owner==next_camera &&
            next_scene.sound_listener()->context==next_scene.root_handle() &&
            application.sound_records().listener_handle()==expired_camera.value,
            "new registry fallback selects new live camera without reviving or rewriting expired explicit handle");
      off::graphics::IntroRuntime simultaneous(fixture.build(),application,component_sequence);
      check(next_scene.root_handle()!=simultaneous.root_handle(),"simultaneous scenes have distinct roots");
      for(std::size_t i=0;i<next_scene.resources().sources().directory().size();++i) {
        const auto first=next_scene.source_handle(i),second=simultaneous.source_handle(i);
        check(first!=second && next_scene.source_index(first)==i && simultaneous.source_index(second)==i &&
              next_scene.hierarchy_index(first)==i+1 && simultaneous.hierarchy_index(second)==i+1,
              "application owner ranges preserve source and hierarchy joins without aliasing");
        rejects([&]{(void)next_scene.source_index(second);});
        rejects([&]{(void)simultaneous.hierarchy_index(first);});
      }
      rejects([&]{(void)next_scene.hierarchy_index(simultaneous.root_handle());});
      rejects([&]{simultaneous.set_camera_context(next_scene.root_handle());});
      check(simultaneous.camera_context()==simultaneous.root_handle(),"foreign context rejection preserves scene root");
      application.sound_records().clear_scene_listener();
    }
    {
      using namespace off::graphics;
      std::int32_t camera_sample=0;
      off::runtime::ApplicationServices camera_application(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          {[]{return std::int64_t{0};},[&]{return camera_sample;}});
      IntroRuntime first(fixture.build(),camera_application,component_sequence);
      IntroRuntime interleaved(fixture.build(),camera_application,component_sequence);
      // Explicit synthetic live words/modes, not proof of the source loader.
      {
        IntroRuntime first(fixture.build(),camera_application,component_sequence);
        const auto root=first.root_handle(), child=first.source_handle(0);
        const auto rr=first.resource_handle(root), cr=first.resource_handle(child);
        bool allocating=false,suppressed=false;
        IntroRuntime::ResourceMutationModes modes{allocating,suppressed};
        first.assign_resource_state(child,{0x09000000U,{}});
        rejects([&]{first.mutate_resource_low_byte(cr,0x80U,0);});
        check(first.resource_state(child)->flags==0x09000000U,"unknown ancestor rejects before child mutation");
        first.assign_resource_state(child,{0x090000ffU,{}});
        first.set_resource_flags_no_maintenance(cr,0,0xffU,modes);
        check(first.resource_state(child)->flags==0x09000000U && !first.resource_state(root),
              "clear-only setter does not require or manufacture unknown ancestor state");
        first.assign_resource_state(root,{0x09000000U,{}});
        first.set_resource_flags_no_maintenance(cr,0x09108080U,~0x09108080U,modes);
        check(first.resource_state(child)->flags==0x09108080U && first.resource_state(root)->flags==0x09000080U,
              "replacement propagates only requested low byte through canonical parent");
        first.mutate_resource_low_byte(rr,0xf0U,0xffU);
        first.set_resource_flags_no_maintenance(cr,3U,0xffU,modes);
        check(first.resource_state(root)->flags==0x090000f3U && first.resource_state(child)->flags==0x09108003U,
              "set wins overlap and root accumulates requested child low bits");
        first.mutate_resource_low_byte(cr,0,0xffU);
        first.mutate_resource_low_byte(rr,0,0x80U);
        check(first.resource_state(root)->flags==0x09000073U && first.resource_state(child)->flags==0x09108000U,
              "child clears do not propagate and root clears do not alter child");
        allocating=true;
        rejects([&]{first.set_resource_flags_no_maintenance(cr,0x8001U,0,modes);});
        rejects([&]{first.set_resource_flags_no_maintenance(cr,0x2001U,0,modes);});
        check(first.resource_state(child)->flags==0x09108000U && first.resource_state(root)->flags==0x09000073U,
              "unsupported service branches reject before any low-byte writes");
        first.set_resource_flags_no_maintenance(cr,0x10000000U,0,modes);
        check(first.resource_state(child)->flags==0x19108000U,
              "active allocation allows masks that do not touch maintenance bit");
        first.assign_resource_state(child,{0x0910a000U,{}});
        rejects([&]{first.set_resource_flags_no_maintenance(cr,2U,0x2000U,modes);});
        check(first.resource_state(child)->flags==0x0910a000U && first.resource_state(root)->flags==0x09000073U,
              "clearing live registration bit rejects before requested low-byte mutation");
        first.set_resource_flags_no_maintenance(cr,0x2000U,0x2000U,modes);
        check(first.resource_state(child)->flags==0x0910a000U,
              "unchanged live registration bit is admitted with overlapping set and clear");
        first.assign_resource_state(child,{0x09108000U,{}});
        suppressed=true;
        first.set_resource_flags_no_maintenance(cr,0x8001U,0,modes);
        rejects([&]{first.mutate_resource_low_byte({},1,0);});
        rejects([&]{first.mutate_resource_low_byte(interleaved.resource_handle(interleaved.root_handle()),1,0);});
        auto& picture=first.picture_for_source(1);
        const auto picture_resource=first.resource_handle(picture.handle());
        check(!picture.runtime_resource_flags(),"unproduced picture word stays unknown");
        first.assign_resource_state(picture.handle(),{0x491005a0U,rr});
        first.mutate_resource_low_byte(picture_resource,4U,0xa0U);
        check(picture.runtime_resource_flags()==0x49100504U &&
              first.resource_state(picture.handle())->context==rr &&
              (first.resource_state(child)->flags&4U) && (first.resource_state(root)->flags&4U),
              "picture view follows canonical flags and propagation through every ancestor preserves context");
        first.set_resource_flags_no_maintenance(rr,0xc00U,0,modes);
        const auto fresh=first.create_default_camera_resource(false,[](auto){});
        check(fresh && (first.resource_state(*fresh)->flags&0xc00U)==0xc00U &&
              first.camera_for_owner(*fresh).flags()==0x10020U,
              "genuinely mutated root hide state feeds DefaultCam without changing camera-owner flags");
        first.mutate_resource_low_byte(picture_resource,0,4U);
        check(picture.runtime_resource_flags()==0x49100500U && picture.source_flags()==0,
              "picture view survives dynamic resource storage growth without changing authored flags");
      }
      const auto original_count=first.hierarchy().size();
      const auto children=first.child_owners(first.root_handle());
      check(!first.resource_state(first.root_handle()),"post-load root state is unknown until its actual producer publishes it");
      rejects([&]{(void)first.create_default_camera_resource(false,[](auto){});});
      check(first.hierarchy().size()==original_count && !first.default_camera_handle(),"unknown root cannot manufacture default camera");
      first.assign_resource_state(first.root_handle(),{0x48000000U,first.resource_handle(first.source_handle(0))});
      unsigned queued=0;
      const auto created=first.create_default_camera_resource(false,[&](auto resource){
        ++queued;const auto owner=first.resource_owner(resource);
        check(first.default_camera_handle()==owner && first.resource_parent(owner)==first.resource_handle(first.root_handle()),
              "transform hook receives actual attached camera resource identity");
        check(first.default_camera_metadata()->name=="DefaultCam" && first.default_camera_metadata()->class_identifier==0x400003,
              "normal synthesized camera retains its name and concrete class identity");
        const auto& node=first.hierarchy().at(first.hierarchy_index(owner));
        check(node.position==std::array<float,3>{0,50,-200} &&
              first.resource_state(owner)->flags==0x49100000U && first.camera_for_owner(owner).flags()==0x10020,
              "loader transform and separate owner flag visible before queue callback");
        rejects([&]{(void)first.create_default_camera_resource(false,[](auto){});});
        rejects([&]{(void)first.camera_resource_view(owner);});
      });
      check(created && queued==1 && first.hierarchy().size()==original_count+1 &&
            first.hierarchy_index(*created)==original_count && !first.source_index(*created) &&
            created->value>interleaved.source_handle(interleaved.resources().sources().directory().size()-1).value,
            "dynamic owner uses application allocation, not next hierarchy/source arithmetic");
      rejects([&]{(void)interleaved.hierarchy_index(*created);});
      rejects([&]{(void)first.hierarchy_index(interleaved.root_handle());});
      auto appended=first.child_owners(first.root_handle());
      check(appended.size()==children.size()+1 && appended.back()==*created &&
            std::equal(children.begin(),children.end(),appended.begin()),"default camera appends without rebuilding authored children");
      check(first.additional_owner_order().size()==original_count-1 && first.owner_components(*created).empty() &&
            first.camera_for_owner(*created).priority()==0 && !first.camera_for_owner(*created).parameters().authored &&
            first.registered_cameras().entries().empty() && !first.sound_listener(),
            "resource creation does not fabricate Preview enrollment, priority, renderer membership or listener assignment");
      check(first.resource_state(*created)->context==first.resource_handle(first.source_handle(0)) &&
            first.camera_context(*created)==first.root_handle(),"resource context and owner room remain distinct");
      rejects([&]{(void)first.create_default_camera_resource(false,[](auto){});});
      first.register_camera(*created,0,{[]{return 1920;},[]{return 1080;},[]{return false;},{}});
      check(first.sound_listener()->owner==*created && first.sound_listener()->context==first.root_handle() &&
            first.camera_for_owner(*created).renderer_width()==1920,"same synthesized owner participates in explicit camera/listener registration");
      check(!first.create_default_camera_resource(false,{}),"existing registered camera suppresses creation before requesting missing services");
      first.set_camera_context(*created,first.source_handle(1));
      check(first.sound_listener()->context==first.source_handle(1) &&
            first.resource_state(*created)->context==first.resource_handle(first.source_handle(0)),"live owner context changes do not overwrite resource association");
      camera_application.reset_clock();camera_application.clock().assign_crt_mode(true);camera_application.clock().set_rate(1);
      PreviewCameraComponent preview(camera_application.live_variables());
      PreviewCameraInput input{{0,0},0,{},{},false};
      preview.update(camera_application,first.camera_for_owner(*created),first.camera_resource_view(*created),input,[]{});
      camera_sample+=100;camera_application.advance_crt();camera_application.clock().publish_scene(false);input.pointer={2,1};
      const auto old_basis=first.hierarchy().at(first.hierarchy_index(*created)).matrix;
      preview.update(camera_application,first.camera_for_owner(*created),first.camera_resource_view(*created),input,[&]{
        ++queued;check(first.hierarchy().at(first.hierarchy_index(*created)).matrix!=old_basis,
                       "Preview queue reads the same changed hierarchy, not a detached pose");
      });
      check(queued==2 && first.camera_for_owner(*created).flags()==0x10020,"canonical pointer motion preserves separate owner flags");
      camera_application.sound_records().clear_scene_listener();
    }
}

static OFF_NOINLINE void test_default_camera_and_archive_lifecycle() {
    Fixture fixture;
    std::int32_t clock_sample=1000;
    off::runtime::SceneComponentSequence component_sequence{[]{return std::uint32_t{0};}};
    off::runtime::ApplicationServices application(
        off::runtime::ClockExecutionPolicy::no_recording_or_replay,
        {[] { return std::int64_t{99}; },[&] { return clock_sample; }},
        []() -> off::audio::SoundVolumeBackend* { return nullptr; });
    for(bool mode:{false,true}) for(std::uint32_t parent_flags:{0U,0x400U,0x800U,0xc00U,0x40000U,0x40000000U,0xffffffffU}) {
      off::graphics::IntroRuntime host(fixture.build(),application,component_sequence);
      host.assign_resource_state(host.root_handle(),{parent_flags,{}});
      const auto camera_owner=*host.create_default_camera_resource(mode,[](auto){});
      const auto expected=(mode?0x01100000U:0x09000000U)|(parent_flags&0xc00U)|
          ((parent_flags&0x40040000U)?0x40000000U:0U)|0x100000U;
      check(host.resource_state(camera_owner)->flags==expected && host.resource_state(host.root_handle())->flags==parent_flags,
            "fresh camera inherits only approved hide/context bits, for both live allocation modes");
      check(host.resource_state(camera_owner)->context==((parent_flags&0x40000U)?host.resource_handle(host.root_handle()):
            off::graphics::IntroRuntimeResourceHandle{}),"spatial-parent marker selects root, otherwise null context stays null");
    }
    {
      off::graphics::IntroRuntime host(fixture.build(),application,component_sequence);
      host.assign_resource_state(host.root_handle(),{0,{}});
      rejects([&]{(void)host.create_default_camera_resource(false,[](auto){throw std::runtime_error("queue failed");});});
      const auto owner=*host.default_camera_handle();
      check(host.default_camera_failed() && host.resource_state(owner)->flags==0x09100000U &&
            host.hierarchy().at(host.hierarchy_index(owner)).position==std::array<float,3>{0,50,-200},
            "failed queue preserves child and transform prefix without admitting a successful retry");
      rejects([&]{(void)host.create_default_camera_resource(false,[](auto){});});
      rejects([&]{(void)host.camera_resource_view(owner);});
      rejects([&]{host.register_camera(owner,0,{[]{return 640;},[]{return 480;},[]{return false;},{}});});
    }
    {
      // Synthetic scene population: non-preview factories below are explicit
      // fixtures, not implementations of the corresponding retail components.
      for(bool hidden:{false,true}) {
        std::int32_t sample=0;
        off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
            {[]{return std::int64_t{0};},[&]{return sample;}});
        off::runtime::SceneComponentSequence sequence{[]{return std::uint32_t{0};}};
        off::graphics::IntroRuntime host(fixture.build(),app,sequence);
        const auto original_components=host.components().size();
        for(std::size_t i=0;i<original_components;++i) host.components().construct(i,[](auto& record){
          auto state=record.state();state.class_ordinal=1;state.requested=1;state.attached_owner=record.source().owner;
          return off::runtime::ConstructedComponent{state,[](auto&){},[](auto&){}};
        });
        const auto serial=sequence.next_identity();sequence.set_construction_mode(true);
        host.assign_resource_state(host.root_handle(),{hidden?0x09000400U:0x09000000U,{}});
        std::vector<std::string> order;
        const auto created=*host.ensure_default_camera(false,[&](auto resource){
          order.emplace_back("transform");
          check(host.resource_owner(resource)==*host.default_camera_handle() && !host.default_preview_component_index(),
                "loader transform precedes Preview attachment on same canonical child");
        },{[&]{order.emplace_back("width");return 1280;},[&]{order.emplace_back("height");return 720;},
           [&]{order.emplace_back("backend");return false;},{}});
        const auto index=*host.default_preview_component_index();auto& preview=host.components().at(index);
        check(order==std::vector<std::string>{"transform","width","height","backend"} &&
              preview.identity()==serial && preview.state().class_ordinal==152 && preview.state().priority==0 &&
              preview.state().status==0x30 && preview.state().requested==0x111 && preview.state().script_reference==0 &&
              preview.state().attached_owner==created.value && preview.state().registered_cache==0,
              "actual common serial, class default, owner binding and construction status survive full fallback order");
        check(host.default_camera_components().size()==1 && host.default_camera_components()[0]==index &&
              host.owner_components(created).size()==1 && host.camera_for_owner(created).priority()==0x40000000 &&
              host.registered_cameras().entries().front().key==0 &&
              app.live_variables().enumerate("cam_coli_enable").size()==1,
              "real attached Preview payload and live variables precede distinct camera priority/zero-key registration");
        check(preview.state().admitted==(hidden?0U:0x110U) && host.default_camera_component_mask()==(hidden?0U:0x111U) &&
              (hidden?!host.ordinary_components():(host.ordinary_components() && host.ordinary_components()->pending().size()==1)),
              "live resource hide controls actual ordinary enrollment and lazy manager creation, not camera enabled flag");
        rejects([&]{host.run_ordinary_components({});});
        rejects([&]{host.attach_default_preview_camera();});rejects([&]{host.finish_default_camera_registration({});});
        host.components().run_global_phases({[](bool,auto&,std::size_t){},
          [&](auto owner){return std::optional<std::uint32_t>{owner==created.value?host.resource_state(created)->flags:0};},
          [](auto){},[](auto&){throw std::runtime_error("unexpected fixture retirement");}});
        app.reset_clock();app.clock().assign_crt_mode(true);
        off::graphics::PreviewCameraInput keys{{0,0},0,{},{},false};
        unsigned input_reads=0,queued=0;
        off::graphics::IntroOrdinaryFrameServices frame{[]{return true;},[]{return std::optional<std::uint64_t>{};},
          [&]{++input_reads;return keys;},[&](auto resource){check(host.resource_owner(resource)==created,"ordinary queue uses same DefaultCam resource");++queued;}};
        app.assign_component_dispatch_time(99U);
        host.run_ordinary_components(frame);
        sample=1000;app.advance_crt();app.clock().publish_scene(true);keys.held[5]=true;keys.pointer={1,0};
        host.run_ordinary_components(frame);
        check(input_reads==(hidden?0U:2U) && queued==(hidden?0U:1U) &&
              host.hierarchy().at(host.hierarchy_index(created)).position[2]==(hidden?-200.0F:700.0F) &&
              preview.state().registered_cache==(hidden?0U:0x10U) &&
              app.component_dispatch_time()==(hidden?99U:0U),
              "admitted paused-bypass Preview runs through same initialized component/clock/hierarchy; hidden one never dispatches");
        frame.component_filter=[&]{return std::optional{host.component_handle(index)+1000};};
        host.run_ordinary_components(frame);check(input_reads==(hidden?0U:2U),"captured filter excludes real Preview callback");
        check(!host.ensure_default_camera(false,{},{}),"existing renderer camera prevents duplicate fallback creation");
      }
    }
    {
      using namespace off::graphics;
      off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
      check(app.component_dispatch_time()==0U,"application constructs the actual initial dispatch clock");
      off::runtime::SceneComponentSequence sequence{[&]{return *app.component_dispatch_time();}};
      off::runtime::LiveVariableHandle retired_display;
      {
        IntroRuntime host(fixture.build(),app,sequence);
        const auto prepared_resource=host.resource_handle(host.source_handle(0));
        host.construct_root();
        rejects([&]{(void)host.resource_index(prepared_resource);});
        rejects([&]{(void)host.resource_owner(prepared_resource);});
        rejects([&]{(void)host.resource_handle(host.source_handle(0));});
        rejects([&]{host.assign_resource_state(host.source_handle(0),{0x09000000U,{}});});
        rejects([&]{host.register_camera(0.0F,{});});
        check(host.associated_resource_owner(host.resource_handle(host.root_handle()))==host.root_handle() &&
              host.source_index(host.source_handle(0))==0 && host.registered_cameras().entries().empty(),
              "actual root stage removes prepared resource identities without losing catalog joins or inventing owners");
        const auto root=host.root_handle();
        const auto& state=host.root_owner_state();
        const auto* payload=host.root_group();
        const auto& record=host.components().at(0);
        retired_display=payload->display_variable();
        check(host.resource_load_stage()==IntroResourceLoadStage::root_ready && state &&
              state->name=="ROOT" && state->class_identifier==0x00100021U && state->aggregate_flags==0 &&
              state->component_mask==0x115U && state->enabled && !state->room_mode &&
              state->category_memberships.empty() && host.child_owners(root).empty(),
              "fresh ZROOM retains real class, empty live collections and separate enabled marker");
        check(host.resource_state(root)->flags==0x09000000U && !host.resource_state(root)->context.value &&
              !host.resource_parent(root).value && host.hierarchy()[0].matrix==std::array<float,9>{0,0,1,0,1,0,1,0,0} &&
              host.hierarchy()[0].position==std::array<float,3>{0,0,0},
              "whole root stage preserves constructor resource flags, identity and context");
        check(record.constructed() && record.identity()==0 && record.scheduling_interval()==0.1F &&
              record.scheduling_clock()==0 && sequence.scheduling_phase()==0.1F &&
              record.state().class_ordinal==153 && record.state().priority==0 &&
              record.state().requested==0x115U && record.state().admitted==0x110U &&
              record.state().registered_cache==0 && record.state().status==0x22U &&
              record.state().attached_owner==root.value && !record.state().script_reference,
              "RootGroup common scheduling, concrete metadata and immediate status follow actual enrollment");
        check(payload && payload->owner().value==root.value && payload->initialized() &&
              payload->display_name()==0.0F && host.root_attached_components().size()==1 &&
              host.root_attached_components()[0]==0 && host.ordinary_components()->pending().size()==1 &&
              host.ordinary_components()->pending()[0]==host.component_handle(0) &&
              host.ordinary_components()->retained().empty() &&
              app.input_maps().at(payload->input_map()).references==1 && host.registered_cameras().entries().empty(),
              "real root descriptor/map/owner attachment and pending membership do not invent camera or cache admission");
        for(std::size_t i=0;i<host.resources().sources().directory().size();++i)
          check(!host.resource_state(host.source_handle(i)) && !host.resource_parent(host.source_handle(i)).value,
                "prepared authored parent graph is not already-executed attachment");
        app.live_variables().write_float(retired_display,7.0F);
        host.mutate_resource_low_byte(host.resource_handle(root),0x80U,0);
        host.construct_root();
        check(sequence.next_identity()==1 && app.live_variables().read_float(retired_display)==7.0F &&
              host.resource_state(root)->flags==0x09000080U && app.input_maps().at(payload->input_map()).references==1,
              "existing root reuse preserves live mutations and does not reconstruct or initialize again");
        rejects([&]{(void)host.ensure_default_camera(false,[](auto){},{});});
        rejects([&]{host.run_ordinary_components({});});
        check(!host.default_camera_handle(),"root construction result cannot substitute for completed source loading");
        // Explicit synthetic remaining factories isolate the real RootGroup
        // global callback. They do not implement the retail source population.
        for(std::size_t i=1;i<host.components().size();++i) host.components().construct(i,[](auto& item){
          auto value=item.state(); value.class_ordinal=1;value.attached_owner=item.source().owner;
          return off::runtime::ConstructedComponent{value,{},{}};
        });
        unsigned root_notifications=0;
        host.components().run_global_phases({[](bool,auto&,std::size_t){},
          [&](auto owner){return std::optional<std::uint32_t>{owner==root.value?host.resource_state(root)->flags:0};},
          [&](auto owner){check(owner==root.value,"only real requested RootGroup phase notifies here");++root_notifications;},
          [](auto&){throw std::runtime_error("Unexpected root fixture retirement");}});
        check(record.state().status==0x26U && root_notifications==1 && payload->display_name()==0.0F &&
              app.input_maps().at(payload->input_map()).references==2 && host.resource_state(root)->flags==0x09000080U,
              "global phase one repeats real initializer and owner tail without confusing immediate status2 with status4");
        app.reset_clock();
        bool named_failure=false;
        try {host.run_ordinary_components({[]{return false;},[]{return std::optional<std::uint64_t>{};},{},{}});}
        catch(const std::runtime_error& error) {named_failure=std::string_view(error.what()).find("ZGROUP_RootGroup")!=std::string_view::npos;}
        check(named_failure,"unimplemented real RootGroup input processor cannot succeed as an empty callback");
      }
      check(sequence.live_count()==0 && !app.live_variables().contains(retired_display),
            "root teardown releases component identity and console descriptor before application storage");
      off::runtime::SceneComponentSequence failing_sequence{[]()->std::uint32_t{throw std::runtime_error("clock unavailable");}};
      IntroRuntime failed(fixture.build(),app,failing_sequence);
      rejects([&]{failed.construct_root();});
      check(failed.resource_load_stage()==IntroResourceLoadStage::failed && failed.resource_state(failed.root_handle())->flags==0x09000000U &&
            !failed.root_owner_state()->enabled && failing_sequence.live_count()==0,
            "root clock failure preserves constructor prefix but does not publish a successful root");
      rejects([&]{failed.construct_root();});
      rejects([&]{failed.mutate_resource_low_byte(failed.resource_handle(failed.root_handle()),1,0);});
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
        rejects([&] { (void)with_audio.audio()->open_stream(1); });
        rejects([&] { (void)with_audio.audio()->open_stream(0); }); // PCM has no incremental Vorbis route.
        check(with_audio.audio() && with_audio.audio()->record_indices().size()==1 &&
              with_audio.audio()->record_indices()[0]==1 &&
              with_audio.audio()->read_encoded(0)==Bytes(8,std::byte{0x5a}),
              "normal archive loader follows odd SND resource link into the correct global bank range");
        const auto source_resolver=with_audio.audio()->source_resolver();
        const auto source=source_resolver(0x41U);
        check(source.record.data_offset==7 && source.reader.size()==global_bytes.size(),
              "retained scene-local resolver opens only the authored global-bank source");
        rejects([&] { (void)source_resolver(16U); });
        rejects([&] { (void)with_audio.audio()->open_source_for_sound_link(16U); });
        check(with_audio.audio()->open_source_for_sound_link(0x40U).reader.size()==global_bytes.size(),
              "direct source opening also rejects WHD records absent from prepared SND owners");
        const auto retained_source_resolver=[&] {
          const auto temporary=off::graphics::load_intro_prepared_resources(sound_path);
          return temporary.audio()->source_resolver();
        }();
        check(retained_source_resolver(0x40U).reader.size()==global_bytes.size(),
              "retained resolver owns its source-bank views after prepared scene destruction");
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
}

int main(int argc, char* argv[]) {
    struct TestGroup final {
        std::string_view name;
        void (*run)();
    };
    constexpr std::array groups{
        TestGroup{"outer-loader-tail-readiness", test_outer_loader_tail_readiness},
        TestGroup{"named-global-envelopes", test_named_global_envelopes},
        TestGroup{"prepared-runtime-scopes", test_prepared_runtime_scopes},
        TestGroup{"generic-directory-lifecycle", test_generic_directory_tail_lifecycle},
        TestGroup{"typed-deferred-reader-targets", test_visual_scope_lifecycle},
        TestGroup{"basic-light-owner-runtime", test_basic_light_owner_runtime},
        TestGroup{"param-anim-light-runtime", test_param_anim_light_runtime},
        TestGroup{"particle-emitter-runtime", test_particle_emitter_runtime},
        TestGroup{"prepared-resource-variants", test_prepared_resource_variants},
        TestGroup{"controller-reader-source-consistency", test_controller_reader_source_consistency},
        TestGroup{"sound-source-transforms", test_sound_source_transforms},
        TestGroup{"runtime-and-archive-lifecycle", test_runtime_and_archive_lifecycle},
        TestGroup{"runtime-scene-identity-and-later", test_runtime_scene_identity_and_later},
        TestGroup{"default-camera-and-archive-lifecycle", test_default_camera_and_archive_lifecycle},
    };

    if (argc == 1) {
        for (const auto& group : groups) {
            group.run();
        }
    } else if (argc == 2) {
        const std::string_view selected{argv[1]};
        const auto group = std::find_if(groups.begin(), groups.end(),
                                        [selected](const TestGroup& candidate) {
                                            return candidate.name == selected;
                                        });
        if (group == groups.end()) {
            std::cerr << "Unknown intro prepared-resources test group: " << selected << '\n';
            return 2;
        }
        group->run();
    } else {
        std::cerr << "Usage: off_intro_prepared_resources_tests [test-group]\n";
        return 2;
    }
    return failures == 0 ? 0 : 1;
}
