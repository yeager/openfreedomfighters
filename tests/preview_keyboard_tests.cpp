#include "off/graphics/fresh_intro_camera.hpp"
#include "off/graphics/preview_camera_component.hpp"
#include "off/runtime/application_services.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace off::graphics;
void check(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
template<class F> void rejects(F action) {
  bool rejected=false;try{action();}catch(const std::runtime_error&){rejected=true;}
  check(rejected,"Expected preview keyboard rejection");
}
float f(float value) {const volatile float rounded=value;return rounded;}
PreviewCameraPose pose() {return {{0,0,1,0,1,0,1,0,0},{0,0,0},0x9000400U};}
PreviewCameraInput input() {return {{0,0},0,{},{},false,0.5F};}
using Matrix=std::array<std::array<double,3>,3>;
Matrix multiply(const Matrix& a,const Matrix& b) {
  Matrix result{};
  for(std::size_t i=0;i<3;++i) for(std::size_t j=0;j<3;++j)
    for(std::size_t k=0;k<3;++k) result[i][j]+=a[i][k]*b[k][j];
  return result;
}
void rotation(const PreviewCameraPose& resource,float a,float b,float c) {
  const double sa=std::sin(a),ca=std::cos(a),sb=std::sin(b),cb=std::cos(b),sc=std::sin(c),cc=std::cos(c);
  const Matrix x{{{1,0,0},{0,ca,-sa},{0,sa,ca}}};
  const Matrix y{{{cb,0,sb},{0,1,0},{-sb,0,cb}}};
  const Matrix z{{{cc,-sc,0},{sc,cc,0},{0,0,1}}};
  const auto expected=multiply(y,multiply(x,z));
  for(std::size_t i=0;i<3;++i) {
    check(std::abs(resource.basis[6+i]-expected[0][i])<0.000002,"Keyboard X basis differs");
    check(std::abs(resource.basis[3+i]-expected[1][i])<0.000002,"Keyboard Y basis differs");
    check(std::abs(resource.basis[i]-expected[2][i])<0.000002,"Keyboard Z basis differs");
  }
}
}
int main() {
 try {
  // All modifier/arrow combinations, including simultaneous opposites. These
  // are synthetic callback fixtures, not claims of captured original input.
  const auto angle=f(15.0F*f(f(3.0F*std::bit_cast<float>(0x40490fdbU))/200.0F));
  for(unsigned modifiers=0;modifiers<8;++modifiers) for(unsigned arrows=0;arrows<16;++arrows) {
    FreshIntroCamera owner;PreviewCameraUpdate update;auto resource=pose();auto keys=input();
    const bool alt=(modifiers&1)!=0,ctrl=(modifiers&2)!=0,shift=(modifiers&4)!=0;
    const bool up=(arrows&1)!=0,down=(arrows&2)!=0,left=(arrows&4)!=0,right=(arrows&8)!=0;
    keys.held={false,alt,ctrl,shift,left,up,right,down};
    const float vertical=float(int(up)-int(down)),horizontal=float(int(left)-int(right));
    std::array<float,3> expected{};float a=0,b=0,c=0;
    if(!alt && !ctrl && !shift) {expected[2]=450*vertical;b=angle*horizontal;}
    if(ctrl && !shift) {expected[0]=-450*horizontal;expected[1]=-450*vertical;}
    if(!ctrl && shift) {a=angle*vertical;c=angle*horizontal;}
    unsigned queued=0;
    update.run(owner,resource,keys,[&](auto& live){check(&live==&resource,"Canonical resource queue identity");++queued;});
    check(resource.position==expected,"Modifier/arrow translation differs");rotation(resource,a,b,c);
    const bool changed=expected!=std::array<float,3>{} || a!=0 || b!=0 || c!=0;
    check(queued==unsigned(changed),"No extra transform queue operations");
    check(owner.flags()==0x20 && resource.resource_flags==(changed?0x9100400U:0x9000400U),"Owner and resource flag domains");
  }
  {
    FreshIntroCamera first,second;PreviewCameraUpdate update;auto resource=pose();auto keys=input();
    keys.last_scaled_increment=0;keys.held[0]=true;unsigned queued=0;
    const auto queue=[&](auto&){++queued;};
    update.run(first,resource,keys,queue);
    check(first.flags()==0x10020 && update.toggle_latch && queued==0,"Y rising hold toggles owner only");
    update.run(first,resource,keys,queue);update.run(second,resource,keys,queue);
    check(first.flags()==0x10020 && second.flags()==0x20,"Y latch is shared, not per camera");
    keys.held[0]=false;update.run(first,resource,keys,queue);
    keys.held[0]=true;update.run(second,resource,keys,queue);
    check(second.flags()==0x10020 && queued==0,"Release rearms shared latch without queue work");
    keys.held[0]=false;update.run(first,resource,keys,queue);
    keys.held[0]=true;keys.held[5]=true;keys.last_scaled_increment=0.5F;
    first.set_enabled(false,false,{});update.run(first,resource,keys,queue);
    check(first.flags()==0 && resource.position[2]==450 && queued==1,"Y does not gate movement and disabled owner is not a helper admission gate");
  }
  {
    FreshIntroCamera owner;PreviewCameraUpdate update;auto resource=pose();auto keys=input();
    keys.last_scaled_increment=0;update.run(owner,resource,keys,[](auto&){});
    keys.last_scaled_increment=0.5F;keys.raw_crt_delta=2;keys.pointer={1,2};
    keys.held[3]=true;keys.held[5]=true;keys.held[4]=true;
    update.run(owner,resource,keys,[](auto&){});
    rotation(resource,f(angle-0.8F),-0.4F,angle);
    check(resource.position==std::array<float,3>{},"Combined shift/pointer rotation does not translate");
  }
  for(std::size_t edge=0;edge<4;++edge) {
    FreshIntroCamera owner;PreviewCameraUpdate update;auto resource=pose();auto keys=input();
    keys.held[edge<2?5:4]=true;keys.edges[edge]=1;
    update.run(owner,resource,keys,[](auto&){});
    if(edge<2) check(resource.position[2]==450,"Speed mutation must not change captured current-frame translation");
    else rotation(resource,0,angle,0);
    const float expected=edge%2?f((edge<2?1.5F:3.0F)*1.2F):f((edge<2?1.5F:3.0F)/1.2F);
    check((edge<2?update.secondary_scale:update.movement_scale)==expected,"Speed event uses its correct setting");
    resource=pose();keys.edges={};update.run(owner,resource,keys,[](auto&){});
    if(edge<2) check(resource.position[2]==f(f(15*f(expected*5))*4),"Next frame observes changed translation speed");
    else rotation(resource,0,f(15*f(f(expected*std::bit_cast<float>(0x40490fdbU))/200)),0);
  }
  {
    FreshIntroCamera owner;PreviewCameraUpdate update;auto resource=pose();auto keys=input();
    keys.edges={1,1,1,1};update.run(owner,resource,keys,[](auto&){});
    check(update.secondary_scale==f(f(1.5F/1.2F)*1.2F) && update.movement_scale==f(f(3/1.2F)*1.2F),"Q W E R sequence retained");
    update.secondary_scale=0;keys.edges={1,0,0,0};update.run(owner,resource,keys,[](auto&){});
    check(update.secondary_scale==f(0.01F/1.2F),"No invented post-edge clamp");
    keys.edges={2,2,2,2};update.run(owner,resource,keys,[](auto&){});
    check(update.secondary_scale==0.01F,"Only exact edge state one activates; next update clamps");
    const auto original_flags=owner.flags();const auto original_basis=resource.basis;
    keys.last_scaled_increment=std::numeric_limits<float>::infinity();keys.held[0]=true;
    rejects([&]{update.run(owner,resource,keys,[](auto&){});});
    check(owner.flags()==original_flags && resource.basis==original_basis,"Invalid sample rejects before owner mutation");
    keys=input();std::fesetround(FE_DOWNWARD);
    rejects([&]{update.run(owner,resource,keys,[](auto&){});});std::fesetround(FE_TONEAREST);
  }
  {
    std::int32_t sample=0;
    off::runtime::ApplicationServices application(off::runtime::ClockExecutionPolicy::no_recording_or_replay,
      {[]{return std::int64_t{0};},[&]{return sample;}});
    application.reset_clock();application.clock().assign_crt_mode(true);application.clock().set_rate(2);
    sample=500;(void)application.advance_crt();application.clock().publish_scene(true);
    check(application.clock().state().scene_delta==0 && application.clock().state().last_scaled_increment==1,"Frozen scene retains scaled increment");
    PreviewCameraComponent component(application.live_variables());FreshIntroCamera owner;auto resource=pose();auto keys=input();
    keys.held[5]=true;keys.last_scaled_increment=std::numeric_limits<float>::quiet_NaN();keys.raw_crt_delta=keys.last_scaled_increment;
    component.update(application,owner,resource,keys,[](auto&){});
    check(resource.position[2]==900,"Component uses canonical last scaled increment, not supplied/raw/frozen scene delta");
    application.clock().suppress_next_accumulation();sample=600;(void)application.advance_crt();application.clock().publish_scene(true);
    component.update(application,owner,resource,keys,[](auto&){});
    check(resource.position[2]==1800,"Suppression preserves prior scaled keyboard increment");
  }
  std::cout<<"Preview keyboard branches, shared state and canonical timing verified.\n";
 }catch(const std::exception& error){std::fesetround(FE_TONEAREST);std::cerr<<error.what()<<'\n';return 1;}
}
