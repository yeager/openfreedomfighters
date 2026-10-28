#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace off::graphics {

// A policy and lifetime primitive. It does not create an SDL GPU device,
// window, renderer pass, camera renderer list, or scene inputs. Integrators
// provide the renderer operations and must construct/use the coordinator on
// the SDL device-owning thread. Destroy it on that thread after every Traversal
// is ended.
// View priority is ascending; equal priorities retain admission order.
class ActiveViewCoordinator final {
public:
    struct Dimensions { int width{}; int height{}; };
    struct CameraOwner { virtual ~CameraOwner() = default; };
    struct Resource { virtual ~Resource() = default; };
    struct View final {
        std::uint64_t id{};
        std::uint64_t camera_identity{};
        std::uint32_t priority{};
        std::shared_ptr<const CameraOwner> camera;
        std::shared_ptr<Resource> renderer_resource;
    };
    enum class Outcome { service_unavailable, service_not_ready, deferred, accepted, failure };
    struct Grant final {
        std::uint64_t scene{};
        std::uint64_t request{};
        std::uint64_t nonce{};
    };
    struct Deferred final {
        std::uint64_t id{};
        std::uint64_t camera_identity{};
        std::uint32_t priority{};
        std::shared_ptr<const CameraOwner> camera;
        std::optional<Grant> grant;
    };
    class Scene final {
    public:
        [[nodiscard]] std::uint64_t id() const noexcept { return id_; }
        [[nodiscard]] bool ready() const noexcept { return ready_; }
        [[nodiscard]] bool retired() const noexcept { return retired_; }
    private:
        friend class ActiveViewCoordinator;
        struct OwnedMutation final {
            View view;
            bool registration_attempted{};
        };
        explicit Scene(std::uint64_t id, bool ready) : id_(id), ready_(ready) {}
        std::uint64_t id_{};
        bool ready_{};
        bool retired_{};
        std::uint64_t next_request_{1};
        std::vector<View> views_;
        std::vector<Deferred> requests_;
        std::vector<OwnedMutation> cleanup_;
        std::size_t active_traversals_{};
    };
    struct Ops {
        // Every callback that touches GPU state runs on the construction thread.
        // create must be atomic on failure: returning null or throwing means it
        // acquired no resource. Any returned resource is recorded by the scene
        // before registration is attempted and remains owned until retirement.
        std::function<std::shared_ptr<Resource>(const CameraOwner&, Dimensions)> create;
        std::function<bool(const View&)> register_view;
        std::function<void(const View&)> unregister_view;
        std::function<void(const std::shared_ptr<Resource>&)> release;
        // Invoked once for each view when a traversal begins, with that traversal's identity.
        std::function<bool(const View&, std::uint64_t)> account_use;
    };
    class Traversal final {
    public:
        [[nodiscard]] std::uint64_t scene_id() const noexcept { return scene_; }
        [[nodiscard]] std::uint64_t identity() const noexcept { return identity_; }
    private:
        friend class ActiveViewCoordinator;
        Traversal(std::uint64_t scene, std::uint64_t identity,
                  std::shared_ptr<const Scene> owner)
            : scene_(scene), identity_(identity), owner_(std::move(owner)) {}
        std::uint64_t scene_{};
        std::uint64_t identity_{};
        std::shared_ptr<const Scene> owner_;
    };

    explicit ActiveViewCoordinator(Ops ops, std::size_t deferred_capacity = 64);
    ActiveViewCoordinator(const ActiveViewCoordinator&) = delete;
    ActiveViewCoordinator& operator=(const ActiveViewCoordinator&) = delete;
    ~ActiveViewCoordinator();

    [[nodiscard]] std::shared_ptr<Scene> activate(std::uint64_t scene_id, bool ready);
    void set_ready(const std::shared_ptr<Scene>& scene, bool ready);
    [[nodiscard]] Outcome admit(bool service_present, bool service_ready,
        const std::shared_ptr<Scene>& scene, std::uint64_t camera_identity,
        std::shared_ptr<const CameraOwner> camera, std::uint32_t priority,
        Dimensions dimensions);
    [[nodiscard]] std::optional<Grant> issue_grant(
        const std::shared_ptr<Scene>& scene, std::uint64_t request_id);
    [[nodiscard]] std::vector<Outcome> process_deferred(
        const std::shared_ptr<Scene>& scene, std::span<const Grant> grants,
        Dimensions dimensions, bool service_present, bool service_ready);
    [[nodiscard]] std::optional<Traversal> begin_traversal(
        const std::shared_ptr<Scene>& scene);
    [[nodiscard]] std::optional<std::span<const View>> traversal_views(
        const Traversal& traversal) const noexcept;
    [[nodiscard]] bool end_traversal(const Traversal& traversal);
    void retire(const std::shared_ptr<Scene>& scene);
    // Safe value snapshot; available only on the device thread and outside a
    // mutating boundary. Empty means invalid access or no pending requests.
    [[nodiscard]] std::vector<Deferred> deferred(const Scene& scene) const;

private:
    struct Boundary final { ActiveViewCoordinator& host; explicit Boundary(ActiveViewCoordinator&); ~Boundary(); };
    [[nodiscard]] Outcome accept(const std::shared_ptr<Scene>&, std::uint64_t,
        std::shared_ptr<const CameraOwner>, std::uint32_t, Dimensions);
    [[nodiscard]] bool on_device_thread() const noexcept;
    void retire_impl(const std::shared_ptr<Scene>&) noexcept;
    void release_scene_resources(const std::shared_ptr<Scene>&) noexcept;
    Ops ops_;
    const std::thread::id device_thread_;
    const std::size_t deferred_capacity_;
    std::shared_ptr<Scene> active_;
    bool boundary_busy_{};
    std::uint64_t next_view_{1};
    std::uint64_t next_traversal_{1};
    std::uint64_t next_grant_{1};
    std::unordered_map<std::uint64_t, std::weak_ptr<const Scene>> traversals_;
};

} // namespace off::graphics
