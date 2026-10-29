#include "off/graphics/active_view_coordinator.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace off::graphics {

ActiveViewCoordinator::Boundary::Boundary(ActiveViewCoordinator& value) : host(value) {
    host.boundary_busy_ = true;
}
ActiveViewCoordinator::Boundary::~Boundary() { host.boundary_busy_ = false; }

ActiveViewCoordinator::ActiveViewCoordinator(Ops ops, std::size_t deferred_capacity)
    : ops_(std::move(ops)), device_thread_(std::this_thread::get_id()),
      deferred_capacity_(deferred_capacity) {}

ActiveViewCoordinator::~ActiveViewCoordinator() {
    if (active_) retire(active_);
}

bool ActiveViewCoordinator::on_device_thread() const noexcept {
    return std::this_thread::get_id() == device_thread_;
}

std::shared_ptr<ActiveViewCoordinator::Scene> ActiveViewCoordinator::activate(
    std::uint64_t scene_id, bool ready) {
    if (!on_device_thread() || boundary_busy_ || scene_id == 0) return {};
    Boundary boundary(*this);
    if (active_) retire_impl(active_);
    try {
        active_ = std::shared_ptr<Scene>(new Scene(scene_id, ready));
        return active_;
    } catch (...) { return {}; }
}

void ActiveViewCoordinator::set_ready(const std::shared_ptr<Scene>& scene, bool ready) {
    if (!on_device_thread() || boundary_busy_ || !scene || scene != active_ || scene->retired_) return;
    Boundary boundary(*this);
    scene->ready_ = ready;
}

ActiveViewCoordinator::Outcome ActiveViewCoordinator::admit(
    bool service_present, bool service_ready, const std::shared_ptr<Scene>& scene,
    std::uint64_t camera_identity, std::shared_ptr<const CameraOwner> camera,
    std::uint32_t priority, Dimensions dimensions) {
    if (!service_present) return Outcome::service_unavailable;
    if (!service_ready) return Outcome::service_not_ready;
    if (!on_device_thread() || boundary_busy_) return Outcome::failure;
    Boundary boundary(*this);
    if (!scene || scene != active_ || scene->retired_ || camera_identity == 0 || !camera)
        return Outcome::failure;
    if (!scene->ready_) {
        if (scene->requests_.size() >= deferred_capacity_ ||
            scene->next_request_ == std::numeric_limits<std::uint64_t>::max()) return Outcome::failure;
        const std::uint64_t request_id = scene->next_request_;
        try {
            scene->requests_.push_back(Deferred{request_id, camera_identity,
                                                priority, std::move(camera), std::nullopt});
            ++scene->next_request_;
            return Outcome::deferred;
        } catch (...) { return Outcome::failure; }
    }
    return accept(scene, camera_identity, std::move(camera), priority, dimensions);
}

ActiveViewCoordinator::Outcome ActiveViewCoordinator::accept(
    const std::shared_ptr<Scene>& scene, std::uint64_t camera_identity,
    std::shared_ptr<const CameraOwner> camera, std::uint32_t priority,
    Dimensions dimensions) {
    if (!scene || scene != active_ || scene->retired_ || !scene->ready_ || !camera ||
        camera_identity == 0 || dimensions.width <= 0 || dimensions.height <= 0 ||
        !ops_.create || !ops_.register_view || !ops_.unregister_view || !ops_.release ||
        !ops_.account_use || next_view_ == std::numeric_limits<std::uint64_t>::max() ||
        scene->active_traversals_ != 0) return Outcome::failure;

    View view{next_view_++, camera_identity, priority, std::move(camera), {}};
    // Reserve a scene-owned cleanup slot before the first renderer mutation.
    try { scene->cleanup_.push_back(Scene::OwnedMutation{view, false}); }
    catch (...) { return Outcome::failure; }
    Scene::OwnedMutation& owned = scene->cleanup_.back();
    try {
        owned.view.renderer_resource = ops_.create(*owned.view.camera, dimensions);
        if (!owned.view.renderer_resource) {
            scene->cleanup_.pop_back();
            return Outcome::failure;
        }
        owned.registration_attempted = true; // false return may still have partial effects
        if (!ops_.register_view(owned.view)) return Outcome::failure;
        auto position = std::upper_bound(scene->views_.begin(), scene->views_.end(), priority,
            [](std::uint32_t key, const View& candidate) { return key < candidate.priority; });
        scene->views_.insert(position, owned.view);
        return Outcome::accepted;
    } catch (...) {
        return Outcome::failure; // cleanup record remains owned by the scene
    }
}

std::optional<ActiveViewCoordinator::Grant> ActiveViewCoordinator::issue_grant(
    const std::shared_ptr<Scene>& scene, std::uint64_t request_id) {
    if (!on_device_thread() || boundary_busy_ || !scene || scene != active_ ||
        scene->retired_ || request_id == 0) return std::nullopt;
    Boundary boundary(*this);
    auto it = std::find_if(scene->requests_.begin(), scene->requests_.end(),
        [request_id](const Deferred& request) { return request.id == request_id; });
    if (it == scene->requests_.end() || it->grant || next_grant_ == 0 ||
        next_grant_ == std::numeric_limits<std::uint64_t>::max()) return std::nullopt;
    it->grant = Grant{identity_, scene, it->id, next_grant_++};
    return it->grant;
}

