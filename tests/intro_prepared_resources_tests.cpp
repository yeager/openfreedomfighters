#include "off/graphics/intro_prepared_resources.hpp"
#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/preview_camera_component.hpp"
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
Bytes list(std::initializer_list<std::uint32_t> references,std::uint32_t bias=0) {
    Bytes b(4); scalar(b, 0x89, static_cast<std::uint32_t>(4 + references.size() * 4));
    for (auto r : references) word(b, r?r+bias:0);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes controller(std::uint32_t bias=0) {
    Bytes b(4); scalar(b, 9, 4); b.push_back(std::byte{6});
    scalar(b, 0x88, 6+bias); scalar(b, 0x88, 7+bias); scalar(b, 8, 0);
    b.push_back(std::byte{0x84}); text(b, "IndependentDestination");
    scalar(b, 0x83, 17); scalar(b, 0x88, 0); scalar(b, 8, 2);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes first_cut(std::uint32_t bias=0) {
    Bytes b(4); scalar(b, 0x89, 8); word(b, 5+bias); b.push_back(std::byte{6});
    constexpr std::array<std::uint8_t, 7> tags{0x83, 0x83, 3, 8, 3, 0x83, 3};
    for (std::size_t i = 0; i < tags.size(); ++i) scalar(b, tags[i], i == 3 ? 0 : 1);
    floating(b, 2, -0.0F); b.push_back(std::byte{6});
    for (std::uint32_t i = 0; i < 5; ++i) {
        scalar(b, 3, 31 - i); scalar(b, 0x8a, i == 4 ? 0 : 1);
        scalar(b, 0x88, 2+bias); scalar(b, 0x83, 100 + i);
        b.push_back(std::byte{4}); text(b, "IndependentTarget"); b.push_back(std::byte{6});
    }
    finish(b); return b;
}
Bytes member(std::uint32_t bias=0) {
    Bytes b(4); scalar(b, 0x89, 28);
    for (auto r : {9U, 4U, 0U, 2U, 0U, 0U}) word(b, r?r+bias:0);
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
Bytes window(std::uint32_t bias=0) {
    Bytes b(4); scalar(b, 3, 19); floating(b, 2, -0.0F);
    scalar(b, 3, 7); scalar(b, 3, 9); scalar(b, 3, 23);
    b.push_back(std::byte{6}); b.push_back(std::byte{6});
    scalar(b, 0x88, 9+bias); scalar(b, 0x88, 0); scalar(b, 0x88, 2+bias);
    scalar(b, 0x83, 5); scalar(b, 0x83, 6); scalar(b, 3, 7);
    b.push_back(std::byte{6}); finish(b); return b;
}
Bytes sound_owner(std::uint32_t bias=0) {
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
    scalar(b,0x88,2+bias); scalar(b,0x0a,1); b.push_back(std::byte{6});
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
    explicit Fixture(bool include_sound=false,bool leading_group=false) : payload(1024), snd(16) {
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
        const std::uint32_t bias=leading_group?1U:0U;
        const std::array<Bytes, 10> blocks{camera(), picture(false), controller(bias), picture(true), member(bias), list({8, 8},bias), list({4, 2, 4},bias), first_cut(bias), window(bias),sound_owner(bias)};
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
            payload.insert(payload.end(),deferred_blob.begin(),deferred_blob.end());
            while(payload.size()%4) payload.push_back(std::byte{0});
            const auto directory=payload.size();
            word(payload,static_cast<std::uint32_t>(node_count+1));
            word(payload,static_cast<std::uint32_t>(record/4));word(payload,0);
            const Bytes entries(payload.begin()+36,payload.begin()+static_cast<std::ptrdiff_t>(36+8*node_count));
            payload.insert(payload.end(),entries.begin(),entries.end());
            set(payload,0,static_cast<std::uint32_t>(directory));
            const auto pools=payload.size();word(payload,3);
            payload.resize(payload.size()+3*24*4);
            set(payload,pools+4,2); // First group and window share category zero.
            set(payload,pools+4+2*24*4+4,2);
            set(payload,pools+4+2*24*4+12,include_sound?7U:6U);
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
      Fixture leading(false,true);
      const auto prepared=leading.build();
      const auto& image=prepared.sources();
      check(image.directory().front().source_type==0x00100001U &&
            image.directory().front().object_flags==0x03200000U &&
            image.pool_groups().size()==3 && image.pool_groups()[0].slot_count==2 &&
            image.pool_groups()[1].slot_count==0 && image.directory()[0].local_slot_index==0 &&
            image.directory()[1].local_slot_index==1 && prepared.controller_index()==3 &&
            prepared.first_cut_index()==8 && prepared.camera_index()==9,
            "leading synthetic group preserves biased role joins and interleaved pool counts");
      off::runtime::ApplicationServices app(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
          {[]{return std::int64_t{0};},[]{return std::int32_t{0};}});
      off::runtime::SceneComponentSequence sequence{[&]{return *app.component_dispatch_time();}};
      off::graphics::IntroRuntime host(leading.build(),app,sequence);
      rejects([&]{host.allocate_initial_source_scope();});
      check(!host.loading_progress(),"prepared host does not invent initialized scene loading progress");
      rejects([&]{host.begin_source_loading_without_engine_renderer();});
      check(!host.loading_progress(),"premature load begin does not reset retained progress");
      host.construct_root();
      const auto root_resource=host.resource_handle(host.root_handle());
      check(!host.loading_progress(),"root construction does not initialize loading progress");
      host.begin_source_loading_without_engine_renderer();
      check(host.loading_progress()==0.8F,"native once-only reset and first-row stage three retain binary32 progress");
      const auto scopes=host.source_resource_scopes();
      check(scopes.size()==1 && scopes[0].count_group==0 && scopes[0].resources.size()==2 &&
            scopes[0].counts[0]==2 && scopes[0].next_in_partition[0]==0 &&
            host.resource_load_stage()==off::graphics::IntroResourceLoadStage::initial_scope_ready,
            "only initial synthetic scope is allocated with its real unconsumed partition cursor");
      for(std::size_t category=1;category<24;++category)
        check(scopes[0].counts[category]==0 && !scopes[0].next_in_partition[category],
              "empty partitions retain absent cursors");
      for(std::size_t source=0;source<image.directory().size();++source) {
        const auto resource=host.allocated_source_resource(source);
        if(source>=2) {
          check(!resource,"later child scope resources are not eagerly constructed");
          continue;
        }
        check(resource && *resource==scopes[0].resources[source] && *resource!=root_resource &&
              !host.associated_resource_owner(*resource) &&
              host.resource_index(*resource)==host.hierarchy_index(host.source_handle(source)),
              "pool partition identities join canonical catalog storage without owner association");
        rejects([&]{(void)host.resource_owner(*resource);});
        rejects([&]{(void)host.resource_handle(host.source_handle(source));});
        const auto& state=host.resource_state_for_handle(*resource);
        const auto& pose=host.hierarchy()[host.resource_index(*resource)];
        check(state && state->flags==0x09000000U && !state->context.value &&
              pose.parent==off::graphics::no_picture_transform_parent &&
              pose.matrix==std::array<float,9>{0,0,1,0,1,0,1,0,0} &&
              pose.position==std::array<float,3>{0,0,0} &&
              std::ranges::none_of(pose.position,[](float value){return std::signbit(value);}),
              "ownerless batch resource has actual inactive constructor flags and positive-zero identity pose");
      }
      check(scopes[0].resources[0]!=scopes[0].resources[1],"distinct batch slots retain distinct identities");
      const bool allocating=false,suppressed=false;
      host.set_resource_flags_no_maintenance(scopes[0].resources[0],0x100004U,0,{allocating,suppressed});
      host.mutate_resource_low_byte(scopes[0].resources[0],2U,4U);
      check(host.resource_state_for_handle(scopes[0].resources[0])->flags==0x09100002U &&
            host.resource_state_for_handle(scopes[0].resources[1])->flags==0x09000000U &&
            host.resource_state_for_handle(root_resource)->flags==0x09000000U &&
            !host.resource_state_for_handle(root_resource)->context.value &&
            host.child_owners(host.root_handle()).empty() && sequence.live_count()==1,
            "ownerless canonical mutation changes only targeted resource, not root or component state");
      rejects([&]{host.allocate_initial_source_scope();});
      rejects([&]{host.begin_source_loading_without_engine_renderer();});
      check(host.loading_progress()==0.8F,"repeated load begin rejects without resetting completed progress");
      check(host.source_resource_scopes().size()==1 && host.source_resource_scopes()[0].resources.size()==2,
            "repeated scope allocation cannot replace retained identities");
    }
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
            sound.source.gain_multiplier==66 && sound.source.range_input_scalar==0.5F &&
            sound.source.category==7 && sound.source.enabled_option==9 &&
            std::signbit(sound.source.final_scalar) &&
            sound.source.component_groups_offset==sound_fixture.block_offsets[9]+70,
            "owner prefix preserves raw fields and leaves component groups unread");
      const auto& attachments=sound.attachments;
      check(attachments.extend.scalars==std::array<float,6>{-2.5F,0.25F,0.5F,0.75F,-0.0F,1.25F} &&
            std::signbit(attachments.extend.scalars[4]) &&
            attachments.extend.integers==std::array<std::uint32_t,4>{11,12,13,14} &&
            attachments.extend.option && attachments.extend.category==7 &&
            attachments.extend.option_a==5 && attachments.extend.option_b==6 &&
            attachments.extend.authored_output_mode==99,
            "typed Extend fields preserve varied values and unknown output enum without applying effects");
      check(attachments.notify.target_reference==2 && attachments.notify.event_reference==1 &&
            attachments.segment.start_event_reference==1 && attachments.segment.stop_event_reference==0 &&
            attachments.segment.enabled && attachments.segment.subtitles &&
            attachments.segment.probability==0.375F &&
            attachments.segment.subtitle=="Independent caption key" &&
            attachments.property_on_parent && attachments.property_key=="IndependentProperty",
            "attachment references, booleans and owned strings remain authored values");
      for(std::uint32_t group=0;group<4;++group)
        check(attachments.segment.times[group]==std::array<std::uint32_t,4>{group,group*10+1,group*10+2,group*10+3},
              "time groups retain integer units, not float reinterpretation or discarded zeros");
      const std::size_t start=sound.source.component_groups_offset;
      const auto segment_start=start+76+11;
      struct TimeVector { std::array<std::uint32_t,4> words; std::uint32_t expected; };
      constexpr std::array<TimeVector,7> time_vectors{{
        {{0,0,0,1},0x3d23d70aU}, {{1,2,3,4},0x4568b28fU},
        {{0,0,16777217U,1},0x4b800000U}, {{0,0,0xffffffffU,0},0x4f800000U},
        {{1193047U,0,0,0},0x44ee0000U}, {{0,0,0,0xffffffffU},0x4d23d70aU},
        {{0xffffffffU,0xffffffffU,0xffffffffU,0xffffffffU},0x4f851eb1U}
      }};
      for(std::size_t iteration=0;iteration<time_vectors.size();++iteration) {
        auto times=sound_fixture;
        for(std::size_t group=0;group<4;++group) {
          const auto& vector=time_vectors[(iteration+group)%time_vectors.size()];
          for(std::size_t field=0;field<4;++field)
            set(times.payload,segment_start+15+group*20+field*5+1,vector.words[field]);
        }
        const auto prepared=times.build();
        for(std::size_t group=0;group<4;++group) {
          const auto& vector=time_vectors[(iteration+group)%time_vectors.size()];
          check(std::bit_cast<std::uint32_t>(prepared.sounds()[0].segment_times[group])==vector.expected &&
                prepared.sounds()[0].attachments.segment.times[group]==vector.words,
                "segment time preparation preserves raw fields and exact unsigned wrap/binary32 rounding");
        }
      }
      auto variants=sound_fixture;
      variants.payload[start+6*5]=std::byte{2};
      variants.payload[segment_start]=std::byte{0x83};
      variants.payload[segment_start+15+3*20]=std::byte{3};
      const auto alternate=variants.image().intro_sound_attachments(9);
      check(alternate.extend.scalars==attachments.extend.scalars &&
            alternate.segment.enabled && alternate.segment.times==attachments.segment.times,
            "observed high-tag alternatives retain the same typed payload");
      std::vector<std::size_t> field_tags;
      for(std::size_t i=0;i<15;++i) field_tags.push_back(start+i*5);
      for(std::size_t i=0;i<2;++i) field_tags.push_back(start+76+i*5);
      for(std::size_t i=0;i<21;++i) field_tags.push_back(segment_start+i*5);
      field_tags.push_back(segment_start+105);
      const auto property_start=segment_start+105+1+std::string_view("Independent caption key").size()+1+1;
      field_tags.push_back(property_start); field_tags.push_back(property_start+5);
      for(const auto offset:field_tags) {
        auto malformed=sound_fixture; malformed.payload[offset]=std::byte{0x7f};
        rejects([&] { (void)malformed.build(); });
      }
      for(const auto offset:{start+40,segment_start,segment_start+100,property_start}) {
        auto malformed=sound_fixture; set(malformed.payload,offset+1,2);
        rejects([&] { (void)malformed.image().intro_sound_attachments(9); });
      }
      for(const auto field:{0U,3U,4U,5U,6U,9U}) {
        auto malformed=sound_fixture; set(malformed.payload,start+5*field+1,0x7fc00000);
        rejects([&] { (void)malformed.image().intro_sound_attachments(9); });
      }
      for(std::size_t end=start;end<sound_fixture.payload.size();++end) {
        auto truncated=sound_fixture; truncated.payload.resize(end);
        set(truncated.payload,truncated.block_offsets[9],static_cast<std::uint32_t>(end-truncated.block_offsets[9]));
        rejects([&] { (void)truncated.image().intro_sound_attachments(9); });
      }
      auto trailing=sound_fixture;
      trailing.payload.push_back(std::byte{0xff});
      set(trailing.payload,trailing.block_offsets[9],static_cast<std::uint32_t>(trailing.payload.size()-trailing.block_offsets[9]));
      rejects([&] { (void)trailing.image().intro_sound_attachments(9); });
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
          [&](auto) { calls.push_back("spatial"); return off::graphics::IntroSoundSpatialState{{3,4,5},{1,0,0}}; },
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
        check(calls==std::vector<std::string>{"parent","flags","spatial","gate","enable"} &&
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
            [](auto) { return off::graphics::IntroSoundSpatialState{{0,0,0},{0,0,1}}; },
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
        broken.spatial_state=[](auto)->off::graphics::IntroSoundSpatialState { throw std::runtime_error("live spatial unavailable"); };
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
          [](auto)->off::graphics::IntroSoundSpatialState { throw std::runtime_error("must not reach spatial after failed prepare"); },
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
