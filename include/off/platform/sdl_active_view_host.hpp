#pragma once

#include "off/graphics/active_view_coordinator.hpp"
#include "off/platform/sdl_intro_renderer.hpp"

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace off::platform {

// Connects the generic admission/lifetime policy to actual SDL renderer views.
// The renderer and GPU device must outlive this host. Construct, operate, and
// destroy it on the device owner thread. This does not select an intro camera,
// run a scene, or prove that a first-cut frame is associated with a view.
class SdlActiveViewHost final {
public:
  using Coordinator = graphics::ActiveViewCoordinator;
  struct VisibleView { std::uint64_t id{}, camera_identity{}; std::uint32_t priority{}; };
  class Traversal final {
  public:
    [[nodiscard]] std::uint64_t identity() const noexcept { return coordinator_.identity(); }
  private:
    friend class SdlActiveViewHost;
    Traversal(std::weak_ptr<const char> owner, Coordinator::Traversal coordinator, SdlIntroRenderer::ViewTraversal renderer)
        : owner_(std::move(owner)), coordinator_(std::move(coordinator)), renderer_(std::move(renderer)) {}
    std::weak_ptr<const char> owner_;
    Coordinator::Traversal coordinator_;
    SdlIntroRenderer::ViewTraversal renderer_;
  };

  explicit SdlActiveViewHost(SdlIntroRenderer& renderer);
  ~SdlActiveViewHost();
  SdlActiveViewHost(const SdlActiveViewHost&) = delete;
  SdlActiveViewHost& operator=(const SdlActiveViewHost&) = delete;

  // Caller must ensure the scene lease owns the actual camera storage and the
  // camera is an aliasing shared_ptr with the same control block. Matching
  // control blocks cannot prove that the camera pointer belongs to that owner.
  [[nodiscard]] bool activate(std::shared_ptr<const void> scene_owner, bool ready);
  void set_ready(bool ready);
  [[nodiscard]] Coordinator::Outcome admit(bool service_present, bool service_ready,
      std::uint64_t camera_identity, std::shared_ptr<const graphics::FreshIntroCamera> camera,
      std::uint32_t priority, Coordinator::Dimensions dimensions);
  [[nodiscard]] std::vector<Coordinator::Deferred> deferred() const;
  [[nodiscard]] std::optional<Coordinator::Grant> issue_grant(std::uint64_t request_id);
  [[nodiscard]] std::vector<Coordinator::Outcome> process_deferred(
      std::span<const Coordinator::Grant> grants, Coordinator::Dimensions dimensions,
      bool service_present, bool service_ready);
  [[nodiscard]] std::optional<Traversal> begin_traversal();
  [[nodiscard]] std::vector<VisibleView> views(const Traversal& traversal) const;
  [[nodiscard]] std::unique_ptr<SdlIntroFrame> prepare(
      const Traversal& traversal, std::uint64_t view_id,
      SDL_GPUCommandBuffer* command, std::span<const SdlIntroDraw> draws) const;
  [[nodiscard]] bool end_traversal(const Traversal& traversal);
  void retire();

private:
  struct CameraLease;
  struct RendererView;
  struct Boundary;
  void require_owner() const;
  SdlIntroRenderer& renderer_;
  const std::shared_ptr<const char> identity_{std::make_shared<const char>(0)};
  const std::thread::id owner_thread_{std::this_thread::get_id()};
  mutable bool busy_{};
  std::shared_ptr<const void> scene_owner_;
  std::shared_ptr<Coordinator::Scene> scene_;
  std::optional<Traversal> traversal_;
  std::uint64_t next_scene_{1};
  Coordinator coordinator_;
};

} // namespace off::platform
