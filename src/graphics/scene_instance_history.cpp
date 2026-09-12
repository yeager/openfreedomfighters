#include "off/graphics/scene_instance_history.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace off::graphics {
namespace {

[[nodiscard]] bool finite(const SceneInstanceTransformSnapshot &value) noexcept {
  return std::ranges::all_of(value.source_basis, [](float item) { return std::isfinite(item); }) &&
         std::ranges::all_of(value.source_position, [](float item) { return std::isfinite(item); }) &&
         std::ranges::all_of(value.map_orientation, [](float item) { return std::isfinite(item); }) &&
         std::ranges::all_of(value.map_position, [](float item) { return std::isfinite(item); });
}

void validate(std::span<const SceneInstanceSubmissionTransform> instances) {
  std::map<std::uint64_t, bool> identities;
  for (const auto &instance : instances) {
    if (instance.identity == 0U || !finite(instance.current))
      throw std::runtime_error("scene instance history requires finite nonzero identities");
    if (!identities.emplace(instance.identity, true).second)
      throw std::runtime_error("scene instance history requires unique identities");
  }
}

[[nodiscard]] SceneInstanceTransformSnapshot
snapshot(const SceneRenderInstance &instance) noexcept {
  return {.source_basis = instance.source_basis,
          .source_position = instance.source_position,
          .map_orientation = instance.map_orientation,
          .map_position = instance.map_position};
}

[[nodiscard]] SceneInstanceTransformSnapshot
snapshot(const SceneGpuInstance &instance) noexcept {
  return {.source_basis = instance.source_basis,
          .source_position = instance.source_position,
          .map_orientation = instance.map_orientation,
          .map_position = instance.map_position};
}

} // namespace

std::vector<SceneInstanceSubmissionTransform>
make_initial_scene_instance_submission(std::span<const SceneRenderInstance> instances) {
  std::vector<SceneInstanceSubmissionTransform> result;
  result.reserve(instances.size());
  for (std::size_t index = 0; index < instances.size(); ++index)
    result.push_back({.identity = static_cast<std::uint64_t>(index) + 1U,
                      .current = snapshot(instances[index])});
  validate(result);
  return result;
}

std::vector<SceneInstanceSubmissionTransform>
make_scene_gpu_instance_submission(std::span<const SceneGpuInstance> instances) {
  std::vector<SceneInstanceSubmissionTransform> result;
  result.reserve(instances.size());
  for (const auto &instance : instances) {
    // scene_instance_index originates in SceneRenderAsset's canonical order;
    // retain it rather than using transient draw order as an identity.
    result.push_back({.identity =
                          static_cast<std::uint64_t>(instance.scene_instance_index) +
                              1U,
                      .current = snapshot(instance)});
  }
  validate(result);
  return result;
}

bool SceneInstanceHistoryLifecycle::initialize(
    std::span<const SceneInstanceSubmissionTransform> instances) {
  if (in_flight_)
    return false;
  validate(instances);
  committed_.clear();
  pending_.clear();
  initialized_ = true;
  history_valid_ = false;
  return true;
}

std::optional<std::vector<SceneInstanceSubmissionTransform>>
SceneInstanceHistoryLifecycle::begin_submission(
    std::span<const SceneInstanceSubmissionTransform> instances) {
  if (!initialized_ || in_flight_)
    return std::nullopt;
  validate(instances);
  pending_.clear();
  pending_.reserve(instances.size());
  for (const auto &instance : instances) {
    auto submitted = instance;
    const auto previous = committed_.find(instance.identity);
    if (history_valid_ && previous != committed_.end()) {
      submitted.previous = previous->second;
      submitted.previous_valid = true;
    } else {
      submitted.previous = instance.current;
      submitted.previous_valid = false;
    }
    pending_.push_back(submitted);
  }
  in_flight_ = true;
  return pending_;
}

bool SceneInstanceHistoryLifecycle::commit_submission() noexcept {
  if (!in_flight_)
    return false;
  committed_.clear();
  for (const auto &instance : pending_)
    committed_.emplace(instance.identity, instance.current);
  pending_.clear();
  history_valid_ = true;
  in_flight_ = false;
  return true;
}

void SceneInstanceHistoryLifecycle::cancel_submission() noexcept {
  pending_.clear();
  in_flight_ = false;
}

void SceneInstanceHistoryLifecycle::invalidate() noexcept {
  pending_.clear();
  in_flight_ = false;
  history_valid_ = false;
}

} // namespace off::graphics
