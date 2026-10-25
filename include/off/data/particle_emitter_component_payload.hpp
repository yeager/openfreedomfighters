#pragma once
#include "off/data/compact_typed_value_decoder.hpp"
#include "off/data/deferred_reader_session.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace off::data {
struct ParticleEmitterReference { std::uint32_t bits{}; std::uint8_t value_class{}; };
struct ParticleEmitterComponentPayloadState {
    ParticleEmitterReference template_reference, event_reference;
    bool boolean_a{},boolean_b{},boolean_c{},boolean_d{};
    float scalar_a{},scalar_b{},lower_angle{},upper_angle{},scalar_c{},scalar_d{};
    std::array<float,3> vector_a{},vector_b{};
    std::string template_name;
    // Provenance for every consumed value, including the optional name.
    std::vector<ParticleEmitterReference> consumed_references;
    std::size_t consumed_bytes{};
};
inline std::uint16_t resolve_particle_emitter_event_reference(
    ParticleEmitterReference reference,
    std::span<const std::optional<std::uint32_t>> mapping) {
    if(reference.bits==0U) return 0U;
    if(reference.value_class!=10U || reference.bits>=mapping.size() || !mapping[reference.bits])
        throw std::runtime_error("ParticleEmitter event reference has no mapped event");
    return static_cast<std::uint16_t>(*mapping[reference.bits]);
}
class ParticleEmitterComponentPayloadReader {
    static void fail() { throw std::runtime_error("malformed ParticleEmitter payload"); }
    static std::uint32_t integer(const CompactTypedValue& v) {
        if(v.kind==CompactTypedValueKind::signed32) return v.u32_bits();
        if(v.kind!=CompactTypedValueKind::binary32 && v.kind!=CompactTypedValueKind::binary64) return 0;
        const double d=v.kind==CompactTypedValueKind::binary32?static_cast<double>(std::bit_cast<float>(v.binary32_bits())):std::bit_cast<double>(v.binary64_bits());
        if(!std::isfinite(d)||d < -2147483648.0||d>=2147483648.0) return 0x80000000U;
        return std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(std::trunc(d)));
    }
    static float number(const CompactTypedValue& v) {
        if(v.kind==CompactTypedValueKind::binary32) return std::bit_cast<float>(v.binary32_bits());
        if(v.kind==CompactTypedValueKind::binary64) return static_cast<float>(std::bit_cast<double>(v.binary64_bits()));
        if(v.kind==CompactTypedValueKind::signed32) return static_cast<float>(v.u32_bits());
        return 0.0F;
    }
public:
    static ParticleEmitterComponentPayloadState read(std::span<const std::byte> input) {
        auto remaining=input;
        ParticleEmitterComponentPayloadState s;
        const auto next=[&]() {
            if(remaining.empty() || (std::to_integer<unsigned>(remaining.front())&0x3fU)==12U) fail();
            CompactTypedValueDecoder decoder(remaining); auto v=decoder.next();
            if(v.kind==CompactTypedValueKind::terminator) fail();
            s.consumed_references.push_back({v.kind==CompactTypedValueKind::signed32?v.u32_bits():0U,v.value_class});
            remaining=remaining.subspan(v.encoded.size()); return v;
        };
        const auto ref=[&]() {auto v=next();return ParticleEmitterReference{integer(v),v.value_class};};
        const auto scalar=[&]() {return number(next());};
        const auto boolean=[&]() {return integer(next())!=0;};
        const auto vector=[&]() {
            std::array<float,3> values{}; std::size_t count=0;
            do {if(count==values.size()) fail(); values[count++]=scalar();}
            while(!remaining.empty() && remaining.front()!=std::byte{0xff} && (std::to_integer<unsigned>(remaining.front())&0x40U));
            if(count!=values.size()) fail();
            return values;
        };
        s.template_reference=ref();s.boolean_a=boolean();s.boolean_b=boolean();
        s.scalar_a=scalar();s.scalar_b=scalar();s.vector_a=vector();
        constexpr float radians=0.017453292519943295769F;
        s.lower_angle=scalar()*radians;s.upper_angle=scalar()*radians;
        s.vector_b=vector();s.scalar_c=scalar();s.event_reference=ref();
        s.boolean_c=boolean();s.boolean_d=boolean();s.scalar_d=scalar();
        if(!remaining.empty()) {
            const auto v=next();
            auto chars=v.kind==CompactTypedValueKind::nul_terminated_string?v.payload:
                (v.value_class==7U?v.encoded.subspan(5U):std::span<const std::byte>{});
            if(!chars.empty()) {
                bool terminated=false;
                for(auto c:chars) {if(c==std::byte{0}) {terminated=true;break;} s.template_name.push_back(static_cast<char>(std::to_integer<unsigned char>(c)));}
                if(!terminated) fail();
            }
        }
        s.consumed_bytes=input.size()-remaining.size();return s;
    }
    static ParticleEmitterComponentPayloadState read_block(std::span<const std::byte> block) {
        if(block.size()<4U) fail();
        std::uint32_t extent{};for(unsigned i=0;i<4;++i) extent|=std::to_integer<std::uint32_t>(block[i])<<(i*8U);
        if(extent!=block.size()) fail();
        ParticleEmitterComponentPayloadState values;
        const std::array<DeferredComponentReader,1> readers{[&](std::span<const std::byte>& slice){values=read(slice);slice=slice.subspan(values.consumed_bytes);}};
        const auto result=DeferredComponentDispatcher::dispatch(block.subspan(4U),readers);
        if(result.continuation.size()!=1U||result.continuation.front()!=std::byte{0xff}) fail();
        return values;
    }
};
}
