#pragma once

#include <compare>
#include <cstdint>
#include <optional>

namespace off::simulation {

// These values are intentionally opaque.  They are supplied by a separately
// reviewed private first-mission contract; this public boundary neither names
// a retail control nor assigns a physical device mapping.
struct FirstMissionObservedAction final {
  std::uint64_t value{};
  auto operator<=>(const FirstMissionObservedAction&) const = default;
};

struct FirstMissionObservedOutcome final {
  std::uint64_t value{};
  auto operator<=>(const FirstMissionObservedOutcome&) const = default;
};

struct FirstMissionInputIntent final {
  FirstMissionObservedOutcome observed_outcome;
  auto operator<=>(const FirstMissionInputIntent&) const = default;
};

struct FirstMissionInputIntentBinding final {
  FirstMissionObservedAction observed_action;
  FirstMissionObservedOutcome observed_outcome;
  auto operator<=>(const FirstMissionInputIntentBinding&) const = default;
};

// Dependency boundary for a local reviewed-contract loader.  An implementation
// may expose exactly one observed action/outcome relation only after its own
// private evidence checks pass.  This interface deliberately has no SDL,
// scene, player, camera, rendering, or SimulationWorld dependency.
class FirstMissionInputIntentContract {
 public:
  virtual ~FirstMissionInputIntentContract() = default;

  [[nodiscard]] virtual bool admitted() const noexcept = 0;
  [[nodiscard]] virtual std::optional<FirstMissionInputIntentBinding>
  admitted_binding() const noexcept = 0;
};

// A one-shot, presentation-independent translation boundary.  It accepts only
// the exact action recorded in the admitted contract and produces only that
// contract's opaque outcome.  It cannot create physical-control mappings or
// drive simulation, scene, camera, or presentation state.
class FirstMissionInputIntentAdapter final {
 public:
  [[nodiscard]] static FirstMissionInputIntentAdapter bind(
      const FirstMissionInputIntentContract& contract);

  FirstMissionInputIntentAdapter(const FirstMissionInputIntentAdapter&) = delete;
  FirstMissionInputIntentAdapter& operator=(const FirstMissionInputIntentAdapter&) = delete;
  FirstMissionInputIntentAdapter(FirstMissionInputIntentAdapter&&) = default;
  FirstMissionInputIntentAdapter& operator=(FirstMissionInputIntentAdapter&&) = default;

  [[nodiscard]] std::optional<FirstMissionInputIntent> consume(
      FirstMissionObservedAction action);
  [[nodiscard]] bool consumed() const noexcept { return consumed_; }

 private:
  FirstMissionInputIntentAdapter(const FirstMissionInputIntentContract& contract,
                                 FirstMissionInputIntentBinding binding)
      : contract_(&contract), binding_(binding) {}

  const FirstMissionInputIntentContract* contract_{};
  FirstMissionInputIntentBinding binding_{};
  bool consumed_{};
};

}  // namespace off::simulation
