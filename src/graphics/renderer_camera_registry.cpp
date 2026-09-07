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
      if (!services.pending_count || !services.queue_pending)
        throw std::runtime_error("Non-ready state requires bounded pending-camera services");
      if (services.pending_count(*state)>=RendererPendingCameraQueue::capacity)
        throw std::runtime_error("Renderer pending-camera capacity is exhausted");
      services.queue_pending(*state,camera,camera_priority);
      return;
    }
    if (!services.admitted_view_count || !services.allocate_view || !services.associate_camera_intermediate ||
        !services.register_backend_records || !services.insert_view ||
        !services.increment_view_use || !services.renumber_view_ordinals)
      throw std::runtime_error("Ready state requires concrete view allocation services");
    if (services.admitted_view_count(*state)>=RendererPendingCameraQueue::capacity)
      throw std::runtime_error("Renderer admitted-view capacity is exhausted");
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

void RendererPendingCameraQueue::append(RendererViewState state,std::uint64_t camera,
                                        std::int32_t priority) {
  if(busy_ || failed_) throw std::runtime_error("Renderer pending-camera queue is busy or failed");
  if(!state.value || !camera) throw std::runtime_error("Pending renderer camera requires live state and camera");
  if(state_ && state_->value!=state.value)
    throw std::runtime_error("Pending renderer camera queue belongs to another state");
  if(entries_.size()>=capacity) {
    failed_=true;
    throw std::runtime_error("Renderer pending-camera capacity is exhausted");
  }
  state_=state;
  entries_.push_back({camera,priority});
}

void RendererPendingCameraQueue::materialize(RendererViewState state,
                                              const RendererPendingMaterializationServices& supplied) {
  if(busy_ || failed_) throw std::runtime_error("Renderer pending-camera queue is busy or failed");
  if(!state.value || !state_ || state_->value!=state.value || !supplied.initialize_state || !supplied.admit_ready_camera)
    throw std::runtime_error("Pending renderer camera materialization requires its state and services");
  const auto services=supplied;
  Guard guard(busy_);
  try {
    if(!services.initialize_state(state))
      throw std::runtime_error("Renderer state initialization did not establish ready admission");
    // Do not erase incrementally: a failed later admission retains the exact
    // source order, making incomplete materialization visible to the host.
    for(const auto entry:entries_) services.admit_ready_camera(entry.camera,entry.priority);
    entries_.clear();
  } catch(...) {failed_=true;throw;}
}

std::vector<RendererPendingCamera> RendererPendingCameraQueue::entries() const {
  if(busy_) throw std::runtime_error("Renderer pending-camera queue snapshot reentered materialization");
  return entries_;
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

void RendererCameraRegistry::visit_retained(const std::function<void(const RegisteredCamera&)>& supplied) {
  check_idle();
  if(!supplied) throw std::runtime_error("Renderer retained traversal requires a visitor");
  const auto visitor=supplied;
  Guard guard(busy_);
  try {
    // Do not use camera_at here: that indexed query removes stale entries,
    // while renderer initialization is required to leave them retained.
    for(const auto& entry:entries_) visitor(entry);
  } catch(...) {failed_=true;throw;}
}

void RendererRegistryReplay::initialize(RendererCameraRegistry& registry,
                                        const RendererRegistryReplayServices& supplied) {
  if(busy_ || failed_) throw std::runtime_error("Renderer registry replay is busy or failed");
  if(!supplied.setup_renderer || !supplied.backend_ready_before_initialization ||
     !supplied.initialize_backend || !supplied.state_zero || !supplied.resolve_live_camera || !supplied.admit_camera)
    throw std::runtime_error("Renderer registry replay requires explicit initialization services");
  const auto services=supplied;
  Guard guard(busy_);
  try {
    if(!services.setup_renderer()) throw std::runtime_error("Renderer setup did not complete");
    if(services.backend_ready_before_initialization()) return;
    if(!services.initialize_backend()) throw std::runtime_error("Renderer backend initialization did not complete");
    // An existing zero state suppresses this one initialization replay.  A
    // later ready-bit change is not an authorization to retry it.
    if(services.state_zero()) return;
    registry.visit_retained([&](const RegisteredCamera& registered) {
      const auto live=services.resolve_live_camera(registered.owner);
      if(!live) return;
      if(!live->camera || live->camera!=registered.owner)
        throw std::runtime_error("Renderer replay resolved an invalid camera identity");
      services.admit_camera(live->camera,live->priority);
    });
  } catch(...) {failed_=true;throw;}
}
} // namespace off::graphics
