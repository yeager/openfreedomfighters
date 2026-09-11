#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace off::runtime {

// A scene-owned registry for targets which have actually been constructed.
// It intentionally has no catalogue/resource constructor: callers must admit
// each owner and component through the checked registration API.
class IntroLiveTargetRegistry final {
public:
  using OwnerHandle = std::uint64_t;
  using ComponentHandle = std::uint64_t;

  struct OwnerRegistration {
    OwnerHandle owner{};
    std::uint32_t authored_reference{};
    std::string name;
  };

  struct DispatchServices {
    // Runs synchronously before this registry observes component membership.
    std::function<void(OwnerHandle, std::uint16_t, std::uint32_t, OwnerHandle)> direct_target;
    // Runs once for each still-live, enabled component in registration order.
    std::function<void(OwnerHandle, ComponentHandle, std::uint16_t, std::uint32_t,
                       OwnerHandle)> direct_component;
  };

  IntroLiveTargetRegistry() = default;
  IntroLiveTargetRegistry(const IntroLiveTargetRegistry&) = delete;
  IntroLiveTargetRegistry& operator=(const IntroLiveTargetRegistry&) = delete;
  IntroLiveTargetRegistry(IntroLiveTargetRegistry&&) = delete;
  IntroLiveTargetRegistry& operator=(IntroLiveTargetRegistry&&) = delete;

  // A live sender need not have an authored target reference. Register it
  // separately so an owner that issues commands is not given a fabricated
  // reference merely to pass direct-dispatch validation. If the owner is also
  // a registered target this is an idempotent declaration.
  void register_sender(OwnerHandle owner) {
    reject_mutation_during_dispatch();
    if (owner == 0U) {
      throw std::runtime_error("invalid live intro target sender");
    }
    if (find_owner(owner) ||
        std::find(senders_.begin(), senders_.end(), owner) != senders_.end()) {
      return;
    }
    senders_.push_back(owner);
  }

  // A zero owner/reference, duplicate owner/reference, duplicate nonempty
  // name, or a registration attempted from a direct-dispatch callback is
  // rejected. An empty name has no name-resolution entry.
  void register_owner(OwnerRegistration registration) {
    reject_mutation_during_dispatch();
    if (registration.owner == 0U || registration.authored_reference == 0U ||
        find_owner(registration.owner) || resolve_reference(registration.authored_reference) ||
        (!registration.name.empty() && resolve_name(registration.name))) {
      throw std::runtime_error("invalid or duplicate live intro target owner");
    }
    owners_.push_back(Owner{std::move(registration), {}});
  }

  // Components belong to one currently-live owner and are dispatched in this
  // registration order. A component handle is globally canonical.
  void register_component(OwnerHandle owner, ComponentHandle component, bool eligible = true) {
    reject_mutation_during_dispatch();
    auto* target = find_owner(owner);
    if (!target || component == 0U || find_component(component)) {
      throw std::runtime_error("invalid or duplicate live intro target component");
    }
    target->components.push_back(Component{component, eligible});
  }

  void set_component_eligible(ComponentHandle component, bool eligible) {
    auto* record = find_component(component);
    if (!record) throw std::runtime_error("unknown live intro target component");
    record->eligible = eligible;
  }

  void unregister_component(ComponentHandle component) {
    for (auto& owner : owners_) {
      const auto found = std::find_if(owner.components.begin(), owner.components.end(),
                                      [component](const Component& item) {
                                        return item.handle == component;
                                      });
      if (found != owner.components.end()) {
        owner.components.erase(found);
        return;
      }
    }
  }

  void unregister_owner(OwnerHandle owner) {
    const auto found = std::find_if(owners_.begin(), owners_.end(),
                                    [owner](const Owner& item) {
                                      return item.registration.owner == owner;
                                    });
    if (found != owners_.end()) owners_.erase(found);
  }

  [[nodiscard]] std::optional<OwnerHandle> resolve_reference(
      std::uint32_t authored_reference) const noexcept {
    const auto found = std::find_if(owners_.begin(), owners_.end(),
                                    [authored_reference](const Owner& item) {
                                      return item.registration.authored_reference == authored_reference;
                                    });
    if (found == owners_.end()) return std::nullopt;
    return found->registration.owner;
  }

