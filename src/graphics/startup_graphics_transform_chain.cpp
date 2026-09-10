#include "off/graphics/startup_graphics_transform_chain.hpp"

#include <cmath>
#include <stdexcept>

namespace off::graphics {

PictureCacheTransform compose_startup_graphics_transform_chain(
    std::span<const data::StartupGraphicsLocalTransform> chain) {
  if (chain.empty())
    throw std::runtime_error("startup graphics transform chain is empty");
  PictureCacheTransform result{.basis = {1, 0, 0, 0, 1, 0, 0, 0, 1},
                               .translation = {0, 0, 0}};
  for (const auto &local : chain) {
    for (const float value : local.basis)
      if (!std::isfinite(value))
        throw std::runtime_error("startup graphics transform basis is non-finite");
    for (const float value : local.position)
      if (!std::isfinite(value))
        throw std::runtime_error("startup graphics transform position is non-finite");
    std::array<float, 9> basis{};
    std::array<float, 3> translation{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = 0; column < 3; ++column) {
        float value{};
        for (std::size_t inner = 0; inner < 3; ++inner)
          value += result.basis[row * 3 + inner] *
                   local.basis[inner * 3 + column];
        basis[row * 3 + column] = value;
      }
      translation[row] = result.basis[row * 3] * local.position[0] +
                         result.basis[row * 3 + 1] * local.position[1] +
                         result.basis[row * 3 + 2] * local.position[2] +
                         result.translation[row];
    }
    result.basis = basis;
    result.translation = translation;
  }
  for (const float value : result.basis)
    if (!std::isfinite(value))
      throw std::runtime_error("startup graphics composed basis is non-finite");
  for (const float value : result.translation)
    if (!std::isfinite(value))
      throw std::runtime_error("startup graphics composed translation is non-finite");
  return result;
}

} // namespace off::graphics
