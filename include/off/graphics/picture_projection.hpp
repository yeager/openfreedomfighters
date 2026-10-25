#pragma once

#include <array>
#include <cstdint>

namespace off::graphics {

class PictureProjection final {
public:
  [[nodiscard]] const std::array<float, 16> &matrix() const noexcept {
    return matrix_;
  }

private:
  PictureProjection() = default;
  friend PictureProjection prepare_picture_projection(float, float, float,
                                                      float);
  friend PictureProjection prepare_picture_projection_alternate_depth(
      float, float, float);
  std::array<float, 16> matrix_{};
};

struct PictureViewport {
  std::uint32_t x;
  std::uint32_t y;
  std::uint32_t width;
  std::uint32_t height;
};

// Raw setter conversion, not viewport validation: finite binary32 components
// in [-2^63,2^63) truncate to int64, then retain their low uint32 bits.
// Out-of-domain values throw instead of emulating retail math-error handlers.
// Does not emulate floating-point status/traps or derive camera requests.
[[nodiscard]] PictureViewport
convert_picture_viewport_request(const std::array<float, 4> &request);

// Conditional ordinary projection, with explicitly resolved camera inputs.
// Replacement safety policy: finite n/f/h0/h1, n>=5, f>n, signed nonzero
// half-extents. No near clamp, camera defaults, or half-extent derivation.
// Nearest-even independently rounded binary32 sums/products/divisions, including
// doubled half-extents and computed offsets. No contraction or cancellation.
// Finite intermediates are required; tangent derivation is a separate contract.
[[nodiscard]] PictureProjection prepare_picture_projection(float near_distance,
                                                           float far_distance,
                                                           float half_extent_0,
                                                           float half_extent_1);

// Recovered alternate camera-view depth block. It preserves the same
// perspective X/Y scales and clip W=Z while mapping depth as Z-near. This is
// a separate source-free matrix variant, not a selectable runtime mode.
[[nodiscard]] PictureProjection
prepare_picture_projection_alternate_depth(float near_distance,
                                           float half_extent_0,
                                           float half_extent_1);

// WORLD/VIEW are identity here: input is already expanded picture XYZ. Do not
// apply the picture basis a second time. Clip W equals input Z.
[[nodiscard]] std::array<float, 4>
project_picture_position(const PictureProjection &projection,
                         const std::array<float, 3> &position);

// Fixed viewport depth range [0,1]. Positive extents and uint64-computed
// right/bottom <= UINT32_MAX are replacement safety policy. Rejects zero W,
// but allows negative W algebraically: this is NOT a visibility/clipping test.
// Clip and screen outputs use checked double-to-binary32 arithmetic, with no
// viewport integer conversion or rasterizer half-pixel correction inferred.
// All invalid inputs or non-finite/unrepresentable outputs throw runtime_error.
[[nodiscard]] std::array<float, 3>
map_picture_clip_to_viewport(const std::array<float, 4> &clip,
                             const PictureViewport &viewport);

} // namespace off::graphics
