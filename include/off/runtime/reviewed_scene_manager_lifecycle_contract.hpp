#pragma once

#include <filesystem>
#include <optional>

namespace off::runtime {

// The only public fact represented by a reviewed scene-manager evidence
// receipt. It intentionally has no scene identity, callback, resource,
// service, timing, or activation authority.
enum class SceneManagerLifecycleReceipt {
  repeated_success_with_distinct_failure,
};

// A private, source-free receipt emitted by
// scene_manager_lifecycle_contract_bundle.py. Loading it is deliberately
// inert: no scene, component, camera, rendering, input, or startup code
// consumes this type.
class ReviewedSceneManagerLifecycleContract final {
 public:
  [[nodiscard]] static std::optional<ReviewedSceneManagerLifecycleContract>
  load_local(const std::filesystem::path& local_directory);

  [[nodiscard]] bool admitted() const noexcept { return true; }
  [[nodiscard]] SceneManagerLifecycleReceipt receipt() const noexcept { return receipt_; }

 private:
  explicit ReviewedSceneManagerLifecycleContract(SceneManagerLifecycleReceipt receipt)
      : receipt_(receipt) {}

  SceneManagerLifecycleReceipt receipt_;
};

}  // namespace off::runtime
