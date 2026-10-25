#include "off/data/startup_graphics_composition.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
void check(bool value, const char* message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

constexpr std::uint32_t overlay_type = 0x0010002eU;
constexpr std::uint32_t window_type = 0x00100031U;
constexpr std::uint32_t group_type = 0x00100001U;
constexpr std::uint32_t control_type = 0x00100033U;
constexpr std::uint32_t row_type = 0x00100034U;
constexpr std::uint32_t text_type = 0x0020002dU;
constexpr std::uint32_t picture_type = 0x00200046U;

struct Fixture {
  std::vector<off::data::GmsDirectoryEntry> directory;
  std::vector<off::data::GmsHierarchyNode> hierarchy;
  std::vector<std::size_t> registered_window_owner_sources;
  std::size_t overlay_index{};

  std::size_t add(std::optional<std::size_t> parent, std::uint32_t type,
                  float x, float y) {
    const auto index = directory.size();
    off::data::GmsDirectoryEntry entry;
    entry.source_type = type;
    entry.position = {x, y, 0};
    directory.push_back(entry);
    hierarchy.push_back({index, parent, {}});
    if (parent) hierarchy[*parent].children_in_directory_order.push_back(index);
    return index;
  }
};

Fixture valid_fixture() {
  Fixture f;
  const auto add_rows = [&](std::size_t container) {
    for (std::size_t row_index = 0; row_index < 3; ++row_index) {
      const auto row = f.add(container, row_type, 20,
                             static_cast<float>(10 + row_index * 10));
      f.add(row, picture_type, 99, -31);  // Direct typed child is the background role.
      const auto control = f.add(row, control_type, -7, 13);
      f.add(row, text_type, 500, -500);  // Non-rendering row labels are irrelevant.
      for (std::size_t picture = 0; picture <= row_index % 2; ++picture)
        f.add(control, picture_type, static_cast<float>(picture), 0);
    }
  };
  // A structurally valid but semantically unrelated overlay precedes the
  // registered route in directory order.
  const auto unrelated_overlay = f.add({}, overlay_type, -800, 700);
  f.directory[unrelated_overlay].buf_name_offset = 0x1111U;
  add_rows(unrelated_overlay);
  const auto registered_owner = f.add({}, window_type, 12, 34);
  f.registered_window_owner_sources.push_back(registered_owner);
  const auto intermediate_group = f.add(registered_owner, group_type, 12, 34);
  const auto container = f.add(intermediate_group, overlay_type, 12, 34);
  f.overlay_index = container;
  add_rows(container);
  return f;
}

off::data::PictureDrawPlan plan(std::size_t group_count,
                                std::size_t image_bias = 0) {
  std::vector<off::data::PictureResourceDescriptor> descriptors(group_count);
  std::vector<off::data::PictureDrawGroup> groups;
  std::vector<off::data::PictureTextureBinding> bindings;
  for (std::size_t i = 0; i < group_count; ++i) {
    auto& descriptor = descriptors[i];
    descriptor.horizontal_edge_span = 2.0F;
    descriptor.vertical_edge_span = 2.0F;
    descriptor.modulation_color = 0xffffffffU;
    groups.push_back({1, i});
    bindings.push_back({0, static_cast<std::uint16_t>(2048 + i),
                        static_cast<std::uint16_t>(i), image_bias + i,
                        off::data::TextureManagerKeyBank::upper});
  }
  return off::data::PictureDrawPlan::build(descriptors, groups, bindings);
}

off::data::PictureDrawPlan multi_descriptor_plan(std::size_t image_index) {
  std::vector<off::data::PictureResourceDescriptor> descriptors(2);
  for (auto& descriptor : descriptors) {
    descriptor.horizontal_edge_span = 2.0F;
    descriptor.vertical_edge_span = 2.0F;
    descriptor.modulation_color = 0xffffffffU;
  }
  const std::array groups{off::data::PictureDrawGroup{2, 0}};
  const std::array bindings{off::data::PictureTextureBinding{
      0, 2048, 9, image_index, off::data::TextureManagerKeyBank::upper}};
  return off::data::PictureDrawPlan::build(descriptors, groups, bindings);
}

std::vector<off::data::StartupGraphicsRowComposition> valid_rows() {
  std::vector<off::data::StartupGraphicsRowComposition> rows(3);
  std::size_t next_picture = 200;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    auto& row = rows[i];
    row.owner_directory_index = 100 + i;
    row.slot_y = static_cast<float>(10 + i * 10);
    row.same_slot_multiplicity = 1;
    row.same_slot_ordinal = 0;
    row.authored_hidden = i == 2;
    row.construction_chain = {row.owner_directory_index};
    row.transform_chain = {{row.owner_directory_index, {}, {}}};
    const auto picture_count = 1U + (i % 2U);
    for (std::size_t picture_index = 0; picture_index <= picture_count; ++picture_index) {
      off::data::StartupGraphicsCompositionInstance picture;
      picture.role = picture_index == 0
          ? off::data::StartupGraphicsCompositionRole::row_background
          : off::data::StartupGraphicsCompositionRole::row_chrome;
      picture.directory_index = next_picture++;
      picture.construction_chain = {row.owner_directory_index, picture.directory_index};
      picture.transform_chain = {{row.owner_directory_index, {}, {}},
                                 {picture.directory_index, {}, {}}};
      picture.authored_state_mask = picture_index == 0 ? 0x80U : 0x01U;
      picture.base_render_property = static_cast<std::uint32_t>(0x1200U + i * 8U + picture_index);
      picture.authored_alpha = static_cast<std::uint8_t>(180U + picture_index);
      picture.alignment_enum = static_cast<std::uint8_t>(picture_index + 1U);
      picture.draw_plan = i == 0 && picture_index == 1
          ? multi_descriptor_plan(12)
          : plan(picture_index == 0 ? 1U : picture_index + 1U,
                 10U + picture_index * 3U);
      row.pictures.push_back(std::move(picture));
    }
  }
  return rows;
}

