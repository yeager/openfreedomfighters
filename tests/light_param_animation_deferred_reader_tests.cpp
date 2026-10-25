#include "off/data/light_param_animation_deferred_reader.hpp"
#include "off/data/deferred_reader_session.hpp"
#include "off/data/param_anim_component_payload.hpp"

#include <array>
#include <bit>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Bytes = std::vector<std::byte>;
using Family = off::data::LightOwnerFamily;
using Reader = off::data::LightParamAnimationDeferredReader;
void check(bool ok) { if (!ok) throw std::runtime_error("light reader contract failed"); }
template<class F> void rejects(F f) {
    bool failed=false;
    try { f(); } catch(const std::runtime_error&) { failed=true; }
    check(failed);
}
void byte(Bytes& b, unsigned v) { b.push_back(static_cast<std::byte>(v)); }
void word(Bytes& b, std::uint32_t v) {
    for(unsigned i=0;i<32;i+=8) byte(b,(v>>i)&255U);
}
void integer(Bytes& b, int v, unsigned tag=3) {
    byte(b,tag); word(b,std::bit_cast<std::uint32_t>(v));
}
void real(Bytes& b, float v) { byte(b,2); word(b,std::bit_cast<std::uint32_t>(v)); }
void wide(Bytes& b, double v) {
    byte(b,1); const auto bits=std::bit_cast<std::uint64_t>(v);
    for(unsigned i=0;i<64;i+=8) byte(b,static_cast<unsigned>((bits>>i)&255U));
}
void seal(Bytes& b) {
    const auto n=static_cast<std::uint32_t>(b.size());
    for(unsigned i=0;i<4;++i) b[i]=static_cast<std::byte>((n>>(8*i))&255U);
}
void required(Bytes& b,int flag) { integer(b,19,8); integer(b,-7,10); integer(b,flag,11); }
void opaque(Bytes& b,unsigned tag) {
    byte(b,tag); word(b,7); byte(b,6); byte(b,255); byte(b,198);
}
void payload(Bytes& b) {
    integer(b,1); real(b,37.5F); integer(b,0);
    // Delimiter scanning must skip unrelated trailing values, including opaque bodies.
    for(unsigned tag:{7U,9U,12U}) opaque(b,tag);
    byte(b,13); byte(b,198); byte(b,255);
}
void variants() {
    for(auto family:{Family::spotlight,Family::square_spotlight,Family::omnidirectional})
    for(int branch=0;branch<3;++branch) for(int flag:{0,-1}) {
        Bytes b(4);
        if(branch) {
            wide(b,1.25); real(b,-4.5F);
            if(branch==1) { integer(b,9,195); real(b,8.0F); }
            else { real(b,6.5F); integer(b,1); integer(b,2,8); integer(b,3,10); }
        }
        required(b,flag);
        for(unsigned tag:{7U,9U,12U}) opaque(b,tag);
        byte(b,13); byte(b,134);
        if(family==Family::spotlight && branch) {
            wide(b,0.25); real(b,0.5F); integer(b,2);
        }
        byte(b,70);
        const auto start=b.size(); payload(b); seal(b);
        const auto parsed=Reader::read(b,family);
        check(parsed.owner_flag_enabled==(flag!=0));
        check(parsed.component_suffix.data()==b.data()+start);
        check(parsed.component_suffix.size()==b.size()-start);
        check(parsed.component_extent==b.size()-start);
        const off::data::DeferredReaderWorkIdentity id{17,29,4};
        off::data::DeferredReaderSession session(id,b);
        session.prepare(id);
        session.read_owner([&](auto copy) {
            const auto owner=Reader::read(copy,family);
            return off::data::DeferredOwnerReaderResult{owner.component_suffix,owner.component_extent};
        });
        unsigned calls=0;
        const std::array<off::data::DeferredComponentReader,1> callbacks{
            [&](std::span<const std::byte>& p) {
                const auto state=off::data::ParamAnimComponentPayloadReader::read(p);
                check(state.control_b && !state.initial_control && state.scalar_c==37.5F);
                ++calls; p=p.subspan(state.consumed_bytes);
            }};
        const auto result=session.read_components(callbacks);
        check(calls==1 && result.dispatched_components==1);
        check(result.continuation.size()==1 && result.continuation.front()==std::byte{255});
    }
}
void failures() {
    Bytes good(4); required(good,1); byte(good,6); byte(good,6); payload(good); seal(good);
    // Every truncated header or required-field prefix must reject, even with a repaired extent.
    for(std::size_t size=0;size<19;++size) {
        Bytes short_block(good.begin(),good.begin()+size);
        if(size>=4) seal(short_block);
        rejects([&]{ (void)Reader::read(short_block,Family::omnidirectional); });
    }
    auto wrong_size=good; wrong_size[0]=std::byte{0};
    rejects([&]{ (void)Reader::read(wrong_size,Family::spotlight); });
    for(unsigned tag:{2U,4U,6U,13U,255U}) {
        auto wrong=good; wrong[4]=static_cast<std::byte>(tag);
        rejects([&]{ (void)Reader::read(wrong,Family::omnidirectional); });
    }
    // Integer variants do not select the class-3 compatibility branch and cannot satisfy float fields.
    for(unsigned tag:{8U,10U,11U}) {
        Bytes wrong(4); wide(wrong,1); real(wrong,2); integer(wrong,3,tag);
        real(wrong,4); required(wrong,1); byte(wrong,6); byte(wrong,6); payload(wrong); seal(wrong);
        rejects([&]{ (void)Reader::read(wrong,Family::spotlight); });
    }
    for(unsigned tag:{7U,9U,12U}) for(std::uint32_t length:{0U,3U,0xffffffffU}) {
        Bytes wrong(4); required(wrong,1); byte(wrong,tag); word(wrong,length);
        byte(wrong,6); byte(wrong,6); payload(wrong); seal(wrong);
        rejects([&]{ (void)Reader::read(wrong,Family::spotlight); });
    }
    for(bool second:{false,true}) {
        Bytes early(4); required(early,1); if(second) byte(early,6); byte(early,255); seal(early);
        const auto parsed=Reader::read(early,Family::spotlight);
        check(parsed.component_suffix.size()==1);
        const std::array<off::data::DeferredComponentReader,1> callbacks{[](auto&){}};
        rejects([&]{ (void)off::data::DeferredComponentDispatcher::dispatch(parsed.component_suffix,callbacks); });
    }
    rejects([&]{ (void)Reader::read(good,static_cast<Family>(99)); });
    for(bool extra:{false,true}) {
        auto bad=good;
        if(extra) bad.insert(bad.end()-1,std::byte{6});
        else bad.pop_back();
        seal(bad);
        const off::data::DeferredReaderWorkIdentity id{31,7,2};
        off::data::DeferredReaderSession session(id,bad);
        session.prepare(id);
        session.read_owner([](auto copy) {
            const auto owner=Reader::read(copy,Family::omnidirectional);
            return off::data::DeferredOwnerReaderResult{owner.component_suffix,owner.component_extent};
        });
        const std::array<off::data::DeferredComponentReader,1> callbacks{[](auto&){}};
        rejects([&]{ (void)session.read_components(callbacks); });
        check(session.state()==off::data::DeferredReaderSessionState::deactivated);
        rejects([&]{ (void)session.read_components(callbacks); });
    }
    for(unsigned tag:{6U,13U}) {
        const std::array input{static_cast<std::byte>(tag)};
        rejects([&]{ off::data::CompactTypedValueDecoder decoder(input); (void)decoder.next(); });
    }
}
}
int main() {
    try { variants(); failures(); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
