#include "off/data/particle_emitter_component_payload.hpp"
#include <iostream>
#include <vector>
using Bytes=std::vector<std::byte>;
static void check(bool ok){if(!ok)throw std::runtime_error("ParticleEmitter synthetic assertion failed");}
template<class F> static void rejects(F f){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}check(rejected);}
static void word(Bytes& b,std::uint32_t v){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::byte>(v>>(8*i)));}
static void value(Bytes& b,unsigned tag,std::uint32_t v){b.push_back(static_cast<std::byte>(tag));word(b,v);}
static Bytes payload(unsigned vector_tag=0x42U,unsigned vector_count=3U) {
    Bytes b;value(b,8U,0x80000009U);value(b,3U,1U);value(b,3U,0U);value(b,3U,0xffffffffU);value(b,2U,0x3f800000U);
    for(unsigned g=0;g<2;++g){for(unsigned i=0;i<vector_count;++i)value(b,i?vector_tag:0x82U,0x3f800000U);if(g==0){value(b,2U,0x43340000U);value(b,2U,0xc2b40000U);}}
    value(b,2U,0U);value(b,10U,0x10001U);value(b,3U,1U);value(b,3U,0U);value(b,2U,0U);return b;
}
static Bytes block(const Bytes& p){Bytes b;word(b,static_cast<std::uint32_t>(p.size()+6U));b.insert(b.end(),p.begin(),p.end());b.push_back(std::byte{6});b.push_back(std::byte{0xff});return b;}
int main(){try{
    using Reader=off::data::ParticleEmitterComponentPayloadReader;
    auto p=payload();auto s=Reader::read(p);check(s.consumed_bytes==p.size()&&s.template_name.empty());
    check(s.template_reference.bits==0x80000009U&&s.template_reference.value_class==8U&&s.event_reference.bits==0x10001U);
    check(s.scalar_a==static_cast<float>(0xffffffffU)&&std::abs(s.lower_angle-3.1415927F)<0.000001F);
    check(Reader::read_block(block(p)).boolean_a);
    p[5]=std::byte{8};check(Reader::read(p).consumed_references[1].bits==1U && Reader::read(p).consumed_references[1].value_class==8U);p=payload();
    for(auto n:{2U,4U})rejects([&]{Reader::read(payload(0x42U,n));});
    rejects([&]{Reader::read(payload(0x82U));});
    p.push_back(std::byte{7});word(p,4U);check(Reader::read(p).template_name.empty());
    p=payload();p.push_back(std::byte{7});word(p,7U);p.push_back(std::byte{'A'});p.push_back(std::byte{0});p.push_back(std::byte{'B'});check(Reader::read(p).template_name=="A");
    p=payload();p.push_back(std::byte{4});p.push_back(std::byte{'A'});rejects([&]{Reader::read(p);});
    p.push_back(std::byte{0});check(Reader::read(p).template_name=="A");
    value(p,12U,4U);check(Reader::read(p).consumed_bytes==p.size()-5U);check(Reader::read_block(block(p)).template_name=="A");
    for(auto cls:{0U,6U,12U,13U,63U}){p=payload();p[0]=static_cast<std::byte>(cls);rejects([&]{Reader::read(p);});}
    for(auto cls:{3U,8U,10U,11U}){p=payload();p[0]=static_cast<std::byte>(cls);check(Reader::read(p).template_reference.bits==0x80000009U);}
    const std::array<std::optional<std::uint32_t>,2> event_map{std::nullopt,0x12345U};
    check(off::data::resolve_particle_emitter_event_reference({1U,10U},event_map)==0x2345U);
    check(off::data::resolve_particle_emitter_event_reference({0U,10U},event_map)==0U);
    rejects([&]{static_cast<void>(off::data::resolve_particle_emitter_event_reference({2U,10U},event_map));});
    for(auto bits:{0x7fc00000U,0x7f800000U,0x4f000000U}){p=payload();p[0]=std::byte{2};for(unsigned i=0;i<4;++i)p[i+1]=static_cast<std::byte>(bits>>(8*i));check(Reader::read(p).template_reference.bits==0x80000000U);}
    for(auto cls:{4U,5U,7U,9U}){auto tail=payload();p.clear();p.push_back(static_cast<std::byte>(cls));if(cls<6U)p.push_back(std::byte{0});else word(p,4U);p.insert(p.end(),tail.begin()+5,tail.end());check(Reader::read(p).template_reference.bits==0U);}
    p=payload();auto b=block(p);b.push_back(std::byte{0});rejects([&]{Reader::read_block(b);});
    b=block(p);b[b.size()-2]=std::byte{0xff};rejects([&]{Reader::read_block(b);});
    for(std::size_t n=0;n<p.size();++n)rejects([&]{Reader::read(std::span(p).first(n));});
    std::cout<<"ParticleEmitter payload tests passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
