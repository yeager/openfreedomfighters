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
  primitive.packed_index = 0x8000002aU;
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
  check(preview.primitive_packed_index == 0x8000002aU &&
            !preview.object_instance.has_value(),
        "preserve preview primitive identity before instance binding");

  off::data::GmsDirectoryEntry object_source;
  object_source.primitive_reference = 0x8000002aU;
  object_source.source_type = 0x00200002U;
  object_source.local_slot_index = 9;
  object_source.basis = {0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
  object_source.position = {10.0F, 20.0F, 30.0F};
  auto duplicate_source = object_source;
  duplicate_source.position = {40.0F, 50.0F, 60.0F};
  const std::array object_sources{object_source, duplicate_source};
  const auto instanced = off::graphics::bind_first_render_preview_instance(
      preview, object_sources);
  check(instanced.object_instance.has_value() &&
            instanced.object_instance->basis == object_source.basis &&
            instanced.object_instance->position == object_source.position &&
            instanced.object_instance->source_type ==
                object_source.source_type &&
            instanced.object_instance->directory_index == 0 &&
            instanced.object_instance->local_slot_index == 9,
        "bind the first exact GMS object-source match and its identity");
  check(off::graphics::transform_render_position(*instanced.object_instance,
                                                 {2.0F, 3.0F, 4.0F}) ==
            std::array{13.0F, 18.0F, 34.0F},
        "apply the GMS basis as rows before adding position");

  bool missing_instance_rejected = false;
  try {
    const std::array<off::data::GmsDirectoryEntry, 0> no_objects{};
    static_cast<void>(
        off::graphics::bind_first_render_preview_instance(preview, no_objects));
  } catch (const std::runtime_error &) {
    missing_instance_rejected = true;
  }
  check(missing_instance_rejected,
        "reject preview primitives without a GMS object instance");

  auto malformed = primitive;
  malformed.batches = {{{0, 1, 4}}};
  bool malformed_rejected = false;
  try {
    const std::array malformed_primitives{malformed};
    static_cast<void>(
        off::graphics::build_render_preview(malformed_primitives, textures));
  } catch (const std::runtime_error &) {
    malformed_rejected = true;
  }
  check(malformed_rejected, "reject out-of-range preview vertex indexes");

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
