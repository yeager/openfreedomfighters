#include "off/graphics/normal_intro_scene_host.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
  using namespace off::graphics;
  NormalIntroSceneHost host(17U, 91U, 1, {}, {false, false, 44U});
  bool rejected{};
  try { host.activate({}); } catch (const std::runtime_error&) { rejected = true; }
  check(rejected && host.stage() == NormalIntroSceneHostStage::failed,
        "missing lifecycle boundaries fail before event, view, or frame admission");
  std::cout << "normal intro scene host tests passed\n";
}