bool rejects_with_owner(const Fixture& fixture,
                        std::span<const std::size_t> owner_sources) {
  try {
    static_cast<void>(off::data::StartupGraphicsComposition::locate(
        fixture.directory, fixture.hierarchy, owner_sources));
    return false;
  } catch (const std::runtime_error&) {
    return true;
  }
}

bool rejects(const Fixture& fixture) {
  return rejects_with_owner(fixture, fixture.registered_window_owner_sources);
}

bool rejects_rows(std::vector<off::data::StartupGraphicsRowComposition> rows) {
  try {
    static_cast<void>(off::data::StartupGraphicsComposition::from_rows(std::move(rows)));
    return false;
  } catch (const std::runtime_error&) {
    return true;
  }
}
}  // namespace

int main() {
  auto fixture = valid_fixture();
  const auto located = off::data::StartupGraphicsComposition::locate(
      fixture.directory, fixture.hierarchy,
      fixture.registered_window_owner_sources);
  check(located.size() == 3 && located[1].chrome_directory_indexes.size() == 2,
        "discover variable row and chrome populations from typed hierarchy");
  check(located[0].background_directory_index !=
            located[0].chrome_directory_indexes.front(),
        "keep direct background and nested chrome identities distinct");

  auto moved_geometry = fixture;
  moved_geometry.directory[fixture.overlay_index].position[0] = -123.5F;
  moved_geometry.directory[fixture.overlay_index].position[1] = 4.25F;
  check(off::data::StartupGraphicsComposition::locate(
            moved_geometry.directory, moved_geometry.hierarchy,
            moved_geometry.registered_window_owner_sources).size() == located.size(),
        "locate typed roles without depending on observed screen coordinates");
  auto non_finite = fixture;
  non_finite.directory.back().position[0] = std::numeric_limits<float>::infinity();
  check(rejects(non_finite), "reject non-finite coordinates globally");

  auto malformed = fixture;
  const auto first_row = malformed.hierarchy[malformed.overlay_index]
                             .children_in_directory_order.front();
  const auto control = malformed.hierarchy[first_row].children_in_directory_order[1];
  const auto chrome = malformed.hierarchy[control].children_in_directory_order.front();
  malformed.directory[chrome].source_type = text_type;
  check(rejects(malformed), "reject a control with an unsupported child type");
  auto ambiguous = fixture;
  const auto offset = ambiguous.directory.size();
  for (const auto& entry : fixture.directory) ambiguous.directory.push_back(entry);
  for (const auto& node : fixture.hierarchy) {
    auto copy = node;
    copy.directory_index += offset;
    if (copy.parent_directory_index) *copy.parent_directory_index += offset;
    for (auto& child : copy.children_in_directory_order) child += offset;
    ambiguous.hierarchy.push_back(std::move(copy));
  }
  for (std::size_t index = offset; index < ambiguous.directory.size(); ++index)
    ambiguous.directory[index].buf_name_offset += 0x1000U;
  ambiguous.directory[offset + fixture.overlay_index].position[1] = -900.0F;
  check(off::data::StartupGraphicsComposition::locate(
            ambiguous.directory, ambiguous.hierarchy,
            fixture.registered_window_owner_sources).size() == located.size(),
        "ignore renamed, differently positioned structural distractors outside the registered owner");
  auto ambiguous_registered_owners = fixture.registered_window_owner_sources;
  ambiguous_registered_owners.push_back(
      offset + fixture.registered_window_owner_sources.front());
  check(rejects_with_owner(ambiguous, ambiguous_registered_owners),
        "reject duplicated semantic route owners even with structurally valid overlays");
  const std::array wrong_owner{fixture.overlay_index};
  check(rejects_with_owner(fixture, wrong_owner),
        "reject a semantic owner with the wrong source type");
  const std::array missing_owner{fixture.directory.size()};
  check(rejects_with_owner(fixture, missing_owner),
        "reject an out-of-range semantic owner identity");
  check(rejects_with_owner(fixture, std::span<const std::size_t>{}),
        "reject a missing semantic route owner");
  auto bad_parent = fixture;
  bad_parent.hierarchy[1].parent_directory_index = 1;
  check(rejects(bad_parent), "reject non-preceding hierarchy parent");

  auto rows = valid_rows();
  const auto composition = off::data::StartupGraphicsComposition::from_rows(rows);
  rows[0].slot_y = 999.0F;
  check(composition.rows()[0].slot_y == 10.0F,
        "factory owns row values independently of input storage");
  const auto visible = composition.visible_pictures(0x01U);
  check(visible.pictures.size() == 5,
        "visibility follows the locally authored row and picture set");
  for (const auto& picture : visible.pictures) {
    const auto& row = composition.rows()[picture.row_index];
    check(picture.row_directory_index == row.owner_directory_index &&
              picture.picture_directory_index == row.pictures[picture.picture_index].directory_index,
          "visibility retains local row and picture identities");
  }
  const auto active = composition.visible_pictures(0x08U);
  check(active.pictures.size() == 2 &&
            std::ranges::all_of(active.pictures, [](const auto& picture) {
              return picture.role == off::data::StartupGraphicsCompositionRole::row_background;
            }),
        "active state retains visible persistent backgrounds");
  const auto fallback = composition.visible_pictures(0x04U);
  check(fallback.effective_state == 0x01U && fallback.pictures.size() == visible.pictures.size(),
        "unsupported state falls back to locally defined resting visibility");

  const auto traversal = composition.traversal_emission_plan(0x01U);
  check(traversal.pictures.size() == visible.pictures.size() &&
            traversal.draw_emissions.size() > traversal.pictures.size(),
        "expand each local descriptor into an ordered draw emission");
  check(traversal.pictures.front().row_index == 1 &&
            traversal.pictures.front().role ==
                off::data::StartupGraphicsCompositionRole::row_chrome &&
            traversal.pictures[1].role ==
                off::data::StartupGraphicsCompositionRole::row_chrome &&
            traversal.pictures[2].role ==
                off::data::StartupGraphicsCompositionRole::row_background,
        "order rows by local directory hierarchy and emit typed chrome before background");
  for (const auto& picture : traversal.pictures) {
    for (std::size_t relative = 0; relative < picture.draw_emission_count; ++relative) {
      const auto& emission = traversal.draw_emissions[picture.first_draw_emission + relative];
      check(emission.row_index == picture.row_index &&
                emission.picture_index == picture.picture_index &&
                emission.picture_directory_index == picture.picture_directory_index,
            "each descriptor emission retains its local picture identity");
    }
  }

  auto bad_role = valid_rows();
  bad_role[0].pictures[0].role = off::data::StartupGraphicsCompositionRole::row_chrome;
  check(rejects_rows(std::move(bad_role)), "reject an invalid role binding");
  auto bad_mask = valid_rows();
  bad_mask[0].pictures[1].authored_state_mask = 0;
  check(rejects_rows(std::move(bad_mask)), "reject an empty authored state mask");
  auto duplicate_picture = valid_rows();
  duplicate_picture[1].pictures[0].directory_index =
      duplicate_picture[0].pictures[0].directory_index;
  check(rejects_rows(std::move(duplicate_picture)), "reject duplicate picture identities");
  auto bad_slot = valid_rows();
  bad_slot[1].same_slot_ordinal = 1;
  check(rejects_rows(std::move(bad_slot)), "reject inconsistent local slot ordinal");
  auto bad_chain = valid_rows();
  bad_chain[0].pictures[1].transform_chain.pop_back();
  check(rejects_rows(std::move(bad_chain)), "reject inconsistent transform chain");
  auto empty_group = valid_rows();
  empty_group[0].pictures[1].draw_plan = {};
  check(rejects_rows(std::move(empty_group)), "reject pictures without draw data");
  std::cout << "startup graphics composition tests passed\n";
}
