#include "off/graphics/picture_preselection.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace off::graphics {
namespace {
std::uint64_t identity(std::uint64_t value) {
  if (!value) throw std::runtime_error("preselection requires a live nonzero identity");
  return value;
}
void optional_identity(std::optional<std::uint64_t> value) {
  if (value) (void)identity(*value);
}
void context_valid(const PictureSelectionContext* context) {
  if (!context) throw std::runtime_error("preselection owner context is missing");
  (void)identity(context->identity);
  (void)identity(context->backend_record);
}
}
PicturePreselection::PicturePreselection(std::span<PictureSelectionContext* const> registry)
    : registry_(registry.begin(), registry.end()) {
  for (const auto* context : registry_)
    if (!context) throw std::runtime_error("preselection registry has a null context");
}

void PicturePreselection::register_context(PictureSelectionContext& context) {
  if (running_ || poisoned_) throw std::runtime_error("preselection registration requires an idle healthy registry");
  context_valid(&context);
  registry_.push_back(&context);
}

void PicturePreselection::run(std::size_t max_chain_steps, const PicturePreselectionHooks& h,
    const std::function<void(std::uint64_t)>& append) {
  if (running_ || poisoned_) throw std::runtime_error("preselection is active or poisoned");
  if (!max_chain_steps || !append || !h.backend_owner || !h.owner_override ||
      !h.selection_interface || !h.selection_identifier || !h.resolve_selection ||
      !h.view_camera || !h.prepare_camera || !h.relative_point || !h.backend_extension ||
      !h.predicate || !h.next_record || !h.related_resources || !h.resource_owner ||
      !h.owner_capabilities || !h.current_resource || !h.resource_registry_identifier ||
      !h.state_context || !h.next_resource)
    throw std::runtime_error("preselection requires complete services and positive chain bound");
  struct Guard {
    bool& active;
    explicit Guard(bool& value) : active(value) { active = true; }
    ~Guard() { active = false; }
  } guard(running_);
  try {
    std::size_t appended = 0;
    const auto collect = [&](PictureSelectionContext& context) {
      context_valid(&context);
      std::unordered_set<std::uint64_t> seen;
      auto record = context.first_record;
      while (record) {
        identity(*record);
        if (seen.size() == max_chain_steps || !seen.insert(*record).second)
          throw std::runtime_error("preselection record chain is cyclic or exceeds bound");
        if (appended == 8192) throw std::runtime_error("preselection append capacity exceeded");
        append(*record); ++appended;
        record = h.next_record(*record);
      }
    };
    std::optional<std::uint64_t> last_view;
    for (std::size_t index = 0; index < registry_.size();) {
      auto& context = *registry_[index];
      if (!context.active) {
        registry_.erase(registry_.begin() + static_cast<std::ptrdiff_t>(index));
        continue;
      }
      context_valid(&context);
      const auto backend = context.backend_record;
      const auto owner = identity(h.backend_owner(backend));
      const auto participation = context.participation;
      const auto override_byte = h.owner_override(owner);
      if (participation > 1 && override_byte == 0) { ++index; continue; }
      const auto query = identity(h.selection_interface(owner));
      const auto selection_id = h.selection_identifier(owner);
      const auto selection = identity(h.resolve_selection(selection_id));
      const auto view = context.associated_view;
      optional_identity(view);
      if (view != last_view) {
        last_view = view;
        if (!view) throw std::runtime_error("preselection changed to a null camera view");
        h.prepare_camera(identity(h.view_camera(*view)));
      }
      const auto point = h.relative_point(context, backend);
      for (const float component : point)
        if (!std::isfinite(component)) throw std::runtime_error("nonfinite preselection relative point");
      if (override_byte == 0) {
        const auto extension = h.backend_extension(backend);
        optional_identity(extension);
        if (!h.predicate(query, point, extension, selection)) { ++index; continue; }
      }
      collect(context);
      std::array<std::uint64_t, 8> related{};
      const auto count = h.related_resources(backend, related);
      if (count > related.size()) throw std::runtime_error("preselection related query exceeded eight slots");
      for (std::size_t r = 0; r < count; ++r) {
        const auto related_owner = h.resource_owner(identity(related[r]));
        optional_identity(related_owner);
        if (!related_owner || !(h.owner_capabilities(*related_owner) & 0x100000U)) continue;
        auto resource = h.current_resource(*related_owner);
        std::unordered_set<std::uint64_t> seen;
        while (resource) {
          identity(*resource);
          if (seen.size() == max_chain_steps || !seen.insert(*resource).second)
            throw std::runtime_error("preselection resource chain is cyclic or exceeds bound");
          const auto registry_id = h.resource_registry_identifier(*resource);
          if (registry_id) {
            const auto live_owner = h.resource_owner(*resource);
            optional_identity(live_owner);
            if (live_owner && (h.owner_capabilities(*live_owner) & 0x200000U)) {
              auto* selected_context = h.state_context(registry_id);
              context_valid(selected_context);
              collect(*selected_context);
            }
          }
          resource = h.next_resource(*related_owner, *resource);
        }
      }
      ++index;
    }
  } catch (...) {
    poisoned_ = true;
    throw;
  }
}
} // namespace off::graphics
