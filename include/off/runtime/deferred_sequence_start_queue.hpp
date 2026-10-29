#pragma once

#include "off/graphics/scene_reference_registry.hpp"
#include "off/runtime/scene_event_names.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>

namespace off::runtime {

// Project-owned queue. The handle is non-owning; scene/event leases in each
// entry are validated against the actual services on both operations.
// Operations are confined to the application/scene owning thread.
class DeferredSequenceStartQueue final {
  struct Entry {
    std::uint32_t owner, argument;
    std::uint64_t owner_identity;
    std::uint16_t event;
    graphics::SceneReferenceRegistry::Lease scene;
    SceneEventNames::Lease events;
  };
  struct State { std::deque<Entry> entries; };
public:
  class Handle final {
    friend class DeferredSequenceStartQueue;
    std::weak_ptr<State> state_;
    explicit Handle(const std::shared_ptr<State>& state) : state_(state) {}
  public:
    Handle() = default;
  };
  DeferredSequenceStartQueue() : state_(std::make_shared<State>()) {}
  DeferredSequenceStartQueue(const DeferredSequenceStartQueue&)=delete;
  DeferredSequenceStartQueue& operator=(const DeferredSequenceStartQueue&)=delete;
  DeferredSequenceStartQueue(DeferredSequenceStartQueue&&)=default;
  DeferredSequenceStartQueue& operator=(DeferredSequenceStartQueue&&)=default;
  [[nodiscard]] Handle handle() const { return Handle(state_); }
  [[nodiscard]] bool live(const Handle& handle) const noexcept {
    auto state=handle.state_.lock(); return state && state==state_;
  }
  [[nodiscard]] std::size_t size(const Handle& handle) const { return checked(handle).entries.size(); }
  bool enqueue(const Handle& handle, graphics::SceneReferenceRegistry& registry,
               const graphics::SceneReferenceRegistry::Lease& scene,
               const SceneEventNames& names, const SceneEventNames::Lease& events,
               std::uint32_t owner, std::uint16_t event, std::uint32_t argument) {
    auto& state=checked(handle);
    validate(registry,scene,names,events,owner,event,argument);
    // A retired entry cannot reserve an owner key in a later generation.
    // Keep the order and contents of every still-live entry.
    std::erase_if(state.entries,[](const Entry& entry) {
      return entry.scene.stale() || entry.events.stale();
    });
    const auto owner_identity=*registry.resolve(owner,scene);
    if(std::any_of(state.entries.begin(),state.entries.end(),[&](const Entry& entry) {
      return registry.owns(entry.scene) && entry.owner_identity==owner_identity;
    })) return false;
    state.entries.push_back({owner,argument,owner_identity,event,scene,events});
    return true;
  }
  // Remove first, then resolve and dispatch. Failure does not invent a native
  // rollback; the popped entry stays removed under this project policy.
  bool dispatch_first(const Handle& handle, graphics::SceneReferenceRegistry& registry,
                      const SceneEventNames& names,
                      const std::function<void(std::uint64_t,std::uint16_t,std::uint32_t)>& dispatch) {
    auto& state=checked(handle);
    // Retired or destroyed generations cannot be dispatched by any service.
    // Prune only those, never a live foreign scene.
    while(!state.entries.empty() &&
          (state.entries.front().scene.stale() || state.entries.front().events.stale()))
      state.entries.pop_front();
    if(state.entries.empty()) return false;
    if(!dispatch) throw std::runtime_error("deferred start dispatch is unavailable");
    const auto entry=state.entries.front();
    if(!registry.owns(entry.scene) || !names.owns(entry.events))
      throw std::runtime_error("deferred start belongs to a different scene or event table");
    state.entries.pop_front();
    validate(registry,entry.scene,names,entry.events,entry.owner,entry.event,entry.argument);
    const auto owner=registry.resolve(entry.owner,entry.scene);
    if(!owner) throw std::runtime_error("deferred start owner is unavailable");
    dispatch(*owner,entry.event,entry.argument);
    return true;
  }
private:
  State& checked(const Handle& handle) const {
    auto state=handle.state_.lock();
    if(!state || state!=state_) throw std::runtime_error("deferred start queue is unavailable");
    return *state;
  }
  static void validate(graphics::SceneReferenceRegistry& registry,
                       const graphics::SceneReferenceRegistry::Lease& scene,
                       const SceneEventNames& names,const SceneEventNames::Lease& events,
                       std::uint32_t owner,std::uint16_t event,std::uint32_t argument) {
    if(!names.is_live(events) || !names.name(event) || !owner || !registry.resolve(owner,scene))
      throw std::runtime_error("deferred start identity is unavailable");
    if(argument) (void)registry.resolve(argument,scene);
  }
  std::shared_ptr<State> state_;
};
} // namespace off::runtime
