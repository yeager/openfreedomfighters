#include "off/graphics/scene_instance_history.hpp"

#include <iostream>

namespace {
int failures{};
void check(bool value, const char *message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

off::graphics::SceneInstanceSubmissionTransform item(std::uint64_t identity,
                                                      float position) {
  off::graphics::SceneInstanceSubmissionTransform result;
  result.identity = identity;
  result.current.source_basis = {1,0,0,0,1,0,0,0,1};
  result.current.map_orientation = {1,0,0,0,1,0,0,0,1};
  result.current.source_position = {position,0,0};
  return result;
}
} // namespace

int main() {
  using off::graphics::SceneInstanceHistoryLifecycle;
  SceneInstanceHistoryLifecycle history;
  const auto first = item(1, 3);
  check(!history.begin_submission({&first, 1}), "uninitialized history rejects submissions");
  check(history.initialize({&first, 1}), "initialize accepts a real transform snapshot");
  const auto initial = history.begin_submission({&first, 1});
  check(initial && initial->size() == 1 && !initial->front().previous_valid &&
            initial->front().previous == first.current,
        "first frame has an explicitly invalid self previous transform");
  check(!history.begin_submission({&first, 1}), "overlap is rejected");
  check(history.commit_submission(), "successful submission publishes history");
  const auto changed = item(1, 9);
  const auto next = history.begin_submission({&changed, 1});
  check(next && next->front().previous_valid &&
            next->front().previous.source_position[0] == 3 &&
            next->front().current.source_position[0] == 9,
        "next real submission observes the committed previous transform");
  history.cancel_submission();
  const auto retry = history.begin_submission({&changed, 1});
  check(retry && retry->front().previous_valid &&
            retry->front().previous.source_position[0] == 3,
        "cancel never publishes pending transforms");
  history.cancel_submission();
  history.invalidate();
  const auto cut = history.begin_submission({&changed, 1});
  check(cut && !cut->front().previous_valid &&
            cut->front().previous == changed.current,
        "invalidation prevents stale vectors after a discontinuity");
  history.cancel_submission();
  const auto duplicate = std::array{item(1, 0), item(1, 1)};
  bool rejected{};
  try { static_cast<void>(history.begin_submission(duplicate)); } catch (...) { rejected = true; }
  check(rejected, "duplicate stable identities are rejected");

  off::graphics::SceneGpuInstance planned_instance;
  planned_instance.scene_instance_index = 4;
  planned_instance.source_basis = {1,0,0,0,1,0,0,0,1};
  planned_instance.source_position = {7,8,9};
  planned_instance.map_orientation = {2,0,0,0,3,0,0,0,4};
  planned_instance.map_position = {10,11,12};
  const auto planned = off::graphics::make_scene_gpu_instance_submission(
      {&planned_instance, 1});
  check(planned.size() == 1 && planned.front().identity == 5 &&
            planned.front().current.source_position ==
                planned_instance.source_position &&
            planned.front().current.map_orientation ==
                planned_instance.map_orientation &&
            planned.front().current.map_position == planned_instance.map_position,
        "GPU plan history keeps canonical identity and authored records verbatim");
  return failures == 0 ? 0 : 1;
}
