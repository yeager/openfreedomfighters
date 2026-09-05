#include "off/graphics/render_preview.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  off::data::TextureImage texture;
  texture.id = 7;
  texture.encoding = off::data::TextureEncoding::abgr32;
  texture.mips.push_back({
      .width = 1,
      .height = 1,
      .encoded =
          {
              std::byte{0x30},
              std::byte{0x20},
              std::byte{0x10},
              std::byte{0xff},
          },
  });

  off::data::PrimitiveEntry primitive;
  primitive.primitive_kind = 0;
  primitive.texture_id = 7;
  primitive.vertices.resize(4);
  primitive.vertices[0].position = {-2.0F, 1.0F, 4.0F};
  primitive.vertices[1].position = {3.0F, -1.0F, 2.0F};
  primitive.vertices[2].position = {1.0F, 5.0F, -3.0F};
  primitive.vertices[3].position = {1000.0F, 1000.0F, 1000.0F};
  for (auto &vertex : primitive.vertices) {
    vertex.color_rgba[3] = 255;
  }
  primitive.batches = {{{0, 1, 2}}};

  const std::array primitives{primitive};
  const std::array textures{texture};
  const auto preview =
      off::graphics::build_render_preview(primitives, textures);
  check(preview.vertices.size() == 4, "copy preview vertices");
  check(preview.indices == std::vector<std::uint16_t>{0, 1, 2},
        "flatten preview indexes");
  check(preview.draws.size() == 1 && preview.draws[0].index_count == 3,
        "preserve preview draw ranges");
  check(preview.texture.width == 1 && preview.texture.height == 1 &&
            preview.texture.pixels ==
                std::vector<std::uint8_t>{0x10, 0x20, 0x30, 0xff},
        "decode preview texture");
  check(preview.minimum_position == std::array{-2.0F, -1.0F, -3.0F} &&
            preview.maximum_position == std::array{3.0F, 5.0F, 4.0F},
        "calculate indexed preview bounds and ignore unused vertices");

  auto degenerate = primitive;
  degenerate.vertices[0].position = {0.0F, 0.0F, 0.0F};
  degenerate.vertices[1].position = {1.0F, 1.0F, 1.0F};
  degenerate.vertices[2].position = {2.0F, 2.0F, 2.0F};
  const std::array candidate_primitives{degenerate, primitive};
  const auto selected =
      off::graphics::build_render_preview(candidate_primitives, textures);
  check(selected.minimum_position == preview.minimum_position,
        "skip degenerate preview candidates");

  auto unsupported = primitive;
  unsupported.texture_id.reset();
  bool unsupported_rejected = false;
  try {
    const std::array unsupported_primitives{unsupported};
    static_cast<void>(
        off::graphics::build_render_preview(unsupported_primitives, textures));
  } catch (const std::runtime_error &) {
    unsupported_rejected = true;
  }
  check(unsupported_rejected, "reject resources without a preview candidate");

  return failures == 0 ? 0 : 1;
}
