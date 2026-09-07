#include "off/graphics/renderer_camera_registry.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace off::graphics {
namespace {
struct Guard {
  bool& busy;
  explicit Guard(bool& value):busy(value) {busy=true;}
  ~Guard(){busy=false;}
};
}
void RendererCameraRegistry::check_idle() const {
  if(busy_ || failed_) throw std::runtime_error("Renderer camera registry is busy or failed");
}

void RendererCameraViewAdmission::admit(
    std::uint64_t camera, std::int32_t camera_priority,
    const RendererCameraViewAdmissionServices& supplied) {
  if (busy_ || failed_) throw std::runtime_error("Renderer camera view admission is busy or failed");
  if (!camera || !supplied.renderer_has_backend || !supplied.renderer_backend_ready)
    throw std::runtime_error("Camera view admission requires a live camera and backend gates");
  const auto services = supplied;
  Guard guard(busy_);
  try {
    // Registry membership is retained even when no renderer backend is live.
    // In that case the distinct backend initialization route owns any replay.
    if (!services.renderer_has_backend() || !services.renderer_backend_ready()) return;
    if (!services.state_zero)
      throw std::runtime_error("Ready renderer backend requires state-zero lookup");
    auto state = services.state_zero();
    if (!state) {
      if (!services.application_width || !services.application_height || !services.create_state_zero)
        throw std::runtime_error("Missing state-zero creation service");
      const RendererViewRectangle rectangle{0,0,services.application_width(),services.application_height()};
      state = services.create_state_zero(rectangle);
      if (!state->value) throw std::runtime_error("State-zero creation did not return a live state");
    }
    if (!state->value || !services.state_ready)
      throw std::runtime_error("Camera view admission requires a live state and readiness service");
    if (!services.state_ready(*state)) {
      if (!services.queue_pending) throw std::runtime_error("Non-ready state requires a pending-camera service");
      services.queue_pending(*state,camera);
      return;
    }
    if (!services.allocate_view || !services.associate_camera_intermediate ||
        !services.register_backend_records || !services.insert_view ||
        !services.increment_view_use || !services.renumber_view_ordinals)
      throw std::runtime_error("Ready state requires concrete view allocation services");
    const auto view = services.allocate_view(*state,camera);
    if (!view) throw std::runtime_error("View allocation did not return a live view");
    services.associate_camera_intermediate(view,camera);
    services.register_backend_records(view);
    // Widen before negation so INT32_MIN retains its reviewed signed ordering.
    services.insert_view(view,-static_cast<std::int64_t>(camera_priority));
    services.increment_view_use(view);
    services.renumber_view_ordinals(*state);
  } catch (...) {
    failed_ = true;
    throw;
  }
}

void RendererCameraRegistry::register_camera(std::uint64_t owner,float key,
    const CameraRegistrationServices& supplied) {
  check_idle();
  if(!owner || !std::isfinite(key) || !supplied.live_owner)
    throw std::runtime_error("Camera registration requires a live identity and finite key");
  const auto services=supplied;
  Guard guard(busy_);
  if(!services.live_owner(owner)) throw std::runtime_error("Camera registration owner is not live");
  if(std::any_of(entries_.begin(),entries_.end(),[&](const auto& e){return e.owner==owner;})) return;
  if(!services.notify_dimensions || !services.backend_ready)
    throw std::runtime_error("Missing renderer camera registration service");
  auto candidate=last_inserted_?*last_inserted_:entries_.begin();
  if(last_inserted_) {
    while(candidate->key>key && candidate!=entries_.begin()) --candidate;
  }
  while(candidate!=entries_.end() && key>candidate->key) ++candidate;
  const auto inserted=entries_.insert(candidate,{owner,key});
  last_inserted_=inserted;
  try {
    services.notify_dimensions(owner);
    if(services.backend_ready()) {
      if(!services.admit_view) throw std::runtime_error("Ready camera backend has no view-admission service");
      services.admit_view(owner);
    }
  } catch(...) {failed_=true;throw;}
}
std::uint64_t RendererCameraRegistry::camera_at(std::size_t index,
    const std::function<bool(std::uint64_t)>& supplied) {
  check_idle();
  if(!supplied) throw std::runtime_error("Camera query requires live scene owner lookup");
  const auto live=supplied;
  Guard guard(busy_);
  try {
    for(auto at=entries_.begin();at!=entries_.end();) {
      if(live(at->owner)) {++at;continue;}
      if(last_inserted_ && *last_inserted_==at) last_inserted_.reset();
      at=entries_.erase(at);
    }
    if(index>=entries_.size()) return 0;
    auto at=entries_.begin();std::advance(at,static_cast<std::ptrdiff_t>(index));
    return at->owner;
  } catch(...) {failed_=true;throw;}
}
std::vector<RegisteredCamera> RendererCameraRegistry::entries() const {
  if(busy_) throw std::runtime_error("Camera snapshot cannot reenter registry mutation");
  return {entries_.begin(),entries_.end()};
}
} // namespace off::graphics
