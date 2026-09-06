#include "off/graphics/root_group_component.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void check(bool value,const char* text) {if(!value) throw std::runtime_error(text);}
template<class F> void rejects(F f) {
  bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}
  check(caught,"expected rejection");
}
}
int main() {
  try {
    using namespace off::runtime;
    using namespace off::graphics;
    LiveVariableRegistry console;
    InputMapRegistry maps;
    LiveVariableHandle stale;
    InputMapHandle retained;
    {
      RootGroupComponent first(&console,&maps),second(&console,&maps);
      stale=first.display_variable();
      check(console.enumerate("DisplayName").size()==2 && !first.display_name() &&
            !first.auxiliary() && !first.latch() && !first.initialized() && !first.owner().value && maps.size()==0,
            "registration does not initialize or read payload fields or acquire input map");
      rejects([&]{(void)console.read_float(stale);});
      check(console.type(stale)==LiveVariableType::floating,"descriptor uses float type");
      console.write_float(stale,2.5F);
      check(first.display_name()==2.5F && !second.display_name(),"independent duplicate descriptors");
      rejects([&]{console.write_float(stale,std::numeric_limits<float>::infinity());});
      first.bind_owner({42});
      rejects([&]{first.bind_owner({43});});
      rejects([&]{second.bind_owner({});});
      first.initialize();
      retained=first.input_map();
      check(first.initialized() && first.display_name()==0.0F && !std::signbit(*first.display_name()) &&
            first.auxiliary()==0 && first.latch()==false && first.owner().value==42 &&
            maps.at(retained).references==1 && maps.at(retained).display_name=="RootControl" &&
            maps.at(retained).option==0,"initializer retains positive zero fields and real map registration");
      second.initialize();
      check(second.input_map()==retained && maps.size()==1 && maps.at(retained).references==2,
            "map identity is reused by different payloads");
      console.write_float(stale,9.0F);
      first.initialize();
      check(console.read_float(stale)==0.0F && maps.at(retained).references==3,
            "global initializer repeats reset and increments existing map reference");
      InputMapRegistry foreign;
      rejects([&]{(void)foreign.at(retained);});
      rejects([&]{(void)maps.acquire(InputMapIdentity::root_control,"RootControl",1);});
      check(maps.at(retained).references==3,"unsupported map option preserves existing references");
    }
    check(!console.contains(stale) && console.enumerate("DisplayName").empty() && maps.at(retained).references==3,
          "payload teardown removes descriptors but invents no map release");
    rejects([&]{(void)console.read_float(stale);});
    RootGroupComponent absent(nullptr,nullptr);
    absent.initialize();
    check(absent.initialized() && absent.display_name()==0.0F && !absent.input_map().value &&
          !absent.display_variable().identity,"absent services still perform local initializer");
    std::optional<float> optional;
    auto lease=console.bind("Uninitialized",optional);
    rejects([&]{(void)console.read_float(lease.handle());});
    console.write_float(lease.handle(),-1.25F);
    check(optional==-1.25F,"optional binding write engages storage");
    std::cout<<"RootGroup retained console fields and input map initialization verified.\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
