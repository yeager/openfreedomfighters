#include "off/runtime/scene_event_names.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message) {if(!value) {std::cerr<<message<<'\n';std::exit(1);}}
template<class Action> void rejects(Action action) {
  try {action();} catch(const std::runtime_error&) {return;}
  check(false,"expected explicit registry rejection");
}
}
int main() {
  static_assert(sizeof(off::runtime::SceneEventNames)<256,
                "Scene event reverse storage must not inflate each caller's stack frame");
  off::runtime::SceneEventNames names;
  check(!names.initialized() && names.counter()==0 && !names.find("FadeIn") && !names.name(0),
        "fresh event state and read-only lookup must not initialize the registry");
  check(names.declare("Unrelated")==1 && names.initialized() && names.counter()==1 &&
        names.find("CAM_ENTERCAMERA")==0x401 && names.find("cam_leavecamera")==0x400,
        "lazy reserved declarations do not consume the dynamic sequence");
  check(names.declare("fAdEiN")==2 && names.declare("FadeIn",0xffff)==2 && names.counter()==2 &&
        names.name(2)=="fadein","ASCII canonical-name hits retain identity regardless of requested ID");
  check(names.declare("FadeOut")==3 && names.counter()==3,"absent fade name advances retained scene sequence");
  check(names.declare("Explicit",900)==900 && names.counter()==3,"explicit identities do not advance dynamic counter");
  rejects([&]{(void)names.declare("Collision",900);});
  rejects([&]{(void)names.declare("ReservedCollision",0x400);});
  rejects([&]{(void)names.declare("TooLarge",0x402);});
  rejects([&]{(void)names.declare(std::string("bad\0name",8));});
  check(!names.find("Collision") && !names.find("TooLarge") && names.counter()==3 && names.name(900)=="explicit",
        "rejected declarations preserve both indices and the dynamic counter");
  const std::string non_ascii="\xc3\x84";
  const auto unicode=names.declare(non_ascii+"A");
  check(names.name(unicode)==non_ascii+"a","canonicalization changes only ASCII A through Z");
  names.clear();
  check(!names.initialized() && names.counter()==0 && !names.name(0x400) && !names.find("FadeIn"),
        "explicit scene clear releases reserved and dynamic names");
  for(std::uint16_t i=1;i<0x400;++i)
    check(names.declare("Independent"+std::to_string(i))==i,"dynamic identifiers retain sequence order");
  rejects([&]{(void)names.declare("Exhausted");});
  check(names.counter()==0x3ff && !names.find("Exhausted") && names.name(0x400)=="cam_leavecamera",
        "dynamic counter cannot overwrite reserved reverse identities or wrap");
}
