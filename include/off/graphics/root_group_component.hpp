#pragma once
#include "off/runtime/live_variables.hpp"
#include "off/runtime/input_maps.hpp"
#include <optional>
namespace off::graphics {
struct RootGroupOwnerHandle {
  std::uint64_t value{};
  friend bool operator==(RootGroupOwnerHandle,RootGroupOwnerHandle)=default;
};
class RootGroupComponent final {
public:
  // Services must outlive this stable-address payload and its descriptor lease.
  RootGroupComponent(runtime::LiveVariableRegistry* console,runtime::InputMapRegistry* input_maps);
  RootGroupComponent(const RootGroupComponent&)=delete;
  RootGroupComponent& operator=(const RootGroupComponent&)=delete;
  void bind_owner(RootGroupOwnerHandle owner);
  void initialize();
  [[nodiscard]] bool initialized() const noexcept {return initialized_;}
  [[nodiscard]] RootGroupOwnerHandle owner() const noexcept {return owner_;}
  [[nodiscard]] std::optional<float> display_name() const noexcept {return display_name_;}
  [[nodiscard]] runtime::LiveVariableHandle display_variable() const noexcept {return display_lease_.handle();}
  [[nodiscard]] runtime::InputMapHandle input_map() const noexcept {return map_;}
  [[nodiscard]] std::optional<std::uint64_t> auxiliary() const noexcept {return auxiliary_;}
  [[nodiscard]] std::optional<bool> latch() const noexcept {return latch_;}
private:
  runtime::InputMapRegistry* input_maps_;
  RootGroupOwnerHandle owner_{};
  std::optional<float> display_name_;
  std::optional<std::uint64_t> auxiliary_;
  std::optional<bool> latch_;
  runtime::InputMapHandle map_{};
  bool initialized_{};
  runtime::LiveVariableLease display_lease_;
};
}
