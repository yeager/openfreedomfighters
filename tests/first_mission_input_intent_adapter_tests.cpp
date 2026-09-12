#include "off/simulation/first_mission_input_intent_adapter.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using namespace off::simulation;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

class Contract final : public FirstMissionInputIntentContract {
 public:
  bool accepted{};
  std::optional<FirstMissionInputIntentBinding> binding;

  [[nodiscard]] bool admitted() const noexcept override { return accepted; }
  [[nodiscard]] std::optional<FirstMissionInputIntentBinding>
  admitted_binding() const noexcept override {
    return binding;
  }
};

template <class Function>
void rejects(Function&& function, const char* message) {
  bool rejected = false;
  try {
    function();
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  check(rejected, message);
}

}  // namespace

int main() {
  const FirstMissionInputIntentBinding observed{
      .observed_action={0U}, .observed_outcome={0U}};

  Contract unavailable;
  unavailable.accepted = false;
  unavailable.binding = observed;
  rejects([&] { (void)FirstMissionInputIntentAdapter::bind(unavailable); },
          "a contract must be admitted before binding");
  Contract incomplete;
  incomplete.accepted = true;
  rejects([&] { (void)FirstMissionInputIntentAdapter::bind(incomplete); },
          "a contract must expose one exact binding");

  Contract contract;
  contract.accepted = true;
  contract.binding = observed;
  auto adapter = FirstMissionInputIntentAdapter::bind(contract);
  check(!adapter.consumed(), "a newly bound adapter is unused");
  check(!adapter.consume({1U}),
        "an unobserved opaque action has no inferred intent");
  check(!adapter.consumed(), "an unobserved action does not consume the binding");
  const auto intent = adapter.consume({0U});
  check(intent && intent->observed_outcome == FirstMissionObservedOutcome{0U},
        "the exact observed action returns only the observed opaque outcome");
  check(adapter.consumed(), "the observed relation is accepted once");
  rejects([&] { (void)adapter.consume({0U}); },
          "the adapter cannot replay one admitted relation");

  auto changed = FirstMissionInputIntentAdapter::bind(contract);
  contract.binding = FirstMissionInputIntentBinding{
      .observed_action={0U}, .observed_outcome={2U}};
  rejects([&] { (void)changed.consume({0U}); },
          "a changed contract cannot alter a bound action outcome");

  std::cout << "first-mission input-intent adapter tests passed\n";
}
