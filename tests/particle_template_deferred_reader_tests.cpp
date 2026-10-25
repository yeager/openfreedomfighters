#include "off/data/particle_template_deferred_reader.hpp"
#include <iostream>
#include <vector>

namespace {
using Bytes=std::vector<std::byte>;
using Reader=off::data::ParticleTemplateDeferredReader;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> void rejects(F action){bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}check(failed,"expected bounded parse failure");}
std::span<const std::byte> bytes(const std::string& text){return std::as_bytes(std::span(text.data(),text.size()));}
void word(Bytes& result,std::uint32_t value){for(unsigned i=0;i<4U;++i)result.push_back(static_cast<std::byte>(value>>(i*8U)));}
void scalar(Bytes& result,unsigned tag,std::uint32_t value){result.push_back(static_cast<std::byte>(tag));word(result,value);}
void floating(Bytes& result,float value){scalar(result,2U,std::bit_cast<std::uint32_t>(value));}
void string(Bytes& result,std::string_view value){result.push_back(std::byte{4});for(char c:value)result.push_back(static_cast<std::byte>(c));result.push_back(std::byte{0});}
void finish(Bytes& result){result.push_back(std::byte{0xff});const auto size=static_cast<std::uint32_t>(result.size());for(unsigned i=0;i<4U;++i)result[i]=static_cast<std::byte>(size>>(i*8U));}
Bytes fixture(bool extended=false,std::string_view resource="") {
    Bytes result(4U);for(const auto v:{2.0F,4.0F,6.0F})floating(result,v);
    for(const auto v:{2.0F,3.0F,4.0F,5.0F,6.0F})floating(result,v);
    floating(result,180.0F);floating(result,-90.0F);
    if(extended){floating(result,45.0F);floating(result,90.0F);scalar(result,3U,0U);}
    string(result,resource);scalar(result,3U,7U);
    if(extended){string(result,"auxiliary");scalar(result,3U,123U);floating(result,2.0F);for(unsigned i=0;i<32U;++i)floating(result,127.5F);}
    scalar(result,11U,9U);finish(result);return result;
}
void replace_word(Bytes& result,std::size_t at,std::uint32_t value){for(unsigned i=0;i<4U;++i)result[at+i]=static_cast<std::byte>(value>>(i*8U));}
void test_reader(){
    const auto minimal=Reader::read(fixture());
    check(minimal.boolean_a&&minimal.angles[2]==0.0F&&minimal.angles[3]==0.0F&&minimal.scale==1.0F,"optional defaults retain neutral values");
    check(!minimal.optional_name&&minimal.final_setting==9U,"optional fields can be absent");
    check(minimal.derived_vector==std::array<float,3>{1.0F,2.0F,3.0F}&&minimal.packed_colors[127]==0U,"derived vector and default zero palette");
    check(minimal.angles[0]==3.1415927410125732421875F,"angle conversion rounds to binary32");
    const auto full=Reader::read(fixture(true));
    check(!full.boolean_a&&full.optional_name=="auxiliary"&&full.scale==2.0F&&full.final_setting==9U,"optional fields consume exact supported classes without retaining discarded values");
    check(full.packed_colors[0]==0x7f7f7f7fU&&full.packed_colors[127]==0x7f7f7f7fU,"inline keys feed packed palette");
    std::string palette;for(unsigned i=0;i<8U;++i)palette+="255,0,0,255\n";
    unsigned calls=0;
    const auto resource_service=[&](std::string_view name){++calls;check(name=="authored/palette","file-service adapter receives the unchanged authored name");const auto result=bytes(palette);return Bytes(result.begin(),result.end());};
    const auto external=Reader::read(fixture(false,"authored/palette"),resource_service);
    check(calls==1U&&external.packed_colors[0]==0xffff0000U,"external provider palette replaces inline defaults");
    static_cast<void>(Reader::read(fixture(),resource_service));check(calls==1U,"empty resource never invokes service");
    for(const auto cls:{8U,10U}) {
        auto invalid=fixture(false,"authored/palette");invalid[4]=static_cast<std::byte>(cls);replace_word(invalid,5U,1U);
        rejects([&]{Reader::read(invalid,resource_service);});
        check(calls==1U,"nonzero relocation domain rejects before external provider call");
        const auto detached=Reader::read_fields(invalid);
        check(detached.fields[0].value_class==cls&&detached.fields[0].payload_bits==1U,"detached parse retains unadmitted reference provenance");
        rejects([&]{Reader::resolve_palette(detached,resource_service);});
        check(calls==1U,"separate palette resolution also validates before provider call");
    }
    std::optional<off::data::ParticleTemplateDeferredState> published;
    auto late=fixture(false,"authored/palette");late.insert(late.end()-1,std::byte{3});replace_word(late,0,static_cast<std::uint32_t>(late.size()));
    rejects([&]{published=Reader::read(late,resource_service);});check(!published&&calls==1U,"late malformed body neither publishes nor calls service");
    rejects([&]{published=Reader::read(fixture(false,"authored/palette"),[](std::string_view)->Bytes{throw std::runtime_error("synthetic provider failure");});});
    check(!published,"provider failure leaves destination unpublished");
    palette="invalid";rejects([&]{published=Reader::read(fixture(false,"authored/palette"),resource_service);});check(!published,"text failure leaves destination unpublished");
    for(auto value:{0.0F,-1.0F,-std::numeric_limits<float>::infinity()}){
        auto block=fixture();replace_word(block,20U,std::bit_cast<std::uint32_t>(value));const auto state=Reader::read(block);
        check(state.scalars[0]==0.01F&&state.derived_vector[0]==200.0F,"minimum scalar clamp precedes reciprocal");
    }
    auto ordered=fixture();replace_word(ordered,5U,std::bit_cast<std::uint32_t>(7.0F));replace_word(ordered,20U,std::bit_cast<std::uint32_t>(3.0F));
    check(std::bit_cast<std::uint32_t>(Reader::read(ordered).derived_vector[0])==0x40155556U,"reciprocal then multiply preserves distinct binary32 operations");
    auto resource_class5=fixture();resource_class5[54]=std::byte{5};check(Reader::read(resource_class5).palette_resource.empty(),"class five resource string shares exact boundary");
    auto bad_string=fixture();bad_string[55]=std::byte{'x'};bad_string.resize(56U);replace_word(bad_string,0,56U);rejects([&]{Reader::read(bad_string);});
    auto nan_block=fixture();replace_word(nan_block,20U,0x7fc00000U);check(std::isnan(Reader::read(nan_block).derived_vector[0]),"compact NaN survives comparison and derived operations");
    auto inf_block=fixture();replace_word(inf_block,20U,0x7f800000U);check(Reader::read(inf_block).derived_vector[0]==0.0F,"positive infinity gives zero reciprocal");
    for(auto cls:{3U,8U,10U,11U}){
        auto block=fixture();block[4]=static_cast<std::byte>(cls);replace_word(block,5U,0xffffffffU);const auto state=Reader::read_fields(block);
        check(state.vector_a[0]==static_cast<float>(0xffffffffU)&&state.fields[0].payload_bits==0xffffffffU&&state.fields[0].value_class==cls,"integer float conversion preserves unsigned payload and provenance");
        if(cls==8U||cls==10U)rejects([&]{Reader::validate_production_domains(state);});else Reader::validate_production_domains(state);
    }
    auto double_block=fixture();double_block.erase(double_block.begin()+4,double_block.begin()+9);Bytes encoded{std::byte{1}};
    const auto bits=std::bit_cast<std::uint64_t>(2.5);for(unsigned i=0;i<8U;++i)encoded.push_back(static_cast<std::byte>(bits>>(8U*i)));
    double_block.insert(double_block.begin()+4,encoded.begin(),encoded.end());replace_word(double_block,0,static_cast<std::uint32_t>(double_block.size()));check(Reader::read(double_block).vector_a[0]==2.5F,"binary64 numeric conversion narrows");
    for(auto cls:{4U,5U,7U,9U,12U}){
        auto block=fixture();block.erase(block.begin()+4,block.begin()+9);Bytes encoded{static_cast<std::byte>(cls)};
        if(cls==4U||cls==5U)encoded.push_back(std::byte{0});else word(encoded,4U);
        block.insert(block.begin()+4,encoded.begin(),encoded.end());replace_word(block,0,static_cast<std::uint32_t>(block.size()));check(Reader::read(block).vector_a[0]==0.0F,"consuming string and extent numeric fields yield zero");
    }
    auto nested=fixture();nested[4]=std::byte{9};replace_word(nested,5U,5U);nested.insert(nested.begin()+9,std::byte{0});replace_word(nested,0,static_cast<std::uint32_t>(nested.size()));
    const auto nested_state=Reader::read_fields(nested);rejects([&]{Reader::validate_production_domains(nested_state);});
    nested[4]=std::byte{12};const auto nested_twelve=Reader::read_fields(nested);rejects([&]{Reader::validate_production_domains(nested_twelve);});
    for(auto cls:{0U,6U,13U,63U}){auto block=fixture();block[4]=static_cast<std::byte>(cls);rejects([&]{Reader::read(block);});}
    auto bad=fixture();bad.push_back(std::byte{0});rejects([&]{Reader::read(bad);});
    bad=fixture();bad.pop_back();replace_word(bad,0,static_cast<std::uint32_t>(bad.size()));rejects([&]{Reader::read(bad);});
    const auto good=fixture();for(std::size_t n=0;n<good.size();++n)rejects([&]{Reader::read(std::span(good).first(n));});
}
void test_color_service(){
    check(Reader::pack_d3d_color({0.5F,0.5F,0.5F,0.5F})==0x7f7f7f7fU,"half channel truncates to 127");
    check(Reader::pack_d3d_color({0.0F,0.0F,2.0F,0.0F})==510U,"blue overflow carries into green without masking");
    check(Reader::pack_d3d_color({0.0F,0.0F,-1.0F,0.0F})==0xffffff01U,"negative blue contributes modulo 32 bits");
    check(Reader::pack_d3d_color({2.0F,0.0F,0.0F,0.0F})==0x01fe0000U,"red overflow carries into alpha");
    check(Reader::pack_d3d_color({0.0F,0.0F,std::numeric_limits<float>::quiet_NaN(),0.0F})==0x80000000U,"NaN conversion returns signed minimum");
    check(Reader::pack_d3d_color({0.0F,0.0F,std::numeric_limits<float>::infinity(),0.0F})==0x80000000U,"out of range conversion returns signed minimum");
    off::data::ParticleTemplatePaletteKeys keys{};
    for(std::size_t key=0;key<8;++key)keys[key]={static_cast<float>(key)*30.0F,0.0F,0.0F,255.0F};
    const auto palette=Reader::build_d3d_palette(keys);
    check(palette[0]==0xff000000U&&palette[127]==0xffd20000U,"palette endpoints preserve endpoint keys");
    check(palette[63]==0xff680000U&&palette[64]==0xff690000U,"128 entry interpolation straddles midpoint");
    for(auto& key:keys)key={127.5F,127.5F,127.5F,127.5F};
    check(Reader::build_d3d_palette(keys)[19]==0x7f7f7f7fU,"caller scales raw keys before channel packing");
}
void test_text(){
    std::string text;for(unsigned i=0;i<8;++i)text+=" +1.25, -2.5, 3e1, .5\r\n";
    const auto keys=Reader::parse_palette_text(bytes(text));check(keys[7][0]==1.25F&&keys[0][2]==30.0F,"finite ASCII decimal and CRLF accepted");
    text+=" \t\n";check(Reader::parse_palette_text(bytes(text))[0][3]==0.5F,"trailing whitespace accepted");
    rejects([&]{Reader::parse_palette_text(bytes(text+"garbage"));});
    for(const auto bad:{"nan","inf","0x1p0","1e","1 2","1e9999",""}){
        std::string invalid=std::string(bad)+",0,0,0\n";for(unsigned i=1;i<8;++i)invalid+="0,0,0,0\n";
        rejects([&]{Reader::parse_palette_text(bytes(invalid));});
    }
    for(const auto row:{"0,0,0\n","0,0,0,0,0\n"}){std::string invalid;for(unsigned i=0;i<8;++i)invalid+=row;rejects([&]{Reader::parse_palette_text(bytes(invalid));});}
    rejects([&]{Reader::parse_palette_text(bytes(std::string("0,0,0,0\n")));});
    const std::string oversize(off::data::particle_template_palette_text_byte_limit+1U,' ');rejects([&]{Reader::parse_palette_text(bytes(oversize));});
}
}
int main(){try{test_color_service();test_text();test_reader();std::cout<<"ParticleTemplate detached tests passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
