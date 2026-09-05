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
  std::array<float, 16> matrix_{};
};

struct PictureViewport {
  std::uint32_t x;
  std::uint32_t y;
  std::uint32_t width;
  std::uint32_t height;
};

// Conditional ordinary projection, with explicitly resolved camera inputs.
// Replacement safety policy: finite n/f/h0/h1, n>=5, f>n, signed nonzero
// half-extents. No near clamp, camera defaults, or half-extent derivation.
// Double intermediates are checked before binary32 coefficient conversion;
// this is a finite portable contract, not bit-exact retail arithmetic.
[[nodiscard]] PictureProjection prepare_picture_projection(float near_distance,
                                                           float far_distance,
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
