#include "off/graphics/normal_intro_scene_host_factory.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <typename Fn> bool rejects(Fn&& fn) {
  try { fn(); } catch (const std::runtime_error&) { return true; }
  return false;
}
off::graphics::MovieControlHostEvidence evidence() {
  using namespace off::graphics;
  const MovieControlHostReaderReceipt owner{.owner_handle = 91,
      .resource_handle = 12, .source_directory_index = 465,
      .source_offset = 44, .component_index = 8};
  return MovieControlHostEvidence::from_reader_receipts(owner, owner, 17);
}
}

int main() {
  using namespace off::graphics;
  int reader_calls{}, tail_calls{}, lifecycle_calls{};
  const auto services = [&] {
    return NormalIntroSceneHostLifecycleServices{
        .reader_bracket = [&] { ++reader_calls; },
        .outer_loader_tail = [&] { ++tail_calls; },
        .enter_global_lifecycle = [&] { ++lifecycle_calls; }};
  };

  const auto host = NormalIntroSceneHostFactory::create(
      evidence(), services(), {false, false, 44});
  check(host.stage() == NormalIntroSceneHostStage::constructed,
        "factory creates an inert host");
  check(reader_calls == 0 && tail_calls == 0 && lifecycle_calls == 0,
        "factory construction has no lifecycle side effects");

  auto missing_reader = services();
  missing_reader.reader_bracket = {};
  check(rejects([&] { static_cast<void>(NormalIntroSceneHostFactory::create(
      evidence(), std::move(missing_reader), {})); }),
      "missing reader bracket is rejected without fallback");
  auto missing_tail = services();
  missing_tail.outer_loader_tail = {};
  check(rejects([&] { static_cast<void>(NormalIntroSceneHostFactory::create(
      evidence(), std::move(missing_tail), {})); }),
      "missing outer loader tail is rejected without fallback");
  auto missing_lifecycle = services();
  missing_lifecycle.enter_global_lifecycle = {};
  check(rejects([&] { static_cast<void>(NormalIntroSceneHostFactory::create(
      evidence(), std::move(missing_lifecycle), {})); }),
      "missing global lifecycle is rejected without fallback");
  check(reader_calls == 0 && tail_calls == 0 && lifecycle_calls == 0,
        "rejected construction has no lifecycle side effects");
  std::cout << "normal intro scene host factory tests passed\n";
}
