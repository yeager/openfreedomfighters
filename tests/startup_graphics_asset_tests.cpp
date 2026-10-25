#include "off/graphics/startup_graphics_asset.hpp"
#include "off/graphics/startup_graphics_prepared_plan.hpp"
#include "off/graphics/picture_expansion.hpp"
#include "off/graphics/picture_material_state.hpp"
#include "off/graphics/picture_submission_cache.hpp"
#include "off/graphics/startup_graphics_expanded_plan.hpp"
#include "off/graphics/startup_graphics_diagnostic_plan.hpp"
#include "off/graphics/startup_graphics_transform_chain.hpp"
#include "off/graphics/startup_source_picture_draw_admission.hpp"
#include "off/graphics/startup_picture_pass_admission.hpp"
#include "off/platform/sdl_startup_picture_executor.hpp"

#include <bit>
#include <algorithm>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
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
static_assert(!std::is_copy_constructible_v<off::graphics::StartupSourcePictureDrawAdmission>);
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

off::data::PictureDrawPlan multi_descriptor_plan(std::size_t image_index,
                                                std::uint16_t texture_id,
                                                std::uint8_t record_tag) {
  std::vector<off::data::PictureResourceDescriptor> descriptors(2);
  for (std::size_t index = 0; index < descriptors.size(); ++index) {
    descriptors[index].local_center_x = static_cast<float>(index);
    descriptors[index].horizontal_edge_span = 2;
    descriptors[index].vertical_edge_span = 2;
    descriptors[index].modulation_color = 0xffffffffU;
  }
  const std::array groups{off::data::PictureDrawGroup{2, 0}};
  std::array bindings{off::data::PictureTextureBinding{
      32U + record_tag, 2048, texture_id, image_index,
      off::data::TextureManagerKeyBank::upper}};
  for (std::size_t byte = 0;
       byte < bindings[0].authored_texture_resource_record.size(); ++byte)
    bindings[0].authored_texture_resource_record[byte] =
        static_cast<std::byte>(record_tag + byte);
  auto result = off::data::PictureDrawPlan::build(descriptors, groups, bindings);
  return result;
}

off::data::StartupGraphicsComposition composition(
    std::uint16_t texture_id_bias = 0) {
  std::vector<off::data::StartupGraphicsRowComposition> rows(3);
  std::size_t next_picture = 200;
  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    auto &row = rows[row_index];
    row.owner_directory_index = 100 + row_index;
    row.slot_y = static_cast<float>(row_index == 0 ? 10 : 20);
    row.same_slot_multiplicity = row_index == 0 ? 1 : 2;
    row.same_slot_ordinal = row_index == 2 ? 1 : 0;
    row.authored_hidden = row_index == 2;
    row.construction_chain = {row.owner_directory_index};
    row.transform_chain = {{row.owner_directory_index, {}, {}}};
    const auto chrome_count = row_index == 0 ? 1U : 2U;
    for (std::size_t picture_index = 0; picture_index <= chrome_count; ++picture_index) {
      off::data::StartupGraphicsCompositionInstance picture;
      picture.role =
          picture_index == 0
              ? off::data::StartupGraphicsCompositionRole::row_background
              : off::data::StartupGraphicsCompositionRole::row_chrome;
      picture.directory_index = next_picture++;
      picture.construction_chain = {row.owner_directory_index,
                                    picture.directory_index};
      picture.transform_chain = {{row.owner_directory_index, {}, {}},
                                 {picture.directory_index, {}, {}}};
      picture.authored_state_mask = picture_index == 0 ? 0x80U : 0x01U;
      picture.base_render_property = static_cast<std::uint32_t>(
          0x2000U + row_index * 3U + picture_index);
      picture.authored_alpha = static_cast<std::uint8_t>(150U + picture_index);
      picture.alignment_enum = static_cast<std::uint8_t>(picture_index + 2U);
      picture.extension_control = picture_index == 1
                                      ? std::optional<std::uint8_t>{12U}
                                      : std::nullopt;
      const auto group_count = picture_index == 0 ? 1U : row_index + 1U;
      const auto image_bias = picture_index == 0 ? 0U : 1U;
      picture.draw_plan = row_index == 1 && picture_index == 1
          ? multi_descriptor_plan(image_bias,
                static_cast<std::uint16_t>(texture_id_bias + image_bias),
                static_cast<std::uint8_t>(row_index + picture_index))
          : plan(group_count, image_bias,
                static_cast<std::uint16_t>(texture_id_bias + image_bias),
                static_cast<std::uint8_t>(row_index + picture_index));
      row.pictures.push_back(std::move(picture));
    }
  }
  return off::data::StartupGraphicsComposition::from_rows(std::move(rows));
}

