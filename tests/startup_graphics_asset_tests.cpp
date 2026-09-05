#include "off/graphics/startup_graphics_asset.hpp"
#include "off/graphics/startup_graphics_prepared_plan.hpp"
#include "off/graphics/picture_expansion.hpp"
#include "off/graphics/picture_material_state.hpp"
#include "off/graphics/startup_graphics_expanded_plan.hpp"

#include <bit>
#include <algorithm>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <zlib.h>

namespace {

constexpr std::size_t index_bytes = 2048 * sizeof(std::uint32_t);
static_assert(!std::is_default_constructible_v<off::graphics::StartupGraphicsAsset>);
static_assert(
    !std::is_default_constructible_v<off::graphics::StartupGraphicsPreparedPlan>);
template <typename T>
concept HasPixels = requires(T value) { value.pixels; };
template <typename T>
concept HasTopology = requires(T value) { value.topology; };
template <typename T>
concept HasBlend = requires(T value) { value.blend_enabled; };
static_assert(!HasPixels<off::graphics::StartupGraphicsPreparedResource>);
static_assert(!HasTopology<off::graphics::StartupGraphicsPreparedSubmission>);
static_assert(!HasBlend<off::graphics::StartupGraphicsPreparedSubmission>);

void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void append_u16(std::vector<std::byte> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void set_u32(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] =
        static_cast<std::byte>((value >> shift) & 0xffU);
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto source = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), source.begin(), source.end());
}

off::data::PictureDrawPlan plan(std::size_t count, std::size_t image_bias,
                                std::uint16_t texture_id_bias = 0,
                                std::uint8_t record_tag = 0) {
  std::vector<off::data::PictureResourceDescriptor> descriptors(count);
  std::vector<off::data::PictureDrawGroup> groups;
  std::vector<off::data::PictureTextureBinding> bindings;
  for (std::size_t index = 0; index < count; ++index) {
    descriptors[index].horizontal_edge_span = 2;
    descriptors[index].vertical_edge_span = 2;
    descriptors[index].modulation_color = 0xffffffffU;
    groups.push_back({1, index});
    bindings.push_back({0, static_cast<std::uint16_t>(2048 + index),
                        static_cast<std::uint16_t>(texture_id_bias + index),
                        image_bias + index,
                        off::data::TextureManagerKeyBank::upper});
    bindings.back().prm_offset = static_cast<std::uint32_t>(32 * (index + 1) + record_tag);
    for (std::size_t byte = 0; byte < 32; ++byte)
      bindings.back().authored_texture_resource_record[byte] =
          static_cast<std::byte>(record_tag + byte);
  }
  return off::data::PictureDrawPlan::build(descriptors, groups, bindings);
}

off::data::StartupGraphicsComposition composition(
    std::uint16_t texture_id_bias = 0) {
  std::array<off::data::StartupGraphicsRowComposition, 8> rows;
  const auto background = plan(1, 0, texture_id_bias);
  const auto chrome =
      plan(5, 1, static_cast<std::uint16_t>(texture_id_bias + 1));
  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    auto &row = rows[row_index];
    row.owner_directory_index = 100 + row_index;
    row.slot_y = static_cast<float>(row_index < 6 ? row_index * 18 : 108);
    row.same_slot_multiplicity = row_index < 6 ? 1 : 2;
    row.same_slot_ordinal = row_index < 6 ? 0 : row_index - 6;
    row.authored_hidden = row_index == 7;
    row.construction_chain = {row.owner_directory_index};
    row.transform_chain = {{row.owner_directory_index, {}, {}}};
    for (std::size_t picture_index = 0; picture_index < 3; ++picture_index) {
      auto &picture = row.pictures[picture_index];
      picture.role =
          picture_index == 0
              ? off::data::StartupGraphicsCompositionRole::row_background
              : off::data::StartupGraphicsCompositionRole::row_chrome;
      picture.directory_index = 200 + row_index * 3 + picture_index;
      picture.construction_chain = {row.owner_directory_index,
                                    picture.directory_index};
      picture.transform_chain = {{row.owner_directory_index, {}, {}},
                                 {picture.directory_index, {}, {}}};
      picture.authored_state_mask = picture_index == 0 ? 0x80U : 0x01U;
      picture.base_render_property = static_cast<std::uint32_t>(
          0x2000U + row_index * 3U + picture_index);
      picture.authored_alpha = static_cast<std::uint8_t>(220U + picture_index);
      picture.alignment_enum = static_cast<std::uint8_t>(picture_index + 6U);
      picture.extension_control = picture_index == 1
                                      ? std::optional<std::uint8_t>{12U}
                                      : std::nullopt;
      if (row_index == 6 && picture_index == 2) {
        picture.base_render_property = 0xfedcba98U;
        picture.authored_alpha = 255U;
        picture.alignment_enum = 15U;
        picture.extension_control = 16U;
      }
      picture.draw_plan = picture_index == 0 ? background :
          (picture_index == 1 ? chrome :
              plan(5, 1, static_cast<std::uint16_t>(texture_id_bias + 1), 64));
    }
  }
  return off::data::StartupGraphicsComposition::from_rows(std::move(rows));
}

