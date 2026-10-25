#pragma once
#include "off/graphics/fresh_intro_camera.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
struct CameraDisplayDimensions {
  int width, height;
  bool fullscreen;
  bool wide_screen; // Explicit host setting, independent of dimensions.
  bool aspect_snapshot() const {
    if(width<=0 || height<=0 || width>std::numeric_limits<int>::max()/3 ||
       height>std::numeric_limits<int>::max()/4)
      throw std::runtime_error("unsupported camera display dimensions");
    return 3*width>4*height;
  }
};
class CameraRendererNode;
class CameraRendererList final {
public:
  CameraRendererList() = default;
  CameraRendererList(const CameraRendererList&) = delete;
  CameraRendererList& operator=(const CameraRendererList&) = delete;
  CameraRendererNode* head() const noexcept { return head_; }
  CameraRendererNode* select(std::uint32_t selector) const noexcept;
private:
  friend class CameraRendererNode;
  CameraRendererNode* head_{};
};
// Backend-owned node; the list must outlive all its nodes.
class CameraRendererNode final {
public:
  CameraRendererNode(CameraRendererList& list, const CameraDisplayDimensions& display,
      std::function<void(FreshIntroCamera&,float)> camera,
      std::function<void(std::uint32_t)> color)
      : list_(list), aspect_(display.aspect_snapshot()), camera_(std::move(camera)), color_(std::move(color)) {
    next_=list_.head_; if(next_) next_->previous_=this; list_.head_=this;
  }
  ~CameraRendererNode() {
    if(previous_) previous_->next_=next_; else list_.head_=next_;
    if(next_) next_->previous_=previous_;
  }
  CameraRendererNode(const CameraRendererNode&)=delete;
  CameraRendererNode& operator=(const CameraRendererNode&)=delete;
  bool aspect() const noexcept { return aspect_; }
  void bind(FreshIntroCamera& camera,float priority) { camera_(camera,priority); }
  void color(std::uint32_t value) { color_(value); }
private:
  friend class CameraRendererList;
  CameraRendererList& list_;
  bool aspect_;
  CameraRendererNode *previous_{}, *next_{};
  std::function<void(FreshIntroCamera&,float)> camera_;
  std::function<void(std::uint32_t)> color_;
};
inline CameraRendererNode* CameraRendererList::select(std::uint32_t selector) const noexcept {
  if(!selector) return nullptr;
  auto* node=head_; while(node && --selector) node=node->next_; return node;
}
struct CameraDisplayServices {
  CameraRendererList& renderers;
  CameraDisplayDimensions display;
  std::function<bool()> wide_screen;
};
inline float camera_mul(float a,float b) { volatile float v=a*b; return v; }
inline float camera_add(float a,float b) { volatile float v=a+b; return v; }
inline float camera_div(float a,float b) { volatile float v=a/b; return v; }
inline void apply_camera_display(FreshIntroCamera& camera, const CameraDisplayServices& services) {
  services.display.aspect_snapshot();
  const auto& source=camera.parameters().authored;
  if(!source) throw std::runtime_error("display reader requires authored camera");
  const auto head_aspect=[&] { return services.renderers.head() && services.renderers.head()->aspect(); };
  float e{};
  switch(source->aspect_mode) {
  case 1:
    e=0.125F;
    if(services.display.fullscreen) {
      if(head_aspect()) e=0.01F;
      e=camera_mul(e,camera_div(1.3333334F,camera_div(float(services.display.width),float(services.display.height))));
    }
    camera.set_display_tuple({0,e,1,camera_add(1,-camera_add(e,e))},true); break;
  case 2:
    e=(head_aspect() || services.wide_screen()) ? 0.122F : 0.216F;
    camera.set_display_tuple({0,e,1,camera_add(1,-camera_add(e,e))},true); break;
  case 3:
    e=(head_aspect() || services.wide_screen()) ? 0.125F : 0.0F;
    camera.set_display_tuple({e,0,camera_add(1,-camera_add(e,e)),1},false); break;
  default: break;
  }
  if(auto* renderer=services.renderers.select(source->renderer_list_selector)) {
    renderer->bind(camera,camera.parameters().registration_priority);
    renderer->color(camera.parameters().background);
  }
}
struct CameraPixelRectangle { std::uint32_t left,top,right,bottom; };
struct CameraPixelViewport { std::int32_t x,y,width,height; };
inline CameraPixelViewport camera_pixel_viewport(std::array<float,4> tuple, CameraPixelRectangle parent) {
  for(auto v:tuple) if(!std::isfinite(v)) throw std::runtime_error("nonfinite camera viewport");
  if(parent.right<=parent.left || parent.bottom<=parent.top) throw std::runtime_error("invalid camera parent rectangle");
  if(tuple[0]<0) tuple[0]=0;
  if(tuple[1]<0) tuple[1]=0;
  if(tuple[2]>1) tuple[2]=1;
  if(tuple[3]>1) tuple[3]=1;
  const float w=float(parent.right-parent.left),h=float(parent.bottom-parent.top);
  const auto integer=[](float v) {
    const double truncated=std::trunc(double(v));
    if(!std::isfinite(v) || truncated<std::numeric_limits<std::int32_t>::min() || truncated>std::numeric_limits<std::int32_t>::max())
      throw std::runtime_error("camera viewport integer out of range");
    return static_cast<std::int32_t>(truncated);
  };
  return {integer(camera_add(camera_mul(tuple[0],w),float(parent.left))),
          integer(camera_add(camera_mul(tuple[1],h),float(parent.top))),
          integer(camera_mul(tuple[2],w)),integer(camera_mul(tuple[3],h))};
}
}