std::vector<std::byte> texture_catalog(std::uint32_t width = 2,
                                       std::uint32_t height = 2,
                                       std::size_t image_count = 4) {
  std::vector<std::byte> bytes(16, std::byte{0});
  std::vector<std::size_t> offsets(image_count);
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

int main() {
  const std::array<off::data::StartupGraphicsLocalTransform, 2> affine_chain{{
      {1, {2, 0, 0, 0, 3, 0, 0, 0, 1}, {5, 7, 0}},
      {2, {1, 0, 0, 0, 1, 0, 0, 0, 1}, {11, 13, 0}}}};
  const auto affine =
      off::graphics::compose_startup_graphics_transform_chain(affine_chain);
  check(affine.basis == std::array<float, 9>{2, 0, 0, 0, 3, 0, 0, 0, 1} &&
            affine.translation == std::array<float, 3>{27, 46, 0},
        "compose root-to-picture transforms in parent-first affine order");
  bool empty_chain_rejected = false;
  try {
    static_cast<void>(off::graphics::compose_startup_graphics_transform_chain({}));
  } catch (const std::runtime_error &) {
    empty_chain_rejected = true;
  }
  check(empty_chain_rejected,
        "reject a picture transform without a bounded source chain");
  auto catalog_bytes = texture_catalog();
  const auto catalog = off::data::TextureCatalog::parse(catalog_bytes);
  auto asset =
      off::graphics::build_startup_graphics_asset(composition(), catalog);
  const auto &authored = asset.composition().rows()[1].pictures[1];
  check(authored.base_render_property == 0x2004U &&
            authored.authored_alpha == 151U && authored.alignment_enum == 3U &&
            authored.extension_control == 12U,
        "asset ownership preserves authored window-picture controls");
  check(asset.images().size() == 4, "own each locally referenced decoded image");
  for (std::size_t index = 0; index < asset.images().size(); ++index) {
    check(asset.images()[index].catalog_image_index == index &&
              asset.images()[index].texture_id == index &&
              asset.images()[index].mip_zero.width == 2 &&
              asset.images()[index].mip_zero.height == 2 &&
              asset.images()[index].mip_zero.pixels.size() == 16,
          "retain sorted catalog-local identities and valid decoded pixels");
  }
  catalog_bytes.assign(catalog_bytes.size(), std::byte{0});
  check(asset.images().back().mip_zero.pixels[0] == 4,
        "own decoded image bytes independently of source storage");

  const auto prepared =
      off::graphics::prepare_startup_graphics_plan(asset, 0x01U);
  const auto composed_expanded =
      off::graphics::expand_startup_graphics_plan_with_composed_transforms(
          asset, 0x01U);
  check(composed_expanded.pictures().size() == prepared.pictures().size() &&
            composed_expanded.submissions().size() == prepared.submissions().size(),
        "source bridge expands every prepared startup submission");
  check(prepared.requested_state() == 0x01U &&
            prepared.effective_state() == 0x01U &&
            prepared.resources().size() == asset.images().size() &&
            prepared.pictures().size() == 5 &&
            prepared.quads().size() == 7 &&
            prepared.submissions().size() == prepared.quads().size(),
        "prepare the variable locally authored pre-raster plan");
  for (std::size_t ordinal = 0; ordinal < prepared.submissions().size();
       ++ordinal) {
    const auto &submission = prepared.submissions()[ordinal];
    check(submission.emission_ordinal == ordinal &&
              submission.prepared_quad_index == ordinal &&
              prepared.quads()[ordinal].emission_ordinal == ordinal &&
              prepared.quads()[ordinal].resource_index < prepared.resources().size(),
          "preserve contiguous traversal emission identities");
  }
  check(prepared.pictures().front().row_index == 1 &&
            prepared.pictures().front().picture_index == 1 &&
            prepared.pictures().front().base_render_property == 0x2004U &&
            prepared.pictures().front().authored_alpha == 151U &&
            prepared.pictures().front().alignment_enum == 3U &&
            prepared.pictures().front().extension_control == 12U &&
            prepared.pictures().front().first_submission == 0 &&
            prepared.pictures().front().submission_count == 2 &&
            prepared.quads().front().source.local_x_min == -1.0F &&
            prepared.quads().front().source.local_x_max == 1.0F &&
            prepared.quads().front().source.local_center_x == 0.0F &&
            prepared.quads().front().source.local_center_y == 0.0F &&
            prepared.quads().front().source.horizontal_edge_span == 2.0F &&
            prepared.quads().front().source.vertical_edge_span == 2.0F,
        "preserve first traversal picture and raw descriptor quad");
  check(prepared.pictures()[1].authored_alpha == 152U &&
            prepared.pictures()[1].extension_control == std::nullopt,
        "preserve distinct authored controls across local picture roles");
  check(prepared.quads().front().texture_resource_prm_offset !=
            prepared.quads()[5].texture_resource_prm_offset &&
            prepared.quads().front().authored_texture_resource_record !=
                prepared.quads()[5].authored_texture_resource_record,
        "preserve distinct source texture records for shared local images");
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
              active.pictures().size() == 2 && active.quads().size() == 2 &&
              active.submissions().size() == active.quads().size(),
          "prepare persistent backgrounds from the variable local row set");
  }
  const auto fallback =
      off::graphics::prepare_startup_graphics_plan(asset, 0x04U);
  check(fallback.effective_state() == 0x01U &&
            fallback.pictures().size() == prepared.pictures().size() &&
            fallback.quads().size() == prepared.quads().size(),
        "preserve the configured fallback state in the pre-raster plan");

  const auto transforms_for = [](const auto &plan) {
    std::vector<off::graphics::StartupGraphicsPictureTransform> transforms;
    for (const auto &picture : plan.pictures())
      transforms.push_back({picture.picture_directory_index,
          {{0, 0, 1, 0, 1, 0, 1, 0, 0},
           {static_cast<float>(picture.picture_directory_index), 0, 0}}});
    std::reverse(transforms.begin(), transforms.end());
    return transforms;
  };
  {
    // Synthetic live hierarchy snapshot uses the prepared local plan as its
    // independently checked expected picture sequence.
    std::vector<off::graphics::StartupLiveWindowNode> nodes;
    std::vector<off::graphics::StartupPictureOwnerView> owner_views;
    std::vector<std::size_t> live_rows;
    for (const auto& picture : prepared.pictures())
      if (picture.role == off::data::StartupGraphicsCompositionRole::row_background)
        live_rows.push_back(picture.row_index);
    std::ranges::sort(live_rows);
    live_rows.erase(std::unique(live_rows.begin(), live_rows.end()), live_rows.end());
    std::ranges::sort(live_rows, std::greater{});
    nodes.push_back({.identity = 1,
                     .kind = off::graphics::StartupLiveWindowKind::container,
                     .linked = true,
                     .candidate_eligible = true,
                     .has_render_component = true,
                     .first_child = 1000 + live_rows.front()});
    std::uint64_t next_group = 2000;
    std::uint64_t next_leaf = 3000;
    for (std::size_t live_ordinal = 0; live_ordinal < live_rows.size(); ++live_ordinal) {
      const auto row = live_rows[live_ordinal];
      const auto row_identity = 1000 + row;
      const auto next_row = live_ordinal + 1 == live_rows.size()
          ? std::optional<std::uint64_t>{}
          : std::optional<std::uint64_t>{1000 + live_rows[live_ordinal + 1]};
      const auto group_identity = next_group++;
      std::vector<const off::graphics::StartupGraphicsPreparedPicture*> chrome;
      const off::graphics::StartupGraphicsPreparedPicture* background = nullptr;
      for (const auto &picture : prepared.pictures()) {
        if (picture.row_index != row) continue;
        if (picture.role == off::data::StartupGraphicsCompositionRole::row_background)
          background = &picture;
        else chrome.push_back(&picture);
      }
      check(background != nullptr && !chrome.empty(),
            "synthetic live snapshot retains each local row's typed picture roles");
      const auto background_identity = next_leaf + chrome.size();
      nodes.push_back({.identity = row_identity,
                       .kind = off::graphics::StartupLiveWindowKind::container,
                       .source_sibling_ordinal = row,
                       .linked = true,
                       .candidate_eligible = true,
                       .has_render_component = true,
                       .first_child = group_identity,
                       .next_sibling = next_row});
      nodes.push_back({.identity = group_identity,
                       .kind = off::graphics::StartupLiveWindowKind::container,
                       .source_sibling_ordinal = 0,
                       .linked = true,
                       .candidate_eligible = true,
                       .has_render_component = true,
                       .first_child = next_leaf,
                       .next_sibling = background_identity});
      for (std::size_t index = 0; index < chrome.size(); ++index) {
        const auto *picture = chrome[index];
        const auto identity = next_leaf + index;
        nodes.push_back({.identity = identity,
                         .picture_directory_index = picture->picture_directory_index,
                         .kind = off::graphics::StartupLiveWindowKind::leaf,
                         .source_sibling_ordinal = index,
                         .linked = true,
                         .candidate_eligible = true,
                         .has_render_component = true,
                         .next_sibling = index + 1 == chrome.size()
                             ? std::optional<std::uint64_t>{}
                             : std::optional<std::uint64_t>{identity + 1}});
        owner_views.push_back({picture->picture_directory_index, identity, 4000 + row});
      }
      nodes.push_back({.identity = background_identity,
                       .picture_directory_index = background->picture_directory_index,
                       .kind = off::graphics::StartupLiveWindowKind::leaf,
                       .source_sibling_ordinal = 1,
                       .linked = true,
                       .candidate_eligible = true,
                       .has_render_component = true});
      owner_views.push_back(
          {background->picture_directory_index, background_identity, 4000 + row});
      next_leaf = background_identity + 1;
    }
    const off::graphics::StartupSourcePictureTraversalSnapshot snapshot{
        .root_identity = 1, .pass_context_identity = 9, .pass_value = 11,
        .nodes = nodes, .owner_views = owner_views};
    std::vector<std::string> trace;
    const off::graphics::StartupSourcePictureBackendHooks hooks{
        [&] { trace.push_back("device"); return true; },
        [&] { trace.push_back("state"); return true; },
        [&](const auto &, const auto &, auto pass) {
          trace.push_back("record:" + std::to_string(pass)); return true;
        },
        [&] { trace.push_back("preselect"); return true; },
        [&](auto, auto) { trace.push_back("texture"); return true; },
        [&](std::span<const off::graphics::StartupGraphicsExpandedSubmission> draws,
            std::span<const off::graphics::StartupGraphicsPreparedResource> resources) {
          check(resources.size() == prepared.resources().size(),
                "ordered source hook receives prepared resource identities");
          trace.push_back("ordered:" + std::to_string(draws.size())); return true;
        },
        [&](const auto &, const auto &, auto pass) {
          trace.push_back("submit:" + std::to_string(pass)); return true; }};
    off::graphics::StartupSourcePictureDrawAdmission admission;
    const auto output = admission.submit(asset, 0x01U, snapshot,
                                         transforms_for(prepared), hooks);
    const auto picture_count = prepared.pictures().size();
    const auto resource_count = prepared.resources().size();
    const auto draw_count = prepared.submissions().size();
    check(output.prepared_picture_count == picture_count &&
              output.submitted_draw_count == draw_count &&
              trace.size() == picture_count + resource_count + draw_count + 4 &&
              trace.front() == "device" && trace[1] == "state" &&
              trace[picture_count + 2] == "preselect" &&
              trace[picture_count + resource_count + 3] ==
                  "ordered:" + std::to_string(draw_count) &&
              trace.back() == "submit:9",
          "source-backed admission preserves live hierarchy, owner/view and backend stage order");
    auto hidden_nodes = nodes;
    for (auto &node : hidden_nodes) {
      if (node.picture_directory_index == prepared.pictures().front().picture_directory_index) {
        node.hide_control_bits = 0x2c00U;
        break;
      }
    }
    const off::graphics::StartupSourcePictureTraversalSnapshot hidden{
        .root_identity = 1, .pass_context_identity = 9, .pass_value = 11,
        .nodes = hidden_nodes, .owner_views = owner_views};
    bool hidden_rejected = false;
    try { static_cast<void>(admission.submit(asset, 0x01U, hidden,
                                              transforms_for(prepared), hooks)); }
    catch (const std::runtime_error &) { hidden_rejected = true; }
    check(hidden_rejected,
          "source-backed admission rejects an unproven visibility mismatch before backend calls");

    const off::graphics::StartupSceneLease scene{.identity = 73};
    const off::graphics::StartupActiveWindowTraversalRoot active_root{
        .scene_lease_identity = scene.identity, .root_identity = snapshot.root_identity};
    const off::graphics::StartupAdmittedCameraView camera_view{
        .scene_lease_identity = scene.identity, .camera_identity = 74,
        .view_identity = 4000, .enabled = true,
        .normalized_viewport = {0, 0, 1, 1}, .owner_projection_scalar = 1};
    const off::graphics::StartupRendererStatePassContext pass{
        .scene_lease_identity = scene.identity,
        .identity = snapshot.pass_context_identity,
        .rectangle = {0, 0, 640, 480}};
    auto pass_owner_views = owner_views;
    for (auto& association : pass_owner_views)
      association.admitted_view_identity = camera_view.view_identity;
    const off::graphics::StartupSourcePictureTraversalSnapshot pass_snapshot{
        .root_identity = snapshot.root_identity,
        .pass_context_identity = snapshot.pass_context_identity,
        .pass_value = snapshot.pass_value, .nodes = nodes,
        .owner_views = pass_owner_views};
    const auto pass_transforms = transforms_for(prepared);
    const off::graphics::StartupSourcePictureRuntimeSnapshot source{
        .scene_lease_identity = scene.identity, .traversal = pass_snapshot,
        .transforms = pass_transforms};
    off::graphics::StartupPicturePassAdmission pass_admission;
    const auto pass_result = pass_admission.submit(
        scene, active_root, camera_view, pass, source, 1.0F, asset, 0x01U, hooks);
    check(pass_result.prepared_picture_count == prepared.pictures().size() &&
              pass_result.submitted_draw_count == prepared.submissions().size(),
          "conditional picture pass delegates only after matching live root/view/context admission");

    // This stays CPU-only: the ordered hook turns the already-resident
    // source-boundary values into an immutable SDL packet, but never prepares
    // a GPU frame or receives a command buffer.
    off::platform::StartupPictureRenderState packet_state{};
    packet_state.projection = {1, 0, 0, 0, 0, 1, 0, 0,
                               0, 0, 1, 0, 23, 29, 0, 1};
    packet_state.viewport = {5, 7, 80, 60, 0, 1};
    packet_state.scissor = {5, 7, 80, 60};
    packet_state.packed_texture_factor = 0xa1b2c3d4U;
    std::vector<std::string> packet_trace;
    std::vector<off::platform::StartupPictureTextureHandle> resident_handles;
    std::optional<off::platform::SdlStartupPictureFramePacket> packet;
    const off::graphics::StartupSourcePictureBackendHooks packet_hooks{
        [&] { packet_trace.push_back("device"); return true; },
        [&] { packet_trace.push_back("state"); return true; },
        [&](const auto&, const auto&, auto) { packet_trace.push_back("record"); return true; },
        [&] { packet_trace.push_back("preselect"); return true; },
        [&](std::size_t resource_index, std::uint32_t texture_id) {
          packet_trace.push_back("resident");
          const auto& resource = prepared.resources().at(resource_index);
          if (resource.texture_id != texture_id) return false;
          resident_handles.push_back(
              {resource_index, resource.catalog_image_index, texture_id});
          return true;
        },
        [&](std::span<const off::graphics::StartupGraphicsExpandedSubmission> draws,
            std::span<const off::graphics::StartupGraphicsPreparedResource> resources) {
          packet_trace.push_back("ordered");
          packet.emplace(off::platform::SdlStartupPictureFramePacket::assemble(
              draws, resources, resident_handles, packet_state));
          return true;
        },
        [&](const auto&, const auto&, auto) { packet_trace.push_back("submit"); return true; }};
    const auto packet_result = pass_admission.submit(
        scene, active_root, camera_view, pass, source, 1.0F, asset, 0x01U, packet_hooks);
    check(packet_result.submitted_draw_count == draw_count && packet &&
              packet_trace[picture_count + 2] == "preselect" &&
              packet_trace[picture_count + resource_count + 2] == "resident" &&
              packet_trace[picture_count + resource_count + 3] == "ordered" &&
              packet_trace[picture_count + resource_count + 4] == "submit" &&
              packet->indexed_draw_count() == draw_count &&
              packet->draws().front().projection == packet_state.projection &&
              packet->draws().front().viewport.x == packet_state.viewport.x &&
              packet->draws().front().scissor.w == packet_state.scissor.w &&
              packet->draws().front().packed_texture_factor == packet_state.packed_texture_factor,
          "pass admission supplies resident resource identities to a CPU-only SDL packet hook unchanged");

    const auto rejects_packet_handles = [&](auto mutate, const char* message) {
      auto rejected_handles = resident_handles;
      rejected_handles.resize(prepared.resources().size());
      mutate(rejected_handles);
      std::size_t submits{};
      off::graphics::StartupSourcePictureBackendHooks rejecting_hooks = packet_hooks;
      rejecting_hooks.ordered_draw_admitted =
          [&](std::span<const off::graphics::StartupGraphicsExpandedSubmission> draws,
              std::span<const off::graphics::StartupGraphicsPreparedResource> resources) {
            try {
              static_cast<void>(off::platform::SdlStartupPictureFramePacket::assemble(
                  draws, resources, rejected_handles, packet_state));
            } catch (const std::runtime_error&) { return false; }
            return true;
          };
      rejecting_hooks.submit = [&](const auto&, const auto&, auto) { ++submits; return true; };
      bool rejected = false;
      try { static_cast<void>(pass_admission.submit(
          scene, active_root, camera_view, pass, source, 1.0F, asset, 0x01U, rejecting_hooks)); }
      catch (const std::runtime_error&) { rejected = true; }
      check(rejected && submits == 0, message);
    };
    rejects_packet_handles([&](auto& handles) {
      const auto first_submission_quad =
          prepared.submissions().front().prepared_quad_index;
      const auto used_resource = prepared.quads().at(first_submission_quad).resource_index;
      const auto found = std::find_if(handles.begin(), handles.end(),
          [&](const auto& handle) { return handle.resource_index == used_resource; });
      if (found != handles.end()) handles.erase(found);
    }, "missing resident handle for a submitted draw rejects before source submit");
    rejects_packet_handles([](auto& handles) {
      handles.push_back({99, 99, 99});
    }, "extra resident packet handle rejects before any source submit");
    rejects_packet_handles([](auto& handles) {
      std::swap(handles[0].catalog_image_index, handles[1].catalog_image_index);
      std::swap(handles[0].texture_id, handles[1].texture_id);
    }, "swapped resident packet identities reject before any source submit");
    auto wrong_view = camera_view;
    wrong_view.view_identity = 4001;
    bool view_rejected = false;
    const auto before_view_rejection = trace.size();
    try {
      static_cast<void>(pass_admission.submit(scene, active_root, wrong_view, pass,
                                              source, 1.0F, asset, 0x01U, hooks));
    } catch (const std::runtime_error&) { view_rejected = true; }
    check(view_rejected,
          "conditional picture pass rejects owner/view disagreement before downstream hooks");
    check(trace.size() == before_view_rejection,
          "conditional picture pass fails closed before downstream hooks on view disagreement");
    auto wrong_pass = pass;
    wrong_pass.identity = 10;
    bool pass_rejected = false;
    try {
      static_cast<void>(pass_admission.submit(scene, active_root, camera_view, wrong_pass,
                                              source, 1.0F, asset, 0x01U, hooks));
    } catch (const std::runtime_error&) { pass_rejected = true; }
    check(pass_rejected,
          "conditional picture pass rejects a mismatched renderer-state pass context");
    auto disabled = camera_view;
    disabled.enabled = false;
    bool disabled_rejected = false;
    try {
      static_cast<void>(pass_admission.submit(scene, active_root, disabled, pass,
                                              source, 1.0F, asset, 0x01U, hooks));
    } catch (const std::runtime_error&) { disabled_rejected = true; }
    check(disabled_rejected,
          "conditional picture pass rejects a camera that is not explicitly enabled");
  }
  for (const auto state : {0x01U, 0x08U, 0x04U}) {
    const auto input = off::graphics::prepare_startup_graphics_plan(asset, state);
    auto transforms = transforms_for(input);
    const auto expanded = off::graphics::expand_startup_graphics_plan(input, transforms);
    check(expanded.requested_state() == state &&
              expanded.effective_state() == input.effective_state() &&
              expanded.resources().size() == asset.images().size() &&
              expanded.pictures().size() == input.pictures().size() &&
              expanded.submissions().size() == input.submissions().size(),
          "expand variable local shapes while preserving requested and effective state");
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
                static_cast<float>(source.picture_directory_index) +
                    source.source.local_x_min &&
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
  check(owned_expanded.resources().size() == 4 &&
            owned_expanded.resources().front().texture_id == prepared.resources().front().texture_id &&
            owned_expanded.pictures().front().authored_alpha == 151U &&
            owned_expanded.submissions().size() == 7 &&
            owned_expanded.submissions().front().vertices[0].position[0] ==
                    static_cast<float>(prepared.quads().front().picture_directory_index) +
                        prepared.quads().front().source.local_x_min,
        "expanded metadata and geometry outlive all input assets, prepared plans and transforms");
  check(owned_expanded.submissions()[5].texture_resource_prm_offset > 0 &&
            owned_expanded.submissions()[5].authored_texture_resource_record[31] !=
                std::byte{0},
        "expanded initial resource provenance outlives all input owners");

  {
    auto bytes = texture_catalog();
    const auto local_catalog = off::data::TextureCatalog::parse(bytes);
    const auto local_asset =
        off::graphics::build_startup_graphics_asset(composition(), local_catalog);
    const auto local_prepared =
        off::graphics::prepare_startup_graphics_plan(local_asset, 0x01U);
    const auto diagnostic = off::graphics::make_startup_graphics_diagnostic_plan(
        local_asset, off::graphics::expand_startup_graphics_plan_with_composed_transforms(
                         local_asset, 0x01U));
    check(diagnostic.source_only_diagnostic &&
              diagnostic.textures.size() == local_asset.images().size() &&
              diagnostic.meshes.size() == local_prepared.submissions().size() &&
              diagnostic.instances.size() == diagnostic.meshes.size() &&
              diagnostic.draws.size() == diagnostic.meshes.size() &&
              diagnostic.draws.front().depth_policy ==
                  off::graphics::SceneDepthPolicy::test_only &&
              diagnostic.draws.front().blend_enabled &&
              diagnostic.draws.front().first_index == 0 &&
              diagnostic.draws.front().index_count == 4 &&
              std::is_sorted(diagnostic.draws.begin(), diagnostic.draws.end(),
                             [](const auto &left, const auto &right) {
                               return left.source_diagnostic_depth >
                                      right.source_diagnostic_depth;
                             }),
          "build a source-only startup graphics GPU diagnostic with ordered blended quads");
  }

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
                                                                  catalog, 63));
  } catch (const std::runtime_error &) {
    budget_rejected = true;
  }
  check(budget_rejected, "reject an aggregate decoded-byte budget overflow");
  check(off::graphics::build_startup_graphics_asset(composition(), catalog, 64)
                .images()
                .size() == asset.images().size(),
        "accept an exact aggregate decoded-byte budget");

  auto bad_rows = composition().rows();
  bad_rows[1].pictures[1].draw_plan = {};
  bool signature_rejected = false;
  try {
    static_cast<void>(
        off::data::StartupGraphicsComposition::from_rows(std::move(bad_rows)));
  } catch (const std::runtime_error &) {
    signature_rejected = true;
  }
  check(signature_rejected, "reject an empty local draw plan");

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
  std::cout << "startup graphics asset tests passed\n";
}
