#include "off/platform/sdl_active_view_host.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace off::platform {

struct SdlActiveViewHost::CameraLease final : Coordinator::CameraOwner {
  explicit CameraLease(std::shared_ptr<const graphics::FreshIntroCamera> camera)
      : camera(std::move(camera)) {}
  std::shared_ptr<const graphics::FreshIntroCamera> camera;
};
struct SdlActiveViewHost::RendererView final : Coordinator::Resource {
  SdlIntroRenderer::RegisteredView view;
  bool registered{};
  std::uint64_t last_traversal{};
  std::uint64_t use_count{};
};
struct SdlActiveViewHost::Boundary final {
  explicit Boundary(bool& busy) : busy(busy) { busy=true; }
  ~Boundary() { busy=false; }
  bool& busy;
};

SdlActiveViewHost::SdlActiveViewHost(SdlIntroRenderer& renderer)
    : renderer_(renderer), coordinator_(Coordinator::Ops{
        [](const Coordinator::CameraOwner& owner, Coordinator::Dimensions) -> std::shared_ptr<Coordinator::Resource> {
          if (!dynamic_cast<const CameraLease*>(&owner)) return {};
          return std::make_shared<RendererView>();
        },
        [this](const Coordinator::View& view) {
          const auto camera=std::dynamic_pointer_cast<const CameraLease>(view.camera);
          const auto resource=std::dynamic_pointer_cast<RendererView>(view.renderer_resource);
          if (!scene_ || !camera || !camera->camera || !resource || resource->registered) return false;
          resource->view=renderer_.register_view(scene_->id(),camera->camera);
          resource->registered=true;
          return true;
        },
        [this](const Coordinator::View& view) {
          const auto resource=std::dynamic_pointer_cast<RendererView>(view.renderer_resource);
          if (resource && resource->registered) {
            renderer_.retire_view(resource->view);
            resource->registered=false;
          }
        },
        [](const std::shared_ptr<Coordinator::Resource>&) {},
        [](const Coordinator::View& view,std::uint64_t traversal) {
          const auto resource=std::dynamic_pointer_cast<RendererView>(view.renderer_resource);
          if (!resource || !resource->registered || !traversal || resource->last_traversal==traversal ||
              resource->use_count==std::numeric_limits<std::uint64_t>::max()) return false;
          resource->last_traversal=traversal;
          ++resource->use_count;
          return true;
        }}) {
  // Fail at construction if the caller is not the renderer's owner thread.
  (void)renderer_.view_traversal_active();
}

