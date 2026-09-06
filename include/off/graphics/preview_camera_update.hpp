#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace off::graphics {
class FreshIntroCamera;
// Borrowed live resource state, not a constructor or listener snapshot.
struct PreviewCameraPose {
  std::array<float,9> basis;
  std::array<float,3> position;
  // Resource hide/transform-dirty domain, NOT camera-owner enabled/toggle flags.
  std::uint32_t resource_flags;
};
struct PreviewCameraInput {
  std::array<float,2> pointer;
  float raw_crt_delta;
  // Actual application queries, NOT SDL scancodes. Ordered identities (hex):
  // held: 59,12,11,10,25,26,27,28; edges: 51,57,45,52.
  std::array<bool,8> held;
  std::array<std::uint32_t,4> edges;
  bool collision_visualization;
  float last_scaled_increment{};
};

// Application-lifetime instance shared by preview cameras and scene loads.
// Explicit admitted keyboard/pointer update; no synthetic input or camera creation.
// Held/event arrays represent a stable real frame snapshot. Mutating input-query
// callbacks require a separate adapter preserving the reviewed query order.
class PreviewCameraUpdate final {
public:
  PreviewCameraUpdate() = default;
  PreviewCameraUpdate(const PreviewCameraUpdate&) = delete;
  PreviewCameraUpdate& operator=(const PreviewCameraUpdate&) = delete;
  // Retained rotation and translation speed settings, respectively.
  float movement_scale{3.0F}, secondary_scale{1.5F};
  bool toggle_latch{};
  [[nodiscard]] const std::optional<std::array<float,2>>& previous_pointer() const noexcept { return previous_; }
  // Real resource and queue service require stable lifetimes. Unsupported
  // collision services/missing queue/nonfinite inputs reject before writes. Later
  // arithmetic/queue errors preserve completed effects; abort that frame.
  // Translation uses the explicit ordered extended-precision helper, then
  // binary32 coordinate storage. Rotation has separate binary32 steps and
  // native transcendental math remains an interoperability policy.
  void run(FreshIntroCamera& owner,PreviewCameraPose& camera, const PreviewCameraInput& input,
           const std::function<void(PreviewCameraPose&)>& enqueue_transform);
private:
  std::optional<std::array<float,2>> previous_;
  bool busy_{};
};
} // namespace off::graphics
