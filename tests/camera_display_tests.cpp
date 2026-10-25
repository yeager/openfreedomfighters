#include "off/graphics/camera_display.hpp"
#include <iostream>
#include <memory>
using namespace off::graphics;
void check(bool v) { if(!v) throw std::runtime_error("camera display test failed"); }
int main() { try {
  CameraRendererList list;
  CameraDisplayDimensions display{1600,900,true,false};
  check(display.aspect_snapshot()); check(!CameraDisplayDimensions{800,600,false,false}.aspect_snapshot());
  check(!CameraDisplayDimensions{600,600,false,false}.aspect_snapshot());
  bool rejected=false; try { CameraDisplayDimensions{0,1,false,false}.aspect_snapshot(); } catch(...) { rejected=true; } check(rejected);
  int selected=0,colors=0,queries=0;
  auto tail=std::make_unique<CameraRendererNode>(list,CameraDisplayDimensions{800,600,false,false},[&](auto&,float p){check(p==7);selected=1;},[&](auto){++colors;});
  auto middle=std::make_unique<CameraRendererNode>(list,display,[](auto&,float){},[](auto){});
  auto head=std::make_unique<CameraRendererNode>(list,display,[&](auto&,float){selected=3;},[](auto){});
  check(list.select(1)==head.get() && list.select(3)==tail.get() && !list.select(0) && !list.select(4));
  off::data::GmsIntroCameraSource source{}; source.viewport={0,0,1,1}; source.priority=7; source.aspect_mode=2; source.renderer_list_selector=3;
  FreshIntroCamera camera(source);
  CameraDisplayServices services{list,display,[&]{++queries;return false;}};
  apply_camera_display(camera,services); check(selected==1 && colors==1 && queries==0);
  check(camera.parameters().viewport==std::array<float,4>{0,.122F,1,1-(.122F+.122F)} && camera.render_control()==4);
  middle.reset(); check(list.select(2)==tail.get()); head.reset(); check(list.head()==tail.get());
  source.aspect_mode=3; source.renderer_list_selector=0; FreshIntroCamera mode3(source); apply_camera_display(mode3,services);
  check(queries==1 && mode3.parameters().viewport==std::array<float,4>{0,0,1,1} && mode3.render_control()==0);
  tail.reset(); check(!list.head());
  source.aspect_mode=1; FreshIntroCamera mode1(source); services.display.fullscreen=false; apply_camera_display(mode1,services);
  check(mode1.parameters().viewport[1]==.125F && mode1.render_control()==4);
  services.display.fullscreen=true; apply_camera_display(mode1,services);
  check(mode1.parameters().viewport[1]==camera_mul(.125F,camera_div(1.3333334F,camera_div(1600,900))));
  for(bool metadata:{false,true}) {
    CameraDisplayDimensions construction{metadata?1600:800,600,true,false};
    CameraRendererNode node(list,construction,[](auto&,float){},[](auto){});
    // Changing the provider after construction must not rewrite node metadata.
    construction.width=metadata?800:1600;
    check(node.aspect()==metadata && construction.aspect_snapshot()!=metadata);
    source.aspect_mode=1; FreshIntroCamera c(source);
    CameraDisplayServices svc{list,construction,[]{throw std::runtime_error("mode one must not query WideScreen");return false;}};
    apply_camera_display(c,svc);
    const float e=camera_mul(metadata?.01F:.125F,
        camera_div(1.3333334F,camera_div(float(construction.width),float(construction.height))));
    check(c.parameters().viewport==std::array<float,4>{0,e,1,camera_add(1,-camera_add(e,e))} && c.render_control()==4);
    svc.display.fullscreen=false;
    apply_camera_display(c,svc);
    check(c.parameters().viewport==std::array<float,4>{0,.125F,1,.75F});
  }
  source.aspect_mode=99; FreshIntroCamera unknown(source); apply_camera_display(unknown,services); check(unknown.render_control()==0 && unknown.parameters().viewport==source.viewport);
  source.aspect_mode=3; FreshIntroCamera keep(source); keep.begin_window_state_projection(false,false);
  apply_camera_display(keep,services); check(keep.render_control()==5);
  source.aspect_mode=99; FreshIntroCamera keep_unknown(source); keep_unknown.begin_window_state_projection(false,false);
  apply_camera_display(keep_unknown,services); check(keep_unknown.render_control()==5);
  mode1.begin_window_state_projection(false,false); check(mode1.render_control()==5);
  apply_camera_display(mode1,services); check(mode1.render_control()==4);
  mode1.begin_window_state_projection(false,true); check(mode1.render_control()==0);
  for(bool present:{false,true}) for(bool metadata:{false,true}) for(bool wide:{false,true}) {
    std::unique_ptr<CameraRendererNode> node;
    if(present) node=std::make_unique<CameraRendererNode>(list,CameraDisplayDimensions{metadata?1600:800,600,false,wide},[](auto&,float){},[](auto){});
    for(unsigned mode:{2U,3U}) {
      source.aspect_mode=mode; FreshIntroCamera c(source); int calls=0;
      CameraDisplayServices svc{list,{800,600,false,wide},[&]{++calls; return wide;}};
      apply_camera_display(c,svc);
      const bool choose=(present&&metadata)||wide;
      check(calls==((present&&metadata)?0:1));
      if(mode==2) check(c.parameters().viewport[1]==(choose?.122F:.216F) && c.render_control()==4);
      else check(c.parameters().viewport[0]==(choose?.125F:0) && c.render_control()==0);
    }
  }
  auto tuple=std::array<float,4>{-.1F,1.25F,1.5F,.333F}; auto original=tuple;
  auto v=camera_pixel_viewport(tuple,{7,11,107,111}); check(v.x==7 && v.y==136 && v.width==100 && v.height==33 && tuple==original);
  v=camera_pixel_viewport({.019F,.019F,-.25F,1},{3,9,103,109}); check(v.x==4 && v.y==10 && v.width==-25 && v.height==100);
  v=camera_pixel_viewport({0,0,1,1},{0,0,100,50}); check(v.width==100 && v.height==50);
  // Origin addition must round before truncation, including binary32 loss.
  v=camera_pixel_viewport({.75F,0,1,1},{16777216U,0,16777218U,1}); check(v.x==16777218);
  const auto rejects=[](auto operation) {
    bool caught=false; try { operation(); } catch(const std::runtime_error&) { caught=true; }
    check(caught);
  };
  rejects([] { (void)camera_pixel_viewport({0,0,1,1},{2,0,1,5}); });
  rejects([] { (void)camera_pixel_viewport({0,0,1,1},{0,0,0,5}); });
  rejects([] { (void)camera_pixel_viewport({std::numeric_limits<float>::quiet_NaN(),0,1,1},{0,0,10,10}); });
  rejects([] { (void)camera_pixel_viewport({0,0,1,std::numeric_limits<float>::infinity()},{0,0,10,10}); });
  rejects([] { (void)camera_pixel_viewport({std::numeric_limits<float>::max(),0,1,1},{0,0,10,10}); });
  std::cout<<"camera display list, tuple and viewport checks passed\n"; return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
