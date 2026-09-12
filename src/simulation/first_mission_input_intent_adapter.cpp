#include "off/simulation/first_mission_input_intent_adapter.hpp"

#include <stdexcept>

namespace off::simulation {

FirstMissionInputIntentAdapter FirstMissionInputIntentAdapter::bind(
    const FirstMissionInputIntentContract& contract) {
  const auto binding = contract.admitted_binding();
  if (!contract.admitted() || !binding) {
    throw std::runtime_error(
        "first-mission input-intent adapter requires an admitted contract binding");
  }
  return FirstMissionInputIntentAdapter(contract, *binding);
}

std::optional<FirstMissionInputIntent> FirstMissionInputIntentAdapter::consume(
    FirstMissionObservedAction action) {
  if (!contract_ || consumed_) {
    throw std::runtime_error("first-mission input-intent adapter is unavailable");
  }
  const auto current = contract_->admitted_binding();
  if (!contract_->admitted() || !current || *current != binding_) {
    throw std::runtime_error("first-mission input-intent contract admission changed");
  }
  if (action != binding_.observed_action) {
    return std::nullopt;
  }
  consumed_ = true;
  return FirstMissionInputIntent{binding_.observed_outcome};
}

}  // namespace off::simulation
