#include "off/graphics/picture_view_parameters.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::graphics {
namespace {
double finite(double value) {
  if (!std::isfinite(value))
    throw std::runtime_error(
        "Picture view parameters require finite arithmetic");
  return value;
}
double divide(double numerator, double denominator) {
  if (denominator == 0)
    throw std::runtime_error("Picture view parameter divisor is zero");
  return finite(numerator / denominator);
}
float resolved(double value) {
  constexpr double limit = std::numeric_limits<float>::max();
  if (finite(value) < -limit || value > limit)
    throw std::runtime_error("Picture view parameter exceeds binary32 range");
  return static_cast<float>(value);
}
std::array<double, 2> dimensions(const PicturePassRectangle &rectangle) {
  for (float value :
       {rectangle.left, rectangle.top, rectangle.right, rectangle.bottom})
    static_cast<void>(finite(value));
  return {double(rectangle.right) - rectangle.left,
          double(rectangle.bottom) - rectangle.top};
}
struct Common {
  double near;
  double far;
  double inverse_ratio;
  double aspect_factor;
};
Common prepare_common(const PictureViewCommonInput &input) {
  for (float value :
       {input.raw_near, input.selected_far, input.renderer_dimension_0,
        input.renderer_dimension_1, input.virtual_aspect})
    static_cast<void>(finite(value));
  auto size = dimensions(input.rectangle);
  for (auto &value : size)
    if (value == 0)
      value = 1;
  const double ratio =
      divide(input.renderer_dimension_0, input.renderer_dimension_1);
  const double viewport_ratio = divide(size[0], size[1]);
  const double aspect =
      finite(double(input.virtual_aspect) * divide(ratio, viewport_ratio));
  return {std::max(double(input.raw_near), 5.0), input.selected_far,
          divide(1.0, ratio), aspect};
}
PictureResolvedView finish(double n, double f, double h0, double h1) {
  const float near = resolved(n);
  const float far = resolved(f);
  const float half0 = resolved(h0);
  const float half1 = resolved(h1);
  return {near, far, half0, half1,
          prepare_picture_projection(near, far, half0, half1)};
}
} // namespace

PictureResolvedView
prepare_picture_view_parameters(const PictureViewCommonInput &common,
                                const PictureOrdinaryCameraInput &camera) {
  static_cast<void>(finite(camera.angle_radians));
  static_cast<void>(finite(camera.renderer_scalar));
  const auto values = prepare_common(common);
  const double tangent = finite(std::tan(double(camera.angle_radians) / 2.0));
  const double half0 = finite(tangent * values.near);
  const double scaled = finite(half0 * values.inverse_ratio);
  const double aspect_scaled = finite(scaled * values.aspect_factor);
  const double half1 = finite(aspect_scaled * camera.renderer_scalar);
  return finish(values.near, values.far, half0, half1);
}

PictureResolvedView
prepare_picture_view_parameters(const PictureViewCommonInput &common,
                                const PictureAlternateCameraInput &camera) {
  static_cast<void>(finite(camera.parameter_0));
  static_cast<void>(finite(camera.parameter_1));
  const auto values = prepare_common(common);
  const double half0 = divide(1.0, finite(2.0 * camera.parameter_0));
  const double denominator =
      finite(finite(2.0 * camera.parameter_1) * values.inverse_ratio);
  const double initial_half1 = divide(1.0, denominator);
  const double scaled = finite(initial_half1 * values.inverse_ratio);
  const double half1 = finite(scaled * values.aspect_factor);
  return finish(values.near, finite(2.0 * values.far), half0, half1);
}

std::array<float, 4>
prepare_picture_viewport_request(const PicturePassRectangle &rectangle,
                                 const std::array<float, 4> &camera_viewport) {
  const auto size = dimensions(rectangle);
  for (float value : camera_viewport)
    static_cast<void>(finite(value));
  const double a = std::max(double(camera_viewport[0]), 0.0);
  const double b = std::max(double(camera_viewport[1]), 0.0);
  const double c = std::min(double(camera_viewport[2]), 1.0);
  const double d = std::min(double(camera_viewport[3]), 1.0);
  const volatile double x_offset = finite(size[0] * a);
  const volatile double y_offset = finite(size[1] * b);
  return {resolved(double(rectangle.left) + x_offset),
          resolved(double(rectangle.top) + y_offset),
          resolved(finite(size[0] * c)), resolved(finite(size[1] * d))};
}

} // namespace off::graphics
