#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/normal_intro_scene_session.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {
void check(bool ok, const char *message) {
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
class Boundary final : public off::graphics::NormalIntroSceneReaderBoundary {
public:
  explicit Boundary(off::graphics::IntroRuntime *value) : value_(value) {}
  void complete_postconstruction_reader_bracket(std::uint64_t saved) override {
    ++calls;
    value = saved;
    if (fail)
      throw std::runtime_error("injected");
  }
  off::graphics::IntroRuntime *runtime() noexcept override { return value_; }
  off::graphics::IntroRuntime *value_;
  std::uint64_t value{};
  unsigned calls{};
  bool fail{};
};
} // namespace
int main() {
  using namespace off::graphics;
  static_assert(!std::is_copy_constructible_v<NormalIntroSceneSession>);
  static_assert(!std::is_move_constructible_v<NormalIntroSceneSession>);
  std::byte sentinel_storage{};
  auto *sentinel = reinterpret_cast<IntroRuntime *>(&sentinel_storage);
  {
    auto boundary = std::make_unique<Boundary>(sentinel);
    auto *observed = boundary.get();
    NormalIntroSceneSession session(std::move(boundary));
    session.complete_postconstruction_reader_bracket(0x91U);
    check(session.stage() ==
                  NormalIntroSceneSessionStage::reader_bracket_complete &&
              observed->calls == 1 && observed->value == 0x91U,
          "success commits one owned reader pass");
    bool rejected{};
    try {
      session.complete_postconstruction_reader_bracket(2);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    check(rejected && observed->calls == 1, "completed pass cannot replay");
  }
  {
    auto boundary = std::make_unique<Boundary>(sentinel);
    auto *observed = boundary.get();
    boundary->fail = true;
    NormalIntroSceneSession session(std::move(boundary));
    bool failed{};
    try {
      session.complete_postconstruction_reader_bracket(7);
    } catch (const std::runtime_error &) {
      failed = true;
    }
    check(failed && session.stage() == NormalIntroSceneSessionStage::failed &&
              observed->calls == 1,
          "failure poisons session without false completion");
    bool rejected{};
    try {
      session.complete_postconstruction_reader_bracket(8);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    check(rejected && observed->calls == 1,
          "failed partial pass cannot replay");
  }
  bool rejected{};
  try {
    NormalIntroSceneSession session(nullptr);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected, "missing boundary rejected");
  std::cout << "normal intro scene session tests passed\n";
}
