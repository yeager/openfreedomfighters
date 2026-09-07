#include "off/graphics/picture_draw_order.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
off::data::PictureTextureBinding resource(std::uint16_t image_id) {
  off::data::PictureTextureBinding value;
  value.texture_id = image_id;
  value.image_index = 999;
  value.authored_texture_resource_record[1] = std::byte{1};
  value.authored_texture_resource_record[2] = std::byte{2};
  return value;
}
}

int main() {
  auto selected = resource(0x345);
  check(make_picture_order_key(1, 7, selected) == 0x083eb452U,
        "compose checked marker category/subtype with current view/control and selected image ID");
  for (std::uint8_t view : {std::uint8_t{0}, std::uint8_t{15}, std::uint8_t{16}, std::uint8_t{255}}) {
    const auto key = make_picture_order_key(view, 255, selected);
    const auto wide = (static_cast<std::uint64_t>(view) << 27U) |
                      (std::uint64_t{255} << 19U) | (std::uint64_t{13} << 15U) |
                      (std::uint64_t{0x345} << 4U) | 2U;
    check(key == static_cast<std::uint32_t>(wide), "key truncates unsigned overflow without rejecting fifth view bit");
    check((key & 0x78000000U) == ((static_cast<std::uint32_t>(view) & 15U) << 27U),
          "consumer view field remains narrower than full unsigned ordering key");
  }
  const auto original = make_picture_order_key(1, 7, selected);
  selected.image_index = 0; selected.prm_offset = 9999; selected.manager_key = 77;
  check(make_picture_order_key(1, 7, selected) == original, "catalog location and paired pointer/key do not replace selected ID");
  selected.texture_id += 0x800;
  check(make_picture_order_key(1, 7, selected) == original, "selected image binding field retains only low eleven bits");
  selected.texture_id = std::numeric_limits<std::uint16_t>::max();
  check((make_picture_order_key(1, 7, selected) & 0x7ff0U) == 0x7ff0U, "selected lower ID preserves full supported word before mask");
  for (std::size_t byte = 0; byte < 4; ++byte) {
    auto invalid = selected;
    invalid.authored_texture_resource_record[byte] ^= std::byte{0x10};
    rejects([&] { (void)make_picture_order_key(1, 7, invalid); }, "unsupported source marker mutation rejects");
  }
  check(merge_picture_draw_order({}, {}).empty(), "empty partitions remain empty");
  const std::array<PictureKeyedRecord, 4> retained{{{10, 0}, {11, 7}, {12, 7}, {13, 0xffffffffU}}};
  const std::array<PictureKeyedRecord, 6> rebuilt{{{20, 7}, {21, 0x80000000U}, {22, 7}, {23, 0}, {24, 0x7fffffffU}, {20, 7}}};
  const auto merged = merge_picture_draw_order(retained, rebuilt);
  constexpr std::array<std::uint64_t, 10> expected{23, 10, 20, 22, 20, 11, 12, 24, 21, 13};
  check(merged.size() == expected.size(), "every record retained, including equal keys and repeated identities");
  for (std::size_t i = 0; i < merged.size(); ++i) {
    check(merged[i].identity == expected[i] && merged[i].slot_index == i,
          "stable-new native policy, new-before-retained equality and resulting slot assignment");
    if (i) check(merged[i-1].key <= merged[i].key, "full unsigned key orders bit31 correctly");
  }
  check(rebuilt[0].identity == 20 && rebuilt[1].identity == 21 && retained[1].identity == 11,
        "ordering never mutates input partitions");
  const auto only_old = merge_picture_draw_order(retained, {});
  const auto only_new = merge_picture_draw_order({}, rebuilt);
  check(only_old.size() == 4 && only_old[2].identity == 12 && only_old[3].slot_index == 3,
        "retained-only order and slots preserved");
  check(only_new.size() == 6 && only_new[0].identity == 23 && only_new[5].identity == 21,
        "new-only list uses unsigned order");
  const std::array<PictureKeyedRecord, 2> invalid{{{1, 0x80000000U}, {2, 0x7fffffffU}}};
  rejects([&] { (void)merge_picture_draw_order(invalid, rebuilt); }, "unsorted retained partition rejects rather than silently rebuilding it");
  {
    PictureRecordRebuild route;
    std::vector<std::string> events;
    PictureRecordRebuildHooks hooks{
      [&](const auto& record) { events.push_back("prepare" + std::to_string(record.record_identity)); },
      [&](const auto& record) { events.push_back("register" + std::to_string(record.record_identity)); },
      [](const auto& record) -> std::optional<PictureCurrentDrawView> {
        if (record.record_identity == 101) return PictureCurrentDrawView{71, 2};
        return std::nullopt;
      }};
    const PictureQueuedDrawRecord first{101, 201, 301, resource(0x120), 4};
    const PictureQueuedDrawRecord second{102, 202, 302, resource(0x110), 8};
    route.accept(first, hooks);
    route.accept(second, hooks);
    const std::array<PictureOrderedDrawEntry, 1> old{{
      {make_picture_order_key(0, 1, resource(0x115)), 99, std::nullopt, 399}}};
    const auto route_entries = route.rebuild(old, hooks);
    check(events == std::vector<std::string>{"prepare101", "register101", "prepare102", "register102"} &&
      route.queued_count() == 0 && route_entries.size() == 3 &&
      route_entries[0].record_identity == 99 && route_entries[1].record_identity == 102 &&
      !route_entries[1].associated_view && route_entries[2].record_identity == 101 &&
      route_entries[2].associated_view == 71 && route_entries[2].resource == 301,
      "source-backed queued records prepare/register before rebuild and preserve live view/resource association");
  }
  {
    PictureRecordRebuild route;
    auto hooks = PictureRecordRebuildHooks{
      [](const auto&) {}, [](const auto&) {},
      [](const auto&) -> std::optional<PictureCurrentDrawView> { return {}; }};
    rejects([&] { route.accept({0, 2, 3, resource(1), 0}, hooks); },
      "missing record identity rejects before preparation");
    rejects([&] { route.accept({1, 2, 3, resource(1), 0}, {}); },
      "incomplete record hooks reject before preparation");
    route.accept({1, 2, 3, resource(1), 0}, hooks);
    const std::array<PictureOrderedDrawEntry, 1> duplicate{{{2, 1, {}, 3}}};
    rejects([&] { (void)route.rebuild(duplicate, hooks); },
      "ambiguous retained and queued record identity rejects without clearing queue");
    check(route.queued_count() == 1 && !route.poisoned(),
      "rebuild validation retains queued record and leaves route reusable");
  }
  {
    PictureRecordRebuild route;
    const auto accept_hooks = PictureRecordRebuildHooks{
      [](const auto&) {}, [](const auto&) {},
      [](const auto&) -> std::optional<PictureCurrentDrawView> { return {}; }};
    route.accept({1, 2, 3, resource(1), 0}, accept_hooks);
    const auto view_only_hooks = PictureRecordRebuildHooks{
      {}, {}, [](const auto&) -> std::optional<PictureCurrentDrawView> { return {}; }};
    check(route.rebuild({}, view_only_hooks).size() == 1,
      "rebuild requires only live view resolution after record acceptance");
  }
  {
    PictureRecordRebuild route;
    auto hooks = PictureRecordRebuildHooks{
      [](const auto&) { throw std::runtime_error("registration failure"); },
      [](const auto&) {}, [](const auto&) -> std::optional<PictureCurrentDrawView> { return {}; }};
    rejects([&] { route.accept({1, 2, 3, resource(1), 0}, hooks); },
      "record preparation failure aborts acceptance");
    check(route.poisoned() && route.queued_count() == 0,
      "callback failure poisons route without publishing its record");
  }
  return failures == 0 ? 0 : 1;
}
