#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace off::runtime {

// Owned by the scene manager, not reset by loading another source archive.
class SceneComponentSequence final {
public:
  SceneComponentSequence() = default;
  SceneComponentSequence(const SceneComponentSequence&) = delete;
  SceneComponentSequence& operator=(const SceneComponentSequence&) = delete;
  [[nodiscard]] std::uint64_t next_identity() const noexcept { return next_; }
  [[nodiscard]] std::uint64_t live_count() const noexcept { return live_; }
  void set_construction_mode(bool value) noexcept { construction_mode_ = value; }
private:
  friend class ComponentLifecycle;
  std::uint64_t next_{};
  std::uint64_t live_{};
  bool construction_mode_{};
  bool busy_{};
};

struct ComponentSource {
  std::uint64_t owner;
  std::optional<std::size_t> directory_index;
  std::optional<std::size_t> attachment_index;
  std::string factory_name;
  std::uint32_t identifier_offset;
  std::uint32_t owner_reader_offset;
  float authored_parameter;
  bool synthesized;
};

struct ComponentState {
  // Supplied by a completed concrete factory/attachment route, not inferred
  // from source flags, authored parameters or the mere presence of an entry.
  std::uint16_t class_ordinal;
  std::uint32_t priority;
  std::uint32_t requested;
  std::uint32_t admitted;
  std::uint32_t registered_cache;
  std::uint32_t status;
  std::uint32_t script_reference;
  std::uint64_t attached_owner;
};

class ComponentRecord;
using ComponentCallback = std::function<void(ComponentRecord&)>;
struct ConstructedComponent {
  ComponentState state;
  // Callbacks retain the concrete component payload through their captures.
  // Missing callbacks are errors when reached, never successful empty bodies.
  ComponentCallback phase_one;
  ComponentCallback phase_two;
};

class ComponentRecord final {
public:
  [[nodiscard]] const ComponentSource& source() const noexcept { return source_; }
  [[nodiscard]] std::optional<std::uint32_t> identity() const noexcept { return identity_; }
  [[nodiscard]] bool constructed() const noexcept { return constructed_; }
  [[nodiscard]] bool removed() const noexcept { return removed_; }
  [[nodiscard]] ComponentState& state();
  [[nodiscard]] const ComponentState& state() const;
private:
  friend class ComponentLifecycle;
  explicit ComponentRecord(ComponentSource source) : source_(std::move(source)) {}
  ComponentSource source_;
  std::optional<std::uint32_t> identity_;
  std::optional<ConstructedComponent> instance_;
  bool constructed_{}, removed_{};
};

struct ComponentLifecycleServices {
  // Called for a phase-bit entry before reading its live attached owner.
  // Third argument counts surviving nodes visited across both passes, including
  // nodes without this phase bit. Caller owns loader offsets/denominator.
  std::function<void(bool, ComponentRecord&, std::size_t)> progress;
  // Pure native lookup: must not mutate engine/component state or emit events.
  // A disengaged result means no live attached owner. No script-ref fallback.
  // Also validates bypassed owners; that validation is not an original callback.
  std::function<std::optional<std::uint32_t>(std::uint64_t)> owner_flags;
  std::function<void(std::uint64_t)> post_phase_one;
  // Required concrete cleanup/destruction route; metadata storage stays stable.
  // Must perform cleanup, live-owner reread, status0x100 and actual removal;
  // some concrete destruction routes call cleanup again. Native tombstoning
  // happens only AFTER this operation succeeds, and does not replace it.
  ComponentCallback retire;
};

// Retained construction list shared by both global passes. Catalog insertion
// does not construct components, assign runtime identities or grant admission.
// Pre/post loader hooks, deferred source setup and ordinary dispatch are separate.
class ComponentLifecycle final {
public:
  explicit ComponentLifecycle(SceneComponentSequence& sequence) : sequence_(sequence) {}
  ~ComponentLifecycle();
  ComponentLifecycle(const ComponentLifecycle&) = delete;
  ComponentLifecycle& operator=(const ComponentLifecycle&) = delete;
  std::size_t append(ComponentSource source);
  [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }
  [[nodiscard]] ComponentRecord& at(std::size_t index) { return *records_.at(index); }
  [[nodiscard]] const ComponentRecord& at(std::size_t index) const { return *records_.at(index); }
  // Historical order includes removed entries; passes visit surviving entries.
  [[nodiscard]] std::span<const std::size_t> construction_order() const noexcept { return order_; }
  using Factory = std::function<ConstructedComponent(ComponentRecord&)>;
  // Registers the common-base identity before invoking the concrete factory.
  // The factory owes actual class setup, owner wiring and event enrollment.
  void construct(std::size_t index, const Factory& factory);
  void run_global_phases(const ComponentLifecycleServices& services);
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  [[nodiscard]] bool phases_completed() const noexcept { return completed_; }
private:
  void check_idle() const;
  void pass(bool second, const ComponentLifecycleServices& services, std::size_t& visited);
  SceneComponentSequence& sequence_;
  std::vector<std::unique_ptr<ComponentRecord>> records_;
  std::vector<std::size_t> order_;
  bool busy_{}, failed_{}, completed_{};
};
} // namespace off::runtime
