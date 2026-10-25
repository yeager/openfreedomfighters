#pragma once

#include "off/data/compact_typed_value_decoder.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace off::data {

inline constexpr std::size_t particle_template_palette_text_byte_limit = 16U * 1024U;
using ParticleTemplatePaletteKeys = std::array<std::array<float,4>,8>;
// The service receives the authored name unchanged and owns project-prefix,
// registered-provider and loose-file lookup policy.
using ParticleTemplatePaletteResourceService = std::function<std::vector<std::byte>(std::string_view)>;

struct ParticleTemplateFieldProvenance {
    std::uint8_t value_class{};
    std::uint32_t payload_bits{};
    std::size_t encoded_bytes{};
    std::vector<std::byte> raw_payload;
};
struct ParticleTemplateDeferredState {
    std::array<float,3> vector_a{}, derived_vector{};
    std::array<float,5> scalars{};
    std::array<float,4> angles{};
    bool boolean_a{true};
    std::string palette_resource;
    std::uint32_t mode{};
    std::optional<std::string> optional_name;
    float scale{1.0F};
    ParticleTemplatePaletteKeys keys{};
    std::uint32_t final_setting{};
    std::array<std::uint32_t,128> packed_colors{};
    std::vector<ParticleTemplateFieldProvenance> fields;
};