std::vector<std::byte> texture_catalog(std::uint32_t width = 2,
                                       std::uint32_t height = 2) {
  std::vector<std::byte> bytes(16, std::byte{0});
  std::array<std::size_t, 6> offsets{};
  for (std::size_t image = 0; image < offsets.size(); ++image) {
    offsets[image] = bytes.size();
    append_u32(bytes, 0);
    append_u32(bytes, 0x52474241U);
    append_u32(bytes, 0x52474241U);
    append_u32(bytes, static_cast<std::uint32_t>(image));
    append_u32(bytes, width | (height << 16U));
    append_u32(bytes, 1);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_text(bytes, "asset");
    bytes.push_back(std::byte{0});
    const auto mip_bytes = width * height * 4U;
    append_u32(bytes, mip_bytes);
    bytes.insert(bytes.end(), mip_bytes, static_cast<std::byte>(image + 1));
    set_u32(bytes, offsets[image],
            static_cast<std::uint32_t>(bytes.size() - offsets[image]));
  }
  const auto data_end = bytes.size();
  bytes.insert(bytes.end(), index_bytes, std::byte{0});
  for (std::size_t image = 0; image < offsets.size(); ++image)
    set_u32(bytes, data_end + image * 4,
            static_cast<std::uint32_t>(offsets[image]));
  const auto sequence_index = bytes.size();
  bytes.insert(bytes.end(), index_bytes, std::byte{0});
  set_u32(bytes, 0, static_cast<std::uint32_t>(data_end));
  set_u32(bytes, 4, static_cast<std::uint32_t>(sequence_index));
  set_u32(bytes, 8, 3);
  set_u32(bytes, 12, 4);
  return bytes;
}

void write_zip(const std::filesystem::path &path,
               const std::vector<std::pair<std::string, std::vector<std::byte>>>
                   &members) {
  struct Central {
    std::string name;
    std::uint32_t crc{};
    std::uint32_t size{};
    std::uint32_t offset{};
  };
  std::vector<std::byte> bytes;
  std::vector<Central> central;
  for (const auto &[name, payload] : members) {
    const auto crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef *>(payload.data()),
                static_cast<uInt>(payload.size())));
    const auto offset = static_cast<std::uint32_t>(bytes.size());
    append_u32(bytes, 0x04034b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, crc);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    append_u16(bytes, static_cast<std::uint16_t>(name.size()));
    append_u16(bytes, 0);
    append_text(bytes, name);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    central.push_back(
        {name, crc, static_cast<std::uint32_t>(payload.size()), offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : central) {
    append_u32(bytes, 0x02014b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, entry.crc);
    append_u32(bytes, entry.size);
    append_u32(bytes, entry.size);
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, entry.offset);
    append_text(bytes, entry.name);
  }
  const auto central_size =
      static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06054b50U);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u32(bytes, central_size);
  append_u32(bytes, central_offset);
  append_u16(bytes, 0);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output)
    throw std::runtime_error("could not write startup ZIP fixture");
}

bool load_rejects(const std::filesystem::path &path, std::string_view text) {
  try {
    static_cast<void>(off::graphics::load_startup_graphics_asset(path));
    return false;
  } catch (const std::runtime_error &error) {
    return std::string_view{error.what()}.find(text) != std::string_view::npos;
  }
}

} // namespace