std::vector<ActiveViewCoordinator::Outcome> ActiveViewCoordinator::process_deferred(
    const std::shared_ptr<Scene>& scene, std::span<const Grant> grants,
    Dimensions dimensions, bool service_present, bool service_ready) {
    std::vector<Outcome> result;
    try { result.reserve(std::max<std::size_t>(grants.size(), 1)); }
    catch (...) { return result; }
    if (!service_present) { result.push_back(Outcome::service_unavailable); return result; }
    if (!service_ready) { result.push_back(Outcome::service_not_ready); return result; }
    if (!on_device_thread() || boundary_busy_) { result.push_back(Outcome::failure); return result; }
    Boundary boundary(*this);
    if (!scene || scene != active_ || scene->retired_ || !scene->ready_) {
        result.push_back(Outcome::failure); return result;
    }
    for (const Grant& grant : grants) {
        if (grant.owner_.lock() != identity_ || grant.scene_.lock() != scene || grant.nonce_ == 0) {
            result.push_back(Outcome::failure); break;
        }
        auto request = std::find_if(scene->requests_.begin(), scene->requests_.end(),
            [&](const Deferred& candidate) {
                return candidate.id == grant.request_ && candidate.grant &&
                       candidate.grant->nonce_ == grant.nonce_ && candidate.grant->scene_.lock() == scene &&
                       candidate.grant->owner_.lock() == identity_;
            });
        if (request == scene->requests_.end()) { result.push_back(Outcome::failure); break; }
        const std::uint64_t id = request->id;
        const auto outcome = accept(scene, request->camera_identity, request->camera,
                                    request->priority, dimensions);
        request->grant.reset();
        if (outcome == Outcome::accepted) {
            scene->requests_.erase(std::find_if(scene->requests_.begin(), scene->requests_.end(),
                [id](const Deferred& candidate) { return candidate.id == id; }));
        }
        result.push_back(outcome);
        if (outcome != Outcome::accepted) break;
    }
    return result;
}

std::optional<ActiveViewCoordinator::Traversal> ActiveViewCoordinator::begin_traversal(
    const std::shared_ptr<Scene>& scene) {
    if (!on_device_thread() || boundary_busy_ || !scene || scene != active_ || scene->retired_ ||
        next_traversal_ == std::numeric_limits<std::uint64_t>::max() || !ops_.account_use)
        return std::nullopt;
    Boundary boundary(*this);
    const std::uint64_t identity = next_traversal_++;
    try { traversals_.emplace(identity, scene); }
    catch (...) { return std::nullopt; }
    ++scene->active_traversals_;
    for (const View& view : scene->views_) {
        try {
            if (!ops_.account_use(view, identity)) {
                --scene->active_traversals_;
                traversals_.erase(identity);
                retire_impl(scene);
                return std::nullopt;
            }
        } catch (...) {
            --scene->active_traversals_;
            traversals_.erase(identity);
            retire_impl(scene);
            return std::nullopt;
        }
    }
    return Traversal{scene->id_, identity, scene};
}

std::optional<std::span<const ActiveViewCoordinator::View>>
ActiveViewCoordinator::traversal_views(const Traversal& traversal) const noexcept {
    const auto scene = std::const_pointer_cast<Scene>(traversal.owner_);
    if (!on_device_thread() || boundary_busy_ || !scene || scene->retired_ ||
        scene->id_ != traversal.scene_ || scene != active_) return std::nullopt;
    auto found = traversals_.find(traversal.identity_);
    if (found == traversals_.end() || found->second.lock() != scene) return std::nullopt;
    return std::span<const View>(scene->views_);
}

bool ActiveViewCoordinator::end_traversal(const Traversal& traversal) {
    const auto scene = std::const_pointer_cast<Scene>(traversal.owner_);
    if (!on_device_thread() || boundary_busy_ || !scene || scene->id_ != traversal.scene_ ||
        traversal.identity_ == 0 || scene->active_traversals_ == 0) return false;
    Boundary boundary(*this);
    auto found = traversals_.find(traversal.identity_);
    if (found == traversals_.end() || found->second.lock() != scene) return false;
    traversals_.erase(found);
    --scene->active_traversals_;
    if (scene->retired_ && scene->active_traversals_ == 0) release_scene_resources(scene);
    return true;
}

void ActiveViewCoordinator::retire(const std::shared_ptr<Scene>& scene) {
    if (!on_device_thread() || boundary_busy_ || !scene) return;
    Boundary boundary(*this);
    retire_impl(scene);
}

void ActiveViewCoordinator::retire_impl(const std::shared_ptr<Scene>& scene) noexcept {
    if (!scene || scene->retired_) return;
    scene->retired_ = true;
    scene->ready_ = false;
    scene->requests_.clear();
    if (active_ == scene) active_.reset();
    if (scene->active_traversals_ == 0) release_scene_resources(scene);
}

void ActiveViewCoordinator::release_scene_resources(const std::shared_ptr<Scene>& scene) noexcept {
    for (const auto& owned : scene->cleanup_) {
        if (owned.registration_attempted) {
            try { if (ops_.unregister_view) ops_.unregister_view(owned.view); } catch (...) {}
        }
        try {
            if (ops_.release && owned.view.renderer_resource) ops_.release(owned.view.renderer_resource);
        } catch (...) {}
    }
    scene->cleanup_.clear();
    scene->views_.clear();
}

std::vector<ActiveViewCoordinator::Deferred> ActiveViewCoordinator::deferred(
    const Scene& scene) const {
    if (!on_device_thread() || boundary_busy_ || scene.retired_ || active_.get() != &scene)
        return {};
    try { return scene.requests_; }
    catch (...) { return {}; }
}

} // namespace off::graphics