SdlActiveViewHost::~SdlActiveViewHost() {
  busy_=true;
  try {
    if (traversal_) {
      renderer_.end_view_traversal(traversal_->renderer_);
      (void)coordinator_.end_traversal(traversal_->coordinator_);
      traversal_.reset();
    }
    if (scene_) coordinator_.retire(scene_);
    scene_.reset();
    scene_owner_.reset();
  } catch (...) { /* Caller must destroy on the renderer owner thread. */ }
}
void SdlActiveViewHost::require_owner() const {
  if (std::this_thread::get_id()!=owner_thread_ || busy_)
    throw std::runtime_error("active view host requires its owner thread and a non-reentrant call");
}
bool SdlActiveViewHost::activate(std::shared_ptr<const void> owner,bool ready) {
  require_owner(); Boundary boundary(busy_);
  if (!owner || traversal_ || renderer_.view_traversal_active() || next_scene_==std::numeric_limits<std::uint64_t>::max()) return false;
  if (scene_) coordinator_.retire(scene_);
  scene_.reset(); scene_owner_.reset();
  auto scene=coordinator_.activate(next_scene_,ready);
  if (!scene) return false;
  ++next_scene_; scene_owner_=std::move(owner); scene_=std::move(scene);
  return true;
}
void SdlActiveViewHost::set_ready(bool ready) {
  require_owner(); Boundary boundary(busy_);
  if (!scene_) throw std::runtime_error("active view host has no scene");
  coordinator_.set_ready(scene_,ready);
}
SdlActiveViewHost::Coordinator::Outcome SdlActiveViewHost::admit(
    bool present,bool ready,std::uint64_t identity,
    std::shared_ptr<const graphics::FreshIntroCamera> camera,std::uint32_t priority,
    Coordinator::Dimensions dimensions) {
  require_owner(); Boundary boundary(busy_);
  if (!present) return Coordinator::Outcome::service_unavailable;
  if (!ready) return Coordinator::Outcome::service_not_ready;
  if (renderer_.view_traversal_active()) return Coordinator::Outcome::failure;
  if (!scene_ || !scene_owner_ || !camera || scene_owner_.owner_before(camera) || camera.owner_before(scene_owner_))
    return Coordinator::Outcome::failure;
  try {
    return coordinator_.admit(true,true,scene_,identity,std::make_shared<CameraLease>(std::move(camera)),priority,dimensions);
  } catch (...) { return Coordinator::Outcome::failure; }
}
std::vector<SdlActiveViewHost::Coordinator::Deferred> SdlActiveViewHost::deferred() const {
  require_owner(); Boundary boundary(busy_);
  return scene_ ? coordinator_.deferred(*scene_) : std::vector<Coordinator::Deferred>{};
}
std::optional<SdlActiveViewHost::Coordinator::Grant> SdlActiveViewHost::issue_grant(std::uint64_t id) {
  require_owner(); Boundary boundary(busy_);
  return scene_ ? coordinator_.issue_grant(scene_,id) : std::nullopt;
}
std::vector<SdlActiveViewHost::Coordinator::Outcome> SdlActiveViewHost::process_deferred(
    std::span<const Coordinator::Grant> grants,Coordinator::Dimensions dimensions,bool present,bool ready) {
  require_owner(); Boundary boundary(busy_);
  if (present && ready && renderer_.view_traversal_active()) return {Coordinator::Outcome::failure};
  return coordinator_.process_deferred(scene_,grants,dimensions,present,ready);
}
std::optional<SdlActiveViewHost::Traversal> SdlActiveViewHost::begin_traversal() {
  require_owner(); Boundary boundary(busy_);
  if (!scene_ || traversal_ || renderer_.view_traversal_active()) return std::nullopt;
  auto coordinator=coordinator_.begin_traversal(scene_);
  if (!coordinator) return std::nullopt;
  try {
    auto renderer=renderer_.begin_view_traversal(scene_->id());
    traversal_=Traversal{identity_,std::move(*coordinator),std::move(renderer)};
  } catch (...) {
    // Accounting has happened. Retire this scene instead of exposing or
    // retrying a traversal for which the renderer did not grant a token.
    coordinator_.retire(scene_);
    (void)coordinator_.end_traversal(*coordinator);
    throw;
  }
  return traversal_;
}
std::vector<SdlActiveViewHost::VisibleView> SdlActiveViewHost::views(const Traversal& traversal) const {
  require_owner(); Boundary boundary(busy_);
  if (traversal.owner_.lock()!=identity_ || !traversal_ || traversal_->identity()!=traversal.identity()) return {};
  const auto views=coordinator_.traversal_views(traversal.coordinator_);
  if (!views) return {};
  std::vector<VisibleView> result; result.reserve(views->size());
  for (const auto& view:*views) result.push_back({view.id,view.camera_identity,view.priority});
  return result;
}
std::unique_ptr<SdlIntroFrame> SdlActiveViewHost::prepare(
    const Traversal& traversal,std::uint64_t view_id,SDL_GPUCommandBuffer* command,
    std::span<const SdlIntroDraw> draws) const {
  require_owner(); Boundary boundary(busy_);
  if (traversal.owner_.lock()!=identity_ || !traversal_ || traversal_->identity()!=traversal.identity()) throw std::runtime_error("active view traversal is stale");
  const auto views=coordinator_.traversal_views(traversal.coordinator_);
  if (!views) throw std::runtime_error("active view scene is retired");
  for (const auto& view:*views) if(view.id==view_id) {
    const auto resource=std::dynamic_pointer_cast<RendererView>(view.renderer_resource);
    if (!resource || !resource->registered) break;
    return renderer_.prepare_view(command,draws,resource->view,traversal.renderer_);
  }
  throw std::runtime_error("active view is not in this traversal");
}
bool SdlActiveViewHost::end_traversal(const Traversal& traversal) {
  require_owner(); Boundary boundary(busy_);
  if (traversal.owner_.lock()!=identity_ || !traversal_ || traversal_->identity()!=traversal.identity()) return false;
  renderer_.end_view_traversal(traversal_->renderer_);
  const bool ended=coordinator_.end_traversal(traversal_->coordinator_);
  traversal_.reset();
  return ended;
}
void SdlActiveViewHost::retire() {
  require_owner(); Boundary boundary(busy_);
  if (scene_) coordinator_.retire(scene_);
  scene_.reset(); scene_owner_.reset();
}
} // namespace off::platform
