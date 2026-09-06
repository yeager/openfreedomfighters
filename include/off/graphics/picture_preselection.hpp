#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

// Live caller-owned owner context, not a picture submission or resource parser.
// Nonoptional identity tokens must be nonzero. Objects and chains stay alive;
// Identity/backend associations stay fixed through a call. Callbacks may update
// activity, participation, override and view association at their live read
// points, not destroy objects or restructure record/resource chains.
struct PictureSelectionContext {
  std::uint64_t identity;
  bool active;
  std::uint64_t backend_record;
  std::uint8_t participation;
  std::optional<std::uint64_t> associated_view;
  std::optional<std::uint64_t> first_record;
};

struct PicturePreselectionHooks {
  using Identity = std::uint64_t;
  using Optional = std::optional<Identity>;
  using Point = std::array<float, 3>;
  std::function<Identity(Identity)> backend_owner;
  std::function<std::uint8_t(Identity)> owner_override;
  std::function<Identity(Identity)> selection_interface;
  std::function<Identity(Identity)> selection_identifier;
  std::function<Identity(Identity)> resolve_selection;
  std::function<Identity(Identity)> view_camera;
  std::function<void(Identity)> prepare_camera;
  // Exact camera-relative input producer remains required, even under override.
  std::function<Point(const PictureSelectionContext&, Identity)> relative_point;
  std::function<Optional(Identity)> backend_extension;
  std::function<bool(Identity, const Point&, Optional, Identity)> predicate;
  std::function<Optional(Identity)> next_record;
  // Fill no more than the supplied eight slots; return count. Values outside
  // count are ignored. A returned null resource is rejected as native policy.
  std::function<std::size_t(Identity, std::span<Identity, 8>)> related_resources;
  std::function<Optional(Identity)> resource_owner;
  std::function<std::uint32_t(Identity)> owner_capabilities;
  std::function<Optional(Identity)> current_resource;
  std::function<std::uint64_t(Identity)> resource_registry_identifier;
  // Must validate registry ownership and stale/mismatched mappings. The registry
  // identifier is not necessarily equal to the context's identity token.
  std::function<PictureSelectionContext*(std::uint64_t)> state_context;
  // Receiver stays the initially resolved related owner, not resource owner.
  std::function<Optional(Identity, Identity)> next_resource;
};

class PicturePreselection final {
public:
  // Supplied membership must come from actual owner activation/extension query;
  // passing no entries is not evidence that the original intro registry is empty.
  explicit PicturePreselection(std::span<PictureSelectionContext* const> registry);
  PicturePreselection(const PicturePreselection&) = delete;
  PicturePreselection& operator=(const PicturePreselection&) = delete;
  PicturePreselection(PicturePreselection&&) = delete;
  PicturePreselection& operator=(PicturePreselection&&) = delete;
  // Caller invokes only after its actual activation membership query succeeds.
  // Append preserves duplicates; it never guesses extension availability.
  // Allowed between invocations only, on a nonpoisoned registry.
  void register_context(PictureSelectionContext& context);
  [[nodiscard]] std::size_t registry_size() const noexcept { return registry_.size(); }
  [[nodiscard]] bool poisoned() const noexcept { return poisoned_; }
  // Appends without clearing the caller's list. Connect directly to the ordered
  // coordinator's bounded Select visitor. This service never marks keys/cursors.
  // All hooks, positive chain bound and append are prevalidated, even if empty.
  // Native rules: finite points; valid nonzero tokens; no concurrent access or
  // reentry; stable object/chain lifetimes; max 8192 append calls per invocation.
  // Exceptions after entry poison and retain inactive removals/appended prefix.
  // No retry, rollback or completion cleanup is synthesized.
  void run(std::size_t max_chain_steps, const PicturePreselectionHooks& hooks,
           const std::function<void(std::uint64_t)>& append);
private:
  std::vector<PictureSelectionContext*> registry_;
  bool running_{false}, poisoned_{false};
};
} // namespace off::graphics