  [[nodiscard]] std::optional<OwnerHandle> resolve_name(std::string_view name) const noexcept {
    if (name.empty()) return std::nullopt;
    const auto found = std::find_if(owners_.begin(), owners_.end(),
                                    [name](const Owner& item) {
                                      return item.registration.name == name;
                                    });
    if (found == owners_.end()) return std::nullopt;
    return found->registration.owner;
  }

  [[nodiscard]] bool contains_owner(OwnerHandle owner) const noexcept {
    return find_owner(owner) != nullptr ||
           std::find(senders_.begin(), senders_.end(), owner) != senders_.end();
  }
  [[nodiscard]] bool contains_component(ComponentHandle component) const noexcept {
    return find_component(component) != nullptr;
  }

  // Direct dispatch cannot re-enter itself and new registration is closed for
  // its duration. Destruction/unregistration remains permitted: the target
  // callback executes first, then component identities are snapshotted and
  // each one is re-resolved. Thus a component removed by an earlier callback
  // is never called through a stale handle.
  void dispatch(OwnerHandle target, std::uint16_t event, std::uint32_t argument,
                OwnerHandle sender, const DispatchServices& services) {
    if (dispatching_ || !services.direct_target || !services.direct_component ||
        !contains_owner(target) || !contains_owner(sender)) {
      throw std::runtime_error("invalid or reentrant live intro target dispatch");
    }
    struct DispatchGuard final {
      bool& dispatching;
      explicit DispatchGuard(bool& value) : dispatching(value) { dispatching = true; }
      ~DispatchGuard() { dispatching = false; }
    } guard(dispatching_);

    services.direct_target(target, event, argument, sender);
    const auto* after_target = find_owner(target);
    if (!after_target) return;
    std::vector<ComponentHandle> snapshot;
    snapshot.reserve(after_target->components.size());
    for (const auto& component : after_target->components) snapshot.push_back(component.handle);
    for (const auto component : snapshot) {
      const auto* live_owner = find_owner(target);
      const auto* live_component = find_component(component);
      if (!live_owner || !live_component || !belongs_to(*live_owner, component) ||
          !live_component->eligible) continue;
      services.direct_component(target, component, event, argument, sender);
    }
  }

private:
  struct Component { ComponentHandle handle{}; bool eligible{}; };
  struct Owner { OwnerRegistration registration; std::vector<Component> components; };

  void reject_mutation_during_dispatch() const {
    if (dispatching_) throw std::runtime_error("live intro target mutation during dispatch is unsupported");
  }
  [[nodiscard]] Owner* find_owner(OwnerHandle owner) noexcept {
    const auto found = std::find_if(owners_.begin(), owners_.end(), [owner](const Owner& item) {
      return item.registration.owner == owner;
    });
    return found == owners_.end() ? nullptr : &*found;
  }
  [[nodiscard]] const Owner* find_owner(OwnerHandle owner) const noexcept {
    return const_cast<IntroLiveTargetRegistry*>(this)->find_owner(owner);
  }
  [[nodiscard]] Component* find_component(ComponentHandle component) noexcept {
    for (auto& owner : owners_) {
      const auto found = std::find_if(owner.components.begin(), owner.components.end(),
                                      [component](const Component& item) { return item.handle == component; });
      if (found != owner.components.end()) return &*found;
    }
    return nullptr;
  }
  [[nodiscard]] const Component* find_component(ComponentHandle component) const noexcept {
    return const_cast<IntroLiveTargetRegistry*>(this)->find_component(component);
  }
  [[nodiscard]] static bool belongs_to(const Owner& owner, ComponentHandle component) noexcept {
    return std::any_of(owner.components.begin(), owner.components.end(), [component](const Component& item) {
      return item.handle == component;
    });
  }

  std::vector<Owner> owners_;
  std::vector<OwnerHandle> senders_;
  bool dispatching_{false};
};

} // namespace off::runtime
