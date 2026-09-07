#include "off/platform/intro_picture_submission.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char *message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <class F> void rejects(F &&action) {
  bool rejected{};
  try { action(); } catch (const std::runtime_error &) { rejected = true; }
  check(rejected, "expected invalid picture submission rejection");
}
}

int main() {
  using namespace off;
  data::PictureQuad quad{};
  quad.local_z = 2.0F;
  quad.horizontal_edge_span = quad.vertical_edge_span = 2.0F;
  quad.u_max = quad.v_min = 1.0F;
  quad.modulation_color = 0xffffffffU;
  data::BoundPictureDrawGroup red{{.image_index = 7}, {quad}};
  data::BoundPictureDrawGroup green{{.image_index = 9}, {quad}};
  const std::array groups{red, green};
  platform::IntroPictureSubmissionInput input{
      .groups = groups,
      .transform = {.basis = {0, 0, 1, 0, 1, 0, 1, 0, 0}},
      .projection = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      .viewport = {0, 0, 640, 480, 0, 1},
      .scissor = {0, 0, 640, 480}};
  const auto submission = platform::IntroPictureSubmission::assemble(input);
  check(submission.draws().size() == 2 &&
            submission.draws()[0].catalog_image_index == 7 &&
            submission.draws()[1].catalog_image_index == 9 &&
            submission.draws()[0].batches.size() == 1 &&
            submission.draws()[0].batches[0].vertices.size() == 4,
        "submission preserves group order, retail image identities and expanded geometry");
  auto empty = input;
  empty.groups = {};
  rejects([&] { static_cast<void>(platform::IntroPictureSubmission::assemble(empty)); });
  auto empty_group = input;
  data::BoundPictureDrawGroup no_quads{{.image_index = 7}, {}};
  empty_group.groups = std::span(&no_quads, 1);
  rejects([&] { static_cast<void>(platform::IntroPictureSubmission::assemble(empty_group)); });
  return 0;
}
