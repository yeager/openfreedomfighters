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

  // The production binder owns the session reference.  A missing receipt must
  // fail before it can manufacture a host-facing callback table.
  check(rejects([&] {
          static_cast<void>(NormalIntroSceneLifecycleServiceAdapters::bind(
              {}, {}));
        }),
        "lifecycle adapter rejects a missing owned scene receipt");

  IntroStartupActivationServices activation_services{};
  activation_services.movie_control_phase_two = {
      [] { return false; }, [] {}, [](bool) {}, [] {}, [] { return 1U; }, [] {}};
  {
    int tail{}, lifecycle{};
    auto failed_reader = NormalIntroSceneHostFactory::create(
        evidence(), {.reader_bracket = [] { throw std::runtime_error("reader unavailable"); },
                     .outer_loader_tail = [&] { ++tail; },
                     .enter_global_lifecycle = [&] { ++lifecycle; }},
        {false, false, 44});
    check(rejects([&] { failed_reader.activate(activation_services); }) &&
              failed_reader.stage() == NormalIntroSceneHostStage::failed &&
              tail == 0 && lifecycle == 0,
          "failed reader service leaves the host inactive before tail or lifecycle work");
  }
  {
    int reader{}, lifecycle{};
    auto failed_tail = NormalIntroSceneHostFactory::create(
        evidence(), {.reader_bracket = [&] { ++reader; },
                     .outer_loader_tail = [] { throw std::runtime_error("tail unavailable"); },
                     .enter_global_lifecycle = [&] { ++lifecycle; }},
        {false, false, 44});
    check(rejects([&] { failed_tail.activate(activation_services); }) &&
              failed_tail.stage() == NormalIntroSceneHostStage::failed &&
              reader == 1 && lifecycle == 0,
          "failed tail service leaves the host inactive before global lifecycle work");
  }
  {
    int reader{}, tail{};
    auto failed_lifecycle = NormalIntroSceneHostFactory::create(
        evidence(), {.reader_bracket = [&] { ++reader; },
                     .outer_loader_tail = [&] { ++tail; },
                     .enter_global_lifecycle = [] {
                       throw std::runtime_error("lifecycle unavailable");
                     }},
        {false, false, 44});
    check(rejects([&] { failed_lifecycle.activate(activation_services); }) &&
              failed_lifecycle.stage() == NormalIntroSceneHostStage::failed &&
              reader == 1 && tail == 1,
          "failed global lifecycle service leaves the host inactive before MovieControl phase two");
  }
  std::cout << "normal intro scene host factory tests passed\n";
}
