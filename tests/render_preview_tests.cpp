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
  primitive.packed_index = 42;
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
  check(preview.primitive_packed_index == 42 &&
            !preview.object_instance.has_value(),
        "preserve preview primitive identity before instance binding");

  off::data::GmsDirectoryEntry object_source;
  object_source.primitive_reference = 42;
  object_source.source_type = 0x00200002U;
  object_source.local_slot_index = 9;
  object_source.basis = {0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
  object_source.position = {10.0F, 20.0F, 30.0F};
  auto earlier_source = object_source;
  earlier_source.local_slot_index = 1;
  earlier_source.position = {40.0F, 50.0F, 60.0F};
  const std::array object_sources{earlier_source, object_source};
  off::data::RenderMapEntry map_entry;
  map_entry.descriptor_offset = 144;
  map_entry.object.primary_geometry_reference = 0x400003f0U;
  map_entry.object.secondary_geometry_reference = 0x40000070U;
  map_entry.object.orientation = {1.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                  0.0F, 0.0F, 0.0F, 1.0F};
  map_entry.object.position = {100.0F, 200.0F, 300.0F};
  const std::array map_entries{map_entry};
  const auto resolutions = off::graphics::resolve_scene_geometry_references(
      primitives, object_sources, map_entries);
  check(resolutions.size() == 2 &&
            resolutions[0].role == off::graphics::SceneGeometryRole::primary &&
            resolutions[0].status ==
                off::graphics::SceneGeometryStatus::local_primitive &&
            resolutions[0].source_directory_index == 1 &&
            resolutions[0].requested_handle_slot_index == 9 &&
            resolutions[0].primitive_reference == 42 &&
            resolutions[0].primitive_entry_index == 0 &&
            resolutions[1].role ==
                off::graphics::SceneGeometryRole::secondary &&
            resolutions[1].status ==
                off::graphics::SceneGeometryStatus::local_primitive &&
            resolutions[1].source_directory_index == 0 &&
            resolutions[1].requested_handle_slot_index == 1,
        "resolve primary and additional secondary handles in stable order");
  const auto instanced =
      off::graphics::build_first_primary_scene_render_preview(
          primitives, textures, object_sources, map_entries);
  check(
      instanced.object_instance.has_value() &&
          instanced.object_instance->basis == object_source.basis &&
          instanced.object_instance->position == object_source.position &&
          instanced.object_instance->source_type == object_source.source_type &&
          instanced.object_instance->directory_index == 1 &&
          instanced.object_instance->local_slot_index == 9 &&
          instanced.object_instance->map_instance.has_value() &&
          instanced.object_instance->map_instance->map_entry_index == 0 &&
          instanced.object_instance->map_instance->map_descriptor_offset ==
              144 &&
          instanced.object_instance->map_instance->geometry_reference ==
              0x400003f0U &&
          instanced.object_instance->map_instance->orientation ==
              map_entry.object.orientation &&
          instanced.object_instance->map_instance->position ==
              map_entry.object.position,
      "resolve the scene handle to its exact GMS source and retain identities");
  check(off::graphics::transform_render_position(*instanced.object_instance,
                                                 {2.0F, 3.0F, 4.0F}) ==
            std::array{13.0F, 18.0F, 34.0F},
        "apply the GMS basis as rows before adding position");

  auto slot_zero_source = object_source;
  slot_zero_source.local_slot_index = 0;
  auto slot_zero_entry = map_entry;
  slot_zero_entry.object.primary_geometry_reference = 0x40000000U;
  const std::array slot_zero_sources{slot_zero_source};
  const std::array slot_zero_entries{slot_zero_entry};
  const auto slot_zero_preview =
      off::graphics::build_first_primary_scene_render_preview(
          primitives, textures, slot_zero_sources, slot_zero_entries);
  check(slot_zero_preview.object_instance->local_slot_index == 0,
        "resolve tagged runtime slot zero as a present object");
  const std::array<off::data::GmsDirectoryEntry, 0> no_slot_zero_source{};
  const auto external_slot_zero =
      off::graphics::resolve_scene_geometry_references(
          primitives, no_slot_zero_source, slot_zero_entries);
  check(external_slot_zero[0].status ==
                off::graphics::SceneGeometryStatus::no_local_source &&
            external_slot_zero[0].requested_handle_slot_index == 0,
        "retain tagged runtime slot zero when no local source exists");

  bool missing_instance_rejected = false;
  try {
    const std::array<off::data::GmsDirectoryEntry, 0> no_objects{};
    const auto external = off::graphics::resolve_scene_geometry_references(
        primitives, no_objects, map_entries);
    check(external.size() == 2 &&
              external[0].status ==
                  off::graphics::SceneGeometryStatus::no_local_source &&
              !external[0].source_directory_index.has_value() &&
              external[0].requested_handle_slot_index == 9 &&
              external[1].requested_handle_slot_index == 1,
          "retain unresolved external handles explicitly");
    static_cast<void>(off::graphics::build_first_primary_scene_render_preview(
        primitives, textures, no_objects, map_entries));
  } catch (const std::runtime_error &) {
    missing_instance_rejected = true;
  }
  check(missing_instance_rejected,
        "reject render maps without a local GMS object instance");

  auto source_without_primitive = object_source;
  source_without_primitive.primitive_reference.reset();
  bool nonprimitive_source_rejected = false;
  try {
    const std::array nonprimitive_sources{source_without_primitive};
    const auto nonprimitive = off::graphics::resolve_scene_geometry_references(
        primitives, nonprimitive_sources, map_entries);
    check(nonprimitive[0].status ==
              off::graphics::SceneGeometryStatus::source_without_primitive,
          "classify a resolved source without direct primitive geometry");
    static_cast<void>(off::graphics::build_first_primary_scene_render_preview(
        primitives, textures, nonprimitive_sources, map_entries));
  } catch (const std::runtime_error &) {
    nonprimitive_source_rejected = true;
  }
  check(nonprimitive_source_rejected,
        "do not guess geometry for a non-primitive GMS source");

  auto missing_primitive_source = object_source;
  missing_primitive_source.primitive_reference = 99;
  const std::array missing_primitive_sources{missing_primitive_source};
  const auto missing_primitive =
      off::graphics::resolve_scene_geometry_references(
          primitives, missing_primitive_sources, map_entries);
  check(missing_primitive[0].status ==
                off::graphics::SceneGeometryStatus::missing_primitive &&
            missing_primitive[0].source_directory_index == 0 &&
            missing_primitive[0].primitive_reference == 99 &&
            !missing_primitive[0].primitive_entry_index.has_value(),
        "retain the identity of a missing PRM reference");

  auto primary_only_entry = map_entry;
  primary_only_entry.object.secondary_geometry_reference = 0;
  const std::array primary_only_entries{primary_only_entry};
  const auto primary_only = off::graphics::resolve_scene_geometry_references(
      primitives, object_sources, primary_only_entries);
  check(primary_only.size() == 1 &&
            primary_only[0].role == off::graphics::SceneGeometryRole::primary,
        "omit an absent optional secondary handle");

  bool duplicate_primitive_rejected = false;
  try {
    const std::array duplicate_primitives{primitive, primitive};
    static_cast<void>(off::graphics::build_first_primary_scene_render_preview(
        duplicate_primitives, textures, object_sources, map_entries));
  } catch (const std::runtime_error &) {
    duplicate_primitive_rejected = true;
  }
  check(duplicate_primitive_rejected,
        "reject ambiguous duplicate PRM packed indexes");

  bool duplicate_slot_rejected = false;
  try {
    const std::array duplicate_sources{object_source, object_source};
    static_cast<void>(off::graphics::build_first_primary_scene_render_preview(
        primitives, textures, duplicate_sources, map_entries));
  } catch (const std::runtime_error &) {
    duplicate_slot_rejected = true;
  }
  check(duplicate_slot_rejected, "reject ambiguous duplicate GMS local slots");

  auto flagged = primitive;
  flagged.packed_index = 0x8000002aU;
  flagged.flagged_reference = true;
  bool flagged_rejected = false;
  try {
    const std::array flagged_primitives{flagged};
    static_cast<void>(
        off::graphics::build_render_preview(flagged_primitives, textures));
  } catch (const std::runtime_error &) {
    flagged_rejected = true;
  }
  check(flagged_rejected,
        "do not treat high-bit PRM aliases as decoded render geometry");
  auto flagged_source = object_source;
  flagged_source.primitive_reference = flagged.packed_index;
  const std::array flagged_sources{flagged_source};
  const std::array flagged_primitives{flagged};
  const auto alias_resolution =
      off::graphics::resolve_scene_geometry_references(
          flagged_primitives, flagged_sources, map_entries);
  check(
      alias_resolution[0].status ==
              off::graphics::SceneGeometryStatus::unresolved_primitive_alias &&
          alias_resolution[0].primitive_reference == 0x8000002aU &&
          alias_resolution[0].primitive_entry_index == 0,
      "retain a high-bit PRM alias as unresolved scene geometry");

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