// Detached, owned results only. The caller publishes a returned value after
// successful parsing; provider exceptions cannot expose a partially filled state.
class ParticleTemplateDeferredReader {
    [[noreturn]] static void fail() { throw std::runtime_error("malformed particle template deferred block or palette"); }
    static float binary32(float value) { volatile float rounded=value; return rounded; }
    static std::int32_t truncate(double value) {
        if(!std::isfinite(value)||value < -2147483648.0||value>=2147483648.0) return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(std::trunc(value));
    }
    static std::uint32_t integer(const CompactTypedValue& value) {
        switch(value.kind) {
        case CompactTypedValueKind::signed32:return value.u32_bits();
        case CompactTypedValueKind::binary32:return std::bit_cast<std::uint32_t>(truncate(std::bit_cast<float>(value.binary32_bits())));
        case CompactTypedValueKind::binary64:return std::bit_cast<std::uint32_t>(truncate(std::bit_cast<double>(value.binary64_bits())));
        default:return 0U;
        }
    }
    static float number(const CompactTypedValue& value) {
        switch(value.kind) {
        case CompactTypedValueKind::signed32:return static_cast<float>(value.u32_bits());
        case CompactTypedValueKind::binary32:return std::bit_cast<float>(value.binary32_bits());
        case CompactTypedValueKind::binary64:return static_cast<float>(std::bit_cast<double>(value.binary64_bits()));
        default:return 0.0F;
        }
    }
    static bool whitespace(char c) {return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f';}
    static std::string_view trim(std::string_view text) {
        while(!text.empty()&&whitespace(text.front()))text.remove_prefix(1);
        while(!text.empty()&&whitespace(text.back()))text.remove_suffix(1);
        return text;
    }
    static float decimal(std::string_view text) {
        text=trim(text);if(text.empty())fail();
        // Explicit ASCII decimal grammar prevents locale, hexadecimal, NaN and
        // infinity extensions from entering through a library's number parser.
        std::size_t i=0;if(text[i]=='+'||text[i]=='-')++i;
        bool digits=false;
        while(i<text.size()&&text[i]>='0'&&text[i]<='9'){digits=true;++i;}
        if(i<text.size()&&text[i]=='.'){++i;while(i<text.size()&&text[i]>='0'&&text[i]<='9'){digits=true;++i;}}
        if(!digits)fail();
        if(i<text.size()&&(text[i]=='e'||text[i]=='E')){
            ++i;if(i<text.size()&&(text[i]=='+'||text[i]=='-'))++i;
            const auto start=i;while(i<text.size()&&text[i]>='0'&&text[i]<='9')++i;
            if(i==start)fail();
        }
        if(i!=text.size())fail();
        if(text.front()=='+')text.remove_prefix(1);
        float result{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result,std::chars_format::general);
        if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!std::isfinite(result))fail();
        return result;
    }
public:
    // Parses authored fields without resource access or production admission.
    static ParticleTemplateDeferredState read_fields(std::span<const std::byte> block) {
        if(block.size()<5U)fail();
        std::uint32_t extent{};for(unsigned i=0;i<4U;++i)extent|=std::to_integer<std::uint32_t>(block[i])<<(i*8U);
        if(extent!=block.size())fail();
        auto remaining=block.subspan(4U);
        ParticleTemplateDeferredState state;
        const auto peek=[&]() -> unsigned {
            if(remaining.empty()||remaining.front()==std::byte{0xff})return 63U;
            return std::to_integer<unsigned>(remaining.front())&63U;
        };
        const auto next=[&]() {
            CompactTypedValueDecoder decoder(remaining);const auto value=decoder.next();
            if(value.kind==CompactTypedValueKind::terminator)fail();
            state.fields.push_back({value.value_class,
                value.kind==CompactTypedValueKind::signed32?value.u32_bits():0U,value.encoded.size(),
                {value.encoded.begin()+1,value.encoded.end()}});
            remaining=remaining.subspan(value.encoded.size());return value;
        };
        const auto scalar=[&]() {return number(next());};
        const auto text=[&]() {
            const auto value=next();
            if(value.kind!=CompactTypedValueKind::nul_terminated_string)fail();
            return std::string(reinterpret_cast<const char*>(value.payload.data()),value.payload.size()-1U);
        };
        for(auto& value:state.vector_a)value=scalar();
        for(auto& value:state.scalars)value=scalar();
        constexpr float radians=0.017453292519943295769F;
        state.angles[0]=binary32(scalar()*radians);state.angles[1]=binary32(scalar()*radians);
        if(peek()!=4U && peek()!=5U){state.angles[2]=binary32(scalar()*radians);state.angles[3]=binary32(scalar()*radians);}
        if(peek()==3U)state.boolean_a=integer(next())!=0U;
        state.palette_resource=text();
        state.mode=integer(next());
        if(peek()==4U){auto name=text();if(!name.empty())state.optional_name=std::move(name);}
        if(peek()==3U)static_cast<void>(integer(next()));
        if(peek()==2U)state.scale=scalar();
        if(peek()==2U)for(auto& row:state.keys)for(auto& channel:row)channel=scalar();
        state.final_setting=integer(next());
        if(remaining.size()!=1U||remaining.front()!=std::byte{0xff})fail();
        return state;
    }
    // Work on a copy so failure cannot alter the caller's retained fields.
    static ParticleTemplateDeferredState resolve_palette(ParticleTemplateDeferredState state,
        const ParticleTemplatePaletteResourceService& resource_service = {}) {
        validate_production_domains(state);
        if(!state.palette_resource.empty()){
            if(!resource_service)fail();
            const auto resource_bytes=resource_service(state.palette_resource);
            state.keys=parse_palette_text(resource_bytes);
        }
        if(state.scalars[0]<=0.01F)state.scalars[0]=0.01F;
        const auto reciprocal=binary32(1.0F/state.scalars[0]);
        for(std::size_t i=0;i<3U;++i)state.derived_vector[i]=binary32(state.vector_a[i]*reciprocal);
        state.packed_colors=build_d3d_palette(state.keys);
        return state;
    }
    static ParticleTemplateDeferredState read(std::span<const std::byte> block,
        const ParticleTemplatePaletteResourceService& resource_service = {}) {
        return resolve_palette(read_fields(block),resource_service);
    }
    static void validate_production_domains(const ParticleTemplateDeferredState& state) {
        for(const auto& field:state.fields) {
            if((field.value_class==8U||field.value_class==10U)&&field.payload_bits!=0U)fail();
            if((field.value_class==9U||field.value_class==12U)&&field.encoded_bytes>5U)fail();
        }
    }
    // Built-in D3D primitive-control arithmetic, intentionally neither clamped
    // nor masked per channel. All unsigned arithmetic below is modulo 2^32.
    static std::uint32_t pack_d3d_color(const std::array<float,4>& rgba) {
        const auto contribution=[&](std::size_t channel,float multiplier,unsigned shift,bool negate){
            const auto converted=truncate(binary32(rgba[channel]*multiplier));
            auto bits=std::bit_cast<std::uint32_t>(converted);
            if(negate)bits=0U-bits;
            return bits<<shift;
        };
        return contribution(0,-255.0F,16,true)+contribution(1,-255.0F,8,true)+
               contribution(2,255.0F,0,false)+contribution(3,-255.0F,24,true);
    }
    static ParticleTemplatePaletteKeys parse_palette_text(std::span<const std::byte> bytes) {
        if(bytes.size()>particle_template_palette_text_byte_limit)fail();
        std::string_view text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        ParticleTemplatePaletteKeys keys{};
        for(auto& row:keys){
            const auto end=text.find('\n');auto line=text.substr(0,end);
            if(end==std::string_view::npos)text={};else text.remove_prefix(end+1U);
            if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);
            for(std::size_t channel=0;channel<4;++channel){
                const auto comma=line.find(',');
                if((channel<3)==(comma==std::string_view::npos))fail();
                row[channel]=decimal(line.substr(0,comma));
                if(comma!=std::string_view::npos)line.remove_prefix(comma+1);
            }
        }
        if(!trim(text).empty())fail();
        return keys;
    }
    static std::array<std::uint32_t,128> build_d3d_palette(const ParticleTemplatePaletteKeys& keys) {
        std::array<std::uint32_t,128> result{};
        for(std::size_t i=0;i<result.size();++i){
            const float position=binary32(binary32(static_cast<float>(i)/127.0F)*7.0F);
            const auto lower=static_cast<std::size_t>(position);const auto upper=lower<7U?lower+1U:7U;
            const float fraction=binary32(position-static_cast<float>(lower));
            std::array<float,4> rgba{};
            for(std::size_t c=0;c<4;++c){
                const auto difference=binary32(keys[upper][c]-keys[lower][c]);
                const auto interpolation=binary32(keys[lower][c]+binary32(difference*fraction));
                rgba[c]=binary32(interpolation*(1.0F/255.0F));
            }
            result[i]=pack_d3d_color(rgba);
        }
        return result;
    }
};
}
