#include "off/graphics/preview_camera_update.hpp"
#include "off/graphics/fresh_intro_camera.hpp"
#include "off/graphics/preview_translation.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cfenv>
#include <stdexcept>

namespace off::graphics {
namespace {
float f(float value) {
  volatile float stored=value;
  if(!std::isfinite(stored)) throw std::runtime_error("preview camera arithmetic is not finite");
  return stored;
}
template<std::size_t N> bool finite(const std::array<float,N>& values) {
  return std::ranges::all_of(values,[](float value) { return std::isfinite(value); });
}
bool same(float a,float b) { return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b); }
}
void PreviewCameraUpdate::run(FreshIntroCamera& owner,PreviewCameraPose& camera,const PreviewCameraInput& input,
    const std::function<void(PreviewCameraPose&)>& enqueue_transform) {
  if(busy_) throw std::runtime_error("preview camera update cannot reenter");
  if(!enqueue_transform || input.collision_visualization)
    throw std::runtime_error("preview camera requires supported controls and a transform queue");
  if(!finite(camera.basis) || !finite(camera.position) || !finite(input.pointer) ||
     !std::isfinite(input.raw_crt_delta) || !std::isfinite(input.last_scaled_increment) ||
     !std::isfinite(movement_scale) || !std::isfinite(secondary_scale) || std::fegetround()!=FE_TONEAREST)
    throw std::runtime_error("preview camera inputs must be finite");
  struct Guard { bool& busy; ~Guard() { busy=false; } } guard{busy_};
  busy_=true;
  if(!input.held[0]) toggle_latch=false;
  else if(!toggle_latch) {toggle_latch=true;owner.toggle_preview_flag();}
  auto basis=camera.basis;
  auto position=camera.position;
  movement_scale=std::max(movement_scale,0.01F);
  secondary_scale=std::max(secondary_scale,0.01F);
  const float rotation_scale=f(f(movement_scale*std::bit_cast<float>(0x40490fdbU))/200.0F);
  const float translation_scale=f(secondary_scale*5.0F);
  const float speed_step=std::bit_cast<float>(0x3f99999aU);
  if(input.edges[0]==1) secondary_scale=f(secondary_scale/speed_step);
  if(input.edges[1]==1) secondary_scale=f(secondary_scale*speed_step);
  if(input.edges[2]==1) movement_scale=f(movement_scale/speed_step);
  if(input.edges[3]==1) movement_scale=f(movement_scale*speed_step);
  const float h=f(input.last_scaled_increment*30.0F);
  const float translation=f(h*translation_scale),rotation=f(h*rotation_scale);
  float tx=0.0F,ty=0.0F,tz=0.0F,a=0.0F,b=0.0F,c=0.0F;
  const bool alt=input.held[1],control=input.held[2],shift=input.held[3];
  const bool up=input.held[5],down=input.held[7],left=input.held[4],right=input.held[6];
  if(!alt && !control && !shift) {
    if(up) tz=f(tz+translation);
    if(down) tz=f(tz-translation);
    if(left) b=f(b+rotation);
    if(right) b=f(b-rotation);
  }
  if(control && !shift) {
    if(up) ty=f(ty-translation);
    if(down) ty=f(ty+translation);
    if(left) tx=f(tx-translation);
    if(right) tx=f(tx+translation);
  }
  if(!control && shift) {
    if(up) a=f(a+rotation);
    if(down) a=f(a-rotation);
    if(left) c=f(c+rotation);
    if(right) c=f(c-rotation);
  }
  if(!previous_) previous_=input.pointer;
  const float du=f(input.pointer[0]-(*previous_)[0]), dv=f(input.pointer[1]-(*previous_)[1]);
  previous_=input.pointer;
  const float scale=f(input.raw_crt_delta*0.2F);
  const float dx=f(du*scale), dy=f(dv*scale);
  if(tx!=0 || ty!=0 || tz!=0 || a!=0 || b!=0 || c!=0 || dx!=0 || dy!=0) {
    tx=f(tx*4.0F);ty=f(ty*4.0F);tz=f(tz*4.0F);
    const auto world=transform_preview_translation({tx,ty,tz},basis);
    for(std::size_t i=0;i<3;++i) position[i]=f(position[i]+world[i]);
    a=f(a-dy);b=f(b-dx);
    const float sa=f(static_cast<float>(std::sin(double(a)))), ca=f(static_cast<float>(std::cos(double(a))));
    const float sb=f(static_cast<float>(std::sin(double(b)))), cb=f(static_cast<float>(std::cos(double(b))));
    const float sc=f(static_cast<float>(std::sin(double(c)))), cc=f(static_cast<float>(std::cos(double(c))));
    for(std::size_t i=0;i<3;++i) {
      const float x=basis[6+i], y=basis[3+i], z=basis[i];
      const float x1=f(f(x*cc)-f(y*sc)), y1=f(f(y*cc)+f(x*sc));
      const float y2=f(f(y1*ca)-f(z*sa)), z2=f(f(z*ca)+f(y1*sa));
      basis[6+i]=f(f(x1*cb)+f(z2*sb));
      basis[3+i]=y2;
      basis[i]=f(f(z2*cb)-f(x1*sb));
    }
  }
  bool unchanged=true;
  for(std::size_t i=0;i<3;++i) unchanged=unchanged && same(position[i],camera.position[i]);
  for(std::size_t i=0;i<6;++i) unchanged=unchanged && same(basis[i],camera.basis[i]);
  if(unchanged) return;
  camera.position=position;
  camera.resource_flags|=0x100000U;
  camera.basis=basis;
  camera.resource_flags|=0x100000U;
  enqueue_transform(camera);
}
} // namespace off::graphics
