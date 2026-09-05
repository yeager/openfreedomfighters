#include "off/graphics/picture_expansion.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int failures{};
void check(bool value, const char *message) {
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
template <class Function> void rejects(Function function, const char *message) {
  try {
    function();
    check(false, message);
  } catch (const std::runtime_error &) {
  }
}
} // namespace

int main() {
  using namespace off::graphics;
  const PictureCacheTransform identity{.basis = {0, 0, 1, 0, 1, 0, 1, 0, 0}};
  off::data::PictureQuad quad{.local_z = 3,
                              .u_min = 0.8F,
                              .u_max = 0.2F,
                              .v_max = -0.3F,
                              .v_min = 0.9F,
                              .modulation_color = 0xff030581U,
                              .local_center_x = 2,
                              .local_center_y = 4,
                              .horizontal_edge_span = 6,
                              .vertical_edge_span = 8};
  const auto expand = [&](const auto &q, const auto &t) {
    return expand_picture_descriptors(std::span(&q, 1), t);
  };
  const auto simple = expand(quad, identity);
  const std::array<std::array<float, 3>, 4> positions{
      {{-1, 0, 3}, {5, 0, 3}, {5, 8, 3}, {-1, 8, 3}}};
  const std::array<std::array<float, 2>, 4> uv{
      {{0.8F, -0.3F}, {0.2F, -0.3F}, {0.2F, 0.9F}, {0.8F, 0.9F}}};
  for (std::size_t i = 0; i < 4; ++i) {
    check(simple[0].vertices[i].position == positions[i],
          "ordered axis-aligned corners");
    check(simple[0].vertices[i].uv == uv[i],
          "preserve asymmetric reversed UV endpoints");
    check(simple[0].vertices[i].color == 0x7f010240U,
          "halve each packed channel independently");
  }
  check(simple[0].indices == std::vector<std::uint16_t>{0, 1, 3, 1, 2, 3},
        "quad topology");
  const PictureCacheTransform shear{.basis = {1, 2, 3, 0, 3, 4, -2, 0, 0},
                                    .translation = {10, 20, 30}};
  const auto changed = expand(quad, shear);
  // C=(9,38,55), basis lengths 2 and 5: rotation/shear does not rotate corners.
  check(changed[0].vertices[0].position == std::array<float, 3>{3, 18, 55},
        "reversed basis and reflected horizontal span");
  check(changed[0].vertices[2].position == std::array<float, 3>{15, 58, 55},
        "shear expands axis-aligned around transformed center");
  const auto prepared =
      prepare_picture_cache_transform({.submission_position = {0.5F, 1.5F, 4},
                                       .aligned_local_position = {50, 40, 0},
                                       .virtual_window_scale = {7, 9, 1, 1},
                                       .cached_basis = identity.basis,
                                       .object_matrix = identity.basis,
                                       .viewport_width = 100,
                                       .viewport_height = 80,
                                       .picture_width = 20,
                                       .picture_height = 10,
                                       .owner_projection_scalar = 2,
                                       .external_y_basis_scale = 2});
  const auto adapted = expand(quad, prepared);
  check(adapted[0].vertices[0].position[0] == -1 &&
            std::abs(adapted[0].vertices[0].position[1] - (-7.9F)) < 0.00001F &&
            adapted[0].vertices[0].position[2] == 13,
        "consume prepared basis and translation without another Y inversion");
  auto precise = quad;
  precise.local_center_x = 16777216.0F;
  precise.local_center_y = 1;
  precise.local_z = 0;
  precise.horizontal_edge_span = 0;
  precise.vertical_edge_span = 0;
  auto rounding = identity;
  rounding.basis[3] = 1;
  rounding.translation[0] = -16777216.0F;
  check(expand(precise, rounding)[0].vertices[0].position[0] == 0,
        "materialize linear center as binary32 before translation");
  precise.local_center_x = 16777216.0F;
  precise.horizontal_edge_span = 1;
  auto tiny = identity;
  tiny.basis[6] = 0.5F;
  tiny.translation[0] = -8388608.0F;
  check(expand(precise, tiny)[0].vertices[0].position[0] == -0.25F,
        "authored span survives collapsed rounded bounds");
  check(expand_picture_descriptors({}, identity).empty(),
        "empty expansion has no batches");
  for (std::size_t count : {2048U, 2049U, 4096U}) {
    std::vector<off::data::PictureQuad> many(count, quad);
    for (std::size_t i = 0; i < count; ++i) {
      many[i].local_center_x = static_cast<float>(i);
      many[i].descriptor_index =
          count - i; // Resource identity does not reorder.
    }
    const auto result = expand_picture_descriptors(many, identity);
    check(result.size() == (count + 2047) / 2048, "batch count at boundary");
    for (const auto &batch : result) {
      check(batch.indices.front() == 0, "indices restart per batch");
      check(batch.vertices.size() <= 8192, "bounded batch vertex count");
      for (std::size_t i = 0; i < batch.vertices.size() / 4; ++i) {
        check(batch.vertices[i * 4].position[0] ==
                  static_cast<float>(batch.first_descriptor + i) - 3,
              "global offset and authored order preserved");
        check(batch.indices[i * 6 + 5] == i * 4 + 3,
              "batch-local indices remain in range");
      }
    }
  }
  rejects(
      [&] {
        static_cast<void>(expand_picture_descriptors(
            std::vector<off::data::PictureQuad>(4097), identity));
      },
      "reject descriptor overflow");
  for (float hostile : {std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    for (auto member :
         {&off::data::PictureQuad::local_center_x,
          &off::data::PictureQuad::local_center_y,
          &off::data::PictureQuad::local_z, &off::data::PictureQuad::u_min,
          &off::data::PictureQuad::u_max, &off::data::PictureQuad::v_min,
          &off::data::PictureQuad::v_max,
          &off::data::PictureQuad::horizontal_edge_span,
          &off::data::PictureQuad::vertical_edge_span}) {
      auto bad = quad;
      bad.*member = hostile;
      rejects([&] { static_cast<void>(expand(bad, identity)); },
              "reject non-finite descriptor input");
    }
    for (std::size_t i = 0; i < 12; ++i) {
      auto bad = identity;
      (i < 9 ? bad.basis[i] : bad.translation[i - 9]) = hostile;
      rejects([&] { static_cast<void>(expand(quad, bad)); },
              "reject non-finite transform input");
    }
  }
  for (auto member : {&off::data::PictureQuad::horizontal_edge_span,
                      &off::data::PictureQuad::vertical_edge_span}) {
    auto bad = quad;
    bad.*member = -1;
    rejects([&] { static_cast<void>(expand(bad, identity)); },
            "reject negative span");
  }
  auto huge = identity;
  huge.basis[6] = std::numeric_limits<float>::max();
  rejects([&] { static_cast<void>(expand(quad, huge)); },
          "reject linear center overflow before conversion");
  auto bad = quad;
  bad.local_center_x = 0;
  rejects([&] { static_cast<void>(expand(bad, huge)); },
          "reject corner overflow");
  bad.local_center_x = std::numeric_limits<float>::max();
  huge = identity;
  huge.translation[0] = std::numeric_limits<float>::max();
  rejects([&] { static_cast<void>(expand(bad, huge)); },
          "reject translated center overflow");
  return failures == 0 ? 0 : 1;
}
