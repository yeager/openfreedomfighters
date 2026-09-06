#include "off/graphics/preview_camera_update.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
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
void PreviewCameraUpdate::run(PreviewCameraPose& camera,const PreviewCameraInput& input,
    const std::function<void(PreviewCameraPose&)>& enqueue_transform) {
  if(busy_) throw std::runtime_error("preview camera update cannot reenter");
  if(!enqueue_transform || input.collision_visualization ||
     std::ranges::any_of(input.held,[](bool v) { return v; }) ||
     std::ranges::any_of(input.edges,[](auto v) { return v==1; }))
    throw std::runtime_error("preview camera requires supported controls and a transform queue");
  if(!finite(camera.basis) || !finite(camera.position) || !finite(input.pointer) ||
     !std::isfinite(input.raw_crt_delta) || !std::isfinite(movement_scale) || !std::isfinite(secondary_scale))
    throw std::runtime_error("preview camera inputs must be finite");
  struct Guard { bool& busy; ~Guard() { busy=false; } } guard{busy_};
  busy_=true;
  movement_scale=std::max(movement_scale,0.01F);
  secondary_scale=std::max(secondary_scale,0.01F);
  toggle_latch=false;
  auto basis=camera.basis;
  auto position=camera.position;
  if(!previous_) previous_=input.pointer;
  const float du=f(input.pointer[0]-(*previous_)[0]), dv=f(input.pointer[1]-(*previous_)[1]);
  previous_=input.pointer;
  const float scale=f(input.raw_crt_delta*0.2F);
  const float dx=f(du*scale), dy=f(dv*scale);
  if(dx!=0 || dy!=0) {
    // Separately rounded operations match the reviewed extended intermediates
    // for this ZERO translation only, including signed zeros. Do not generalize
    // this precision policy to keyboard movement.
    const float zero=f(0.0F*4.0F);
    for(std::size_t i=0;i<3;++i)
      position[i]=f(position[i]+f(f(f(zero*basis[6+i])+f(zero*basis[3+i]))+f(zero*basis[i])));
    const float a=f(0.0F-dy), b=f(0.0F-dx), c=0.0F;
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
  camera.flags|=0x100000U;
  camera.basis=basis;
  camera.flags|=0x100000U;
  enqueue_transform(camera);
}
} // namespace off::graphics