int main(int argc, char **argv) {
  auto catalog_bytes = texture_catalog();
  const auto catalog = off::data::TextureCatalog::parse(catalog_bytes);
  auto asset =
      off::graphics::build_startup_graphics_asset(composition(), catalog);
  const auto &authored = asset.composition().rows()[4].pictures[1];
  check(authored.base_render_property == 0x200dU &&
            authored.authored_alpha == 221U && authored.alignment_enum == 7U &&
            authored.extension_control == 12U,
        "asset ownership preserves authored window-picture controls");
  check(asset.images().size() == 6, "own exactly six decoded startup images");
  for (std::size_t index = 0; index < asset.images().size(); ++index) {
    check(asset.images()[index].catalog_image_index == index &&
              asset.images()[index].texture_id == index &&
              asset.images()[index].mip_zero.width == 2 &&
              asset.images()[index].mip_zero.height == 2 &&
              asset.images()[index].mip_zero.pixels.size() == 16,
          "retain sorted catalog-local identities and valid decoded pixels");
  }
  catalog_bytes.assign(catalog_bytes.size(), std::byte{0});
  check(asset.images()[5].mip_zero.pixels[0] == 6,
        "own decoded image bytes independently of source storage");

  const auto prepared =
      off::graphics::prepare_startup_graphics_plan(asset, 0x01U);
  check(prepared.requested_state() == 0x01U &&
            prepared.effective_state() == 0x01U &&
            prepared.resources().size() == 6 &&
            prepared.pictures().size() == 21 &&
            prepared.quads().size() == 77 &&
            prepared.submissions().size() == 77,
        "prepare the bounded resting pre-raster plan");
  for (std::size_t ordinal = 0; ordinal < prepared.submissions().size();
       ++ordinal) {
    const auto &submission = prepared.submissions()[ordinal];
    check(submission.emission_ordinal == ordinal &&
              submission.prepared_quad_index == ordinal &&
              prepared.quads()[ordinal].emission_ordinal == ordinal &&
              prepared.quads()[ordinal].resource_index < 6,
          "preserve contiguous traversal emission identities");
  }
  check(prepared.pictures().front().row_index == 6 &&
            prepared.pictures().front().picture_index == 1 &&
            prepared.pictures().front().base_render_property == 0x2013U &&
            prepared.pictures().front().authored_alpha == 221U &&
            prepared.pictures().front().alignment_enum == 7U &&
            prepared.pictures().front().extension_control == 12U &&
            prepared.pictures().front().first_submission == 0 &&
            prepared.pictures().front().submission_count == 5 &&
            prepared.quads().front().source.local_x_min == -1.0F &&
            prepared.quads().front().source.local_x_max == 1.0F &&
            prepared.quads().front().source.local_center_x == 0.0F &&
            prepared.quads().front().source.local_center_y == 0.0F &&
            prepared.quads().front().source.horizontal_edge_span == 2.0F &&
            prepared.quads().front().source.vertical_edge_span == 2.0F,
        "preserve first traversal picture and raw descriptor quad");
  check(prepared.pictures()[1].base_render_property == 0xfedcba98U &&
            prepared.pictures()[1].authored_alpha == 255U &&
            prepared.pictures()[1].alignment_enum == 15U &&
            prepared.pictures()[1].extension_control == 16U,
        "preserve clamped authored picture-control maxima");
  check(prepared.quads()[0].resource_index == prepared.quads()[5].resource_index &&
            prepared.quads()[0].texture_resource_prm_offset == 32 &&
            prepared.quads()[5].texture_resource_prm_offset == 96 &&
            prepared.quads()[0].authored_texture_resource_record[0] == std::byte{0} &&
            prepared.quads()[5].authored_texture_resource_record[0] == std::byte{64},
        "prepared groups retain distinct authored records sharing a decoded image");
  const auto owned_controls = [] {
    auto bytes = texture_catalog();
    const auto local_catalog = off::data::TextureCatalog::parse(bytes);
    return off::graphics::prepare_startup_graphics_plan(
        off::graphics::build_startup_graphics_asset(composition(),
                                                     local_catalog),
        0x80U);
  }();
  check(owned_controls.pictures().front().picture_index == 0 &&
            !owned_controls.pictures().front().extension_control.has_value(),
        "own an absent optional picture control beyond source lifetimes");
  for (const auto state : {0x08U, 0x10U, 0x20U, 0x80U}) {
    const auto active =
        off::graphics::prepare_startup_graphics_plan(asset, state);
    check(active.effective_state() == state &&
              active.pictures().size() == 7 && active.quads().size() == 7 &&
              active.submissions().size() == 7,
          "prepare each canonical background-only state shape");
  }
  const auto fallback =
      off::graphics::prepare_startup_graphics_plan(asset, 0x04U);
  check(fallback.effective_state() == 0x01U &&
            fallback.pictures().size() == 21 &&
            fallback.quads().size() == 77,
        "preserve recovered fallback state in the pre-raster plan");

  const auto transforms_for = [](const auto &plan) {
    std::vector<off::graphics::StartupGraphicsPictureTransform> transforms;
    for (const auto &picture : plan.pictures())
      transforms.push_back({picture.picture_directory_index,
          {{0, 0, 1, 0, 1, 0, 1, 0, 0},
           {static_cast<float>(picture.picture_directory_index), 0, 0}}});
    std::reverse(transforms.begin(), transforms.end());
    return transforms;
  };
  for (const auto state : {0x01U, 0x08U, 0x04U}) {
    const auto input = off::graphics::prepare_startup_graphics_plan(asset, state);
    auto transforms = transforms_for(input);
    const auto expanded = off::graphics::expand_startup_graphics_plan(input, transforms);
    check(expanded.requested_state() == state &&
              expanded.effective_state() == input.effective_state() &&
              expanded.resources().size() == 6 &&
              expanded.pictures().size() == input.pictures().size() &&
              expanded.submissions().size() == input.submissions().size(),
          "expand 21/77 and 7/7 shapes while preserving requested and effective state");
    for (std::size_t i = 0; i < expanded.pictures().size(); ++i) {
      const auto &a = expanded.pictures()[i];
      const auto &b = input.pictures()[i];
      check(a.picture_directory_index == b.picture_directory_index &&
                a.base_render_property == b.base_render_property &&
                a.authored_alpha == b.authored_alpha &&
                a.alignment_enum == b.alignment_enum &&
                a.extension_control == b.extension_control &&
                a.first_submission == b.first_submission &&
                a.submission_count == b.submission_count,
            "preserve opaque picture controls without applying them");
    }
    for (std::size_t i = 0; i < expanded.submissions().size(); ++i) {
      const auto &output = expanded.submissions()[i];
      const auto &source = input.quads()[i];
      const auto &binding = asset.composition().rows()[source.row_index]
          .pictures[source.picture_index].draw_plan.groups()[source.group_index].texture;
      check(source.texture_resource_prm_offset == binding.prm_offset &&
                source.authored_texture_resource_record == binding.authored_texture_resource_record &&
                output.texture_resource_prm_offset == binding.prm_offset &&
                output.authored_texture_resource_record == binding.authored_texture_resource_record,
            "prepared and expanded submissions retain all initial per-group record bytes");
      check(output.emission_ordinal == i &&
                output.resource_index == source.resource_index &&
                output.picture_directory_index == source.picture_directory_index &&
                output.row_index == source.row_index && output.picture_index == source.picture_index &&
                output.group_index == source.group_index && output.descriptor_index == source.descriptor_index &&
                output.prepared_picture_index == input.submissions()[i].prepared_picture_index,
            "preserve ordered submission and resource identities");
      check(output.vertices[0].position[0] ==
                static_cast<float>(source.picture_directory_index) - 1.0F &&
                output.indices == std::array<std::uint16_t, 6>{0, 1, 3, 1, 2, 3} &&
                output.vertices[0].color == ((source.source.modulation_color >> 1U) & 0x7f7f7f7fU),
            "key shuffled transforms by directory identity with local topology and unchanged color policy");
    }
    const auto reject_transforms = [&](const auto &bad) {
      bool rejected = false;
      try { static_cast<void>(off::graphics::expand_startup_graphics_plan(input, bad)); }
      catch (const std::runtime_error &) { rejected = true; }
      check(rejected, "reject incomplete, duplicate, extra, unknown or non-finite transform mapping");
    };
    auto bad = transforms;
    bad.pop_back();
    reject_transforms(bad);
    bad = transforms;
    bad.push_back(transforms.front());
    reject_transforms(bad);
    bad = transforms;
    bad[1].picture_directory_index = bad[0].picture_directory_index;
    reject_transforms(bad);
    bad = transforms;
    bad[0].picture_directory_index = std::numeric_limits<std::size_t>::max();
    reject_transforms(bad);
    bad = transforms;
    bad[0].transform.basis[4] = std::numeric_limits<float>::quiet_NaN();
    reject_transforms(bad);
    bad = transforms;
    bad[0].transform.translation[0] = std::numeric_limits<float>::infinity();
    reject_transforms(bad);
  }
  const auto owned_expanded = [&] {
    auto bytes = texture_catalog();
    const auto local_catalog = off::data::TextureCatalog::parse(bytes);
    const auto local_asset = off::graphics::build_startup_graphics_asset(composition(), local_catalog);
    const auto local_prepared = off::graphics::prepare_startup_graphics_plan(local_asset, 0x01U);
    const auto local_transforms = transforms_for(local_prepared);
    return off::graphics::expand_startup_graphics_plan(local_prepared, local_transforms);
  }();
  check(owned_expanded.resources().size() == 6 &&
            owned_expanded.resources().front().texture_id == prepared.resources().front().texture_id &&
            owned_expanded.pictures().front().authored_alpha == 221U &&
            owned_expanded.submissions().size() == 77 &&
            owned_expanded.submissions().front().vertices[0].position[0] ==
                static_cast<float>(prepared.quads().front().picture_directory_index) - 1.0F,
        "expanded metadata and geometry outlive all input assets, prepared plans and transforms");
  check(owned_expanded.submissions()[5].texture_resource_prm_offset == 96 &&
            owned_expanded.submissions()[5].authored_texture_resource_record[31] == std::byte{95},
        "expanded initial resource provenance outlives all input owners");

  const auto mismatched_asset = off::graphics::build_startup_graphics_asset(
      composition(20), catalog);
  bool identity_rejected = false;
  try {
    static_cast<void>(off::graphics::prepare_startup_graphics_plan(
        mismatched_asset, 0x01U));
  } catch (const std::runtime_error &) {
    identity_rejected = true;
  }
  check(identity_rejected,
        "reject disagreement between draw-group and asset texture IDs");

  bool budget_rejected = false;
  try {
    static_cast<void>(off::graphics::build_startup_graphics_asset(composition(),
                                                                  catalog, 95));
  } catch (const std::runtime_error &) {
    budget_rejected = true;
  }
  check(budget_rejected, "reject an aggregate decoded-byte budget overflow");
  check(off::graphics::build_startup_graphics_asset(composition(), catalog, 96)
                .images()
                .size() == 6,
        "accept an exact aggregate decoded-byte budget");

  auto bad_rows = composition().rows();
  bad_rows[7].pictures[2].draw_plan = plan(5, 0);
  bool signature_rejected = false;
  try {
    static_cast<void>(
        off::data::StartupGraphicsComposition::from_rows(std::move(bad_rows)));
  } catch (const std::runtime_error &) {
    signature_rejected = true;
  }
  check(signature_rejected, "reject a noncanonical six-image signature");

  const auto work = std::filesystem::path{OFF_TEST_WORK_DIR};
  std::filesystem::remove_all(work);
  std::filesystem::create_directories(work);
  const std::vector<std::byte> invalid{std::byte{0}};
  const auto missing = work / "missing.zip";
  write_zip(
      missing,
      {{"scene.gms", invalid}, {"scene.buf", invalid}, {"scene.prm", invalid}});
  check(load_rejects(missing, "no .tex member"),
        "reject a startup archive missing TEX");
  const auto duplicate = work / "duplicate.zip";
  write_zip(duplicate, {{"scene.gms", invalid},
                        {"other.GMS", invalid},
                        {"scene.buf", invalid},
                        {"scene.prm", invalid},
                        {"scene.tex", invalid}});
  check(load_rejects(duplicate, "duplicate .gms members"),
        "reject duplicate case-insensitive GMS members");
  const auto malformed = work / "malformed.zip";
  write_zip(malformed, {{"scene.gms", invalid},
                        {"scene.buf", invalid},
                        {"scene.prm", invalid},
                        {"scene.tex", invalid}});
  check(load_rejects(malformed, "packed-resource"),
        "reject malformed startup GMS before composition");
  std::filesystem::remove_all(work);
  if (argc == 2) {
    const auto retail = off::graphics::load_startup_graphics_asset(argv[1]);
    check(retail.images().size() == 6,
          "load six owned images from the verified retail startup archive");
    const auto retail_prepared =
        off::graphics::prepare_startup_graphics_plan(retail, 0x01U);
    check(retail_prepared.pictures().size() == 21 &&
              retail_prepared.submissions().size() == 77,
          "prepare the canonical retail startup pre-raster plan");
    for (const auto &picture : retail_prepared.pictures()) {
      const auto property = picture.base_render_property;
      check(property == 0 || property == 1 || property == 5,
            "retain the observed retail base-picture property domain");
      const auto mapped = off::graphics::map_base_picture_material_property(property);
      check(mapped == (property == 0 ? 0x60010U :
                       property == 1 ? 0x60012U : 0x60210U),
            "map real authored properties without claiming final resource state");
    }
    // Explicit test-only transform: validates descriptor compatibility, not
    // recovered startup placement or final on-screen appearance.
    const off::graphics::PictureCacheTransform compatibility_transform{
        .basis = {0, 0, 1, 0, 1, 0, 1, 0, 0}, .translation = {0, 0, 0}};
    for (const auto &quad : retail_prepared.quads()) {
      const auto expanded = off::graphics::expand_picture_descriptors(
          std::span{&quad.source, 1}, compatibility_transform);
      check(expanded.size() == 1 && expanded[0].vertices.size() == 4 &&
                expanded[0].indices.size() == 6,
            "expand real startup descriptors with an explicit test transform");
    }
    std::vector<off::graphics::StartupGraphicsPictureTransform>
        retail_compatibility_transforms;
    for (const auto &picture : retail_prepared.pictures())
      retail_compatibility_transforms.push_back(
          {picture.picture_directory_index, compatibility_transform});
    const auto retail_expanded = off::graphics::expand_startup_graphics_plan(
        retail_prepared, retail_compatibility_transforms);
    check(retail_expanded.submissions().size() == 77 &&
              retail_expanded.pictures().size() == 21 &&
              retail_expanded.resources().size() == 6,
          "expand the real startup plan using explicit test-only transforms");
    for (std::size_t i = 0; i < retail_expanded.submissions().size(); ++i) {
      const auto &output = retail_expanded.submissions()[i];
      const auto &source = retail_prepared.quads()[i];
      const auto &binding = retail.composition().rows()[source.row_index]
          .pictures[source.picture_index].draw_plan.groups()[source.group_index].texture;
      check(source.texture_resource_prm_offset == binding.prm_offset &&
                source.authored_texture_resource_record == binding.authored_texture_resource_record &&
                output.texture_resource_prm_offset == binding.prm_offset &&
                output.authored_texture_resource_record == binding.authored_texture_resource_record,
            "retain real initial resource records and PRM identity through both CPU plans");
      check(output.emission_ordinal == i &&
                output.picture_directory_index == source.picture_directory_index &&
                output.resource_index == source.resource_index &&
                retail_expanded.resources()[output.resource_index].texture_id ==
                    retail_prepared.resources()[source.resource_index].texture_id,
            "preserve retail submission order, picture identity and resource pairing");
    }
    for (unsigned state = 0; state < 256; ++state) {
      const auto requested = static_cast<std::uint8_t>(state);
      const auto prepared_state =
          off::graphics::prepare_startup_graphics_plan(retail, requested);
      const auto effective = (state & 0xb9U) == 0 ? 1U : state;
      const bool chrome_visible = (effective & 1U) != 0;
      check(prepared_state.requested_state() == requested &&
                prepared_state.effective_state() == effective &&
                prepared_state.pictures().size() ==
                    (chrome_visible ? 21U : 7U) &&
                prepared_state.submissions().size() ==
                    (chrome_visible ? 77U : 7U),
            "validate every retail startup state mask and fallback");
      for (const auto &picture : prepared_state.pictures()) {
        const auto &row = retail.composition().rows()[picture.row_index];
        const auto &source = row.pictures[picture.picture_index];
        check(!row.authored_hidden &&
                  picture.base_render_property == source.base_render_property &&
                  picture.authored_alpha == source.authored_alpha &&
                  picture.alignment_enum == source.alignment_enum &&
                  picture.extension_control == source.extension_control,
              "retain real authored controls across every startup state");
      }
    }
  }
  std::cout << "startup graphics asset tests passed\n";
}
