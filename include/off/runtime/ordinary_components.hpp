#pragma once
#include "off/runtime/component_lifecycle.hpp"
#include <optional>
#include <map>
#include <span>

namespace off::runtime {
struct OrdinarySortItem {std::uint64_t handle;std::uint32_t key;};
// Shared application-wide generator, including advancement by other consumers.
// Never reset this object when a scene or manager is recreated.
struct OrdinarySortingState {
  std::uint32_t state{0x026065caU};
  void sort(std::span<OrdinarySortItem> items);
};
struct OrdinaryOwner {std::uint64_t handle;std::optional<std::uint16_t> class_ordinal;};
struct OrdinaryMembershipServices {
  // Stable during refresh; null means stale. Handles are not component serials.
  std::function<ComponentRecord*(std::uint64_t)> resolve_component;
  std::function<std::optional<OrdinaryOwner>(std::uint64_t)> attached_owner;
  std::function<std::optional<OrdinaryOwner>(std::uint32_t)> script_owner;
};
struct OrdinaryDispatchServices {
  std::function<std::uint32_t()> scene_integer;
  std::function<void(std::uint32_t)> assign_dispatch_time;
  std::function<bool()> paused;
  std::function<std::optional<std::uint64_t>()> filter;
  // This is the direct event16/zero-payload operation, never scheduled event8.
  ComponentCallback direct_event16;
  std::function<void(OrdinaryOwner,ComponentRecord&)> owner_update;
  ComponentCallback phase_one_diagnostic;
  std::function<void(ComponentRecord&,std::uint64_t captured_owner)> retire;
  bool profiling{};
  ComponentCallback profile_begin,profile_end;
};
// Concrete pending/retained ordinary membership. Does not invent admission or
// concrete component callbacks. One controlling thread; stable objects and keys
// during refresh required. Callbacks may change admission and append pending
// work, but cannot refresh/reenter dispatch. Exceptions preserve effects and
// poison any active refresh/traversal; no rollback or invented callback success.
class OrdinaryComponentManager final {
public:
  static constexpr std::size_t retained_capacity=1200,pending_capacity=600;
  OrdinaryComponentManager(OrdinarySortingState& sorting,OrdinaryMembershipServices services);
  void enqueue(std::uint64_t handle);
  void notify_removal();
  void refresh();
  void dispatch(const OrdinaryDispatchServices& services);
  [[nodiscard]] std::span<const std::uint64_t> retained() const;
  [[nodiscard]] std::span<const std::uint64_t> pending() const;
  [[nodiscard]] std::size_t removal_count() const noexcept {return removals_;}
  [[nodiscard]] bool traversing() const noexcept {return traversing_;}
  [[nodiscard]] bool failed() const noexcept {return failed_;}
private:
  OrdinarySortingState& sorting_;
  OrdinaryMembershipServices services_;
  std::vector<std::uint64_t> retained_,pending_;
  std::map<std::uint64_t,std::pair<std::uint32_t,std::uint32_t>> retained_keys_;
  std::size_t removals_{};
  bool refreshing_{},traversing_{},failed_{};
  void check_idle() const;
  [[nodiscard]] ComponentRecord* resolve(std::uint64_t handle) const;
  [[nodiscard]] ComponentRecord& require_live(std::uint64_t handle) const;
  [[nodiscard]] std::optional<OrdinaryOwner> owner(ComponentRecord& record) const;
  [[nodiscard]] std::pair<std::uint32_t,std::uint32_t> key(ComponentRecord& record) const;
  void compact();
};
} // namespace off::runtime
