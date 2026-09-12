#pragma once

#include <filesystem>
#include <optional>

namespace off::graphics {

// A private, source-free receipt for an independently reviewed startup
// coordinator pass. It intentionally carries no source identity, callback,
// work item, rendering service, or runtime handle.
class ReviewedStartupCoordinatorPassContract final {
 public:
  [[nodiscard]] static std::optional<ReviewedStartupCoordinatorPassContract>
  load_local(const std::filesystem::path& local_directory);

  // Admission says only that the fixed structural receipt was reviewed. It is
  // not authority to run a coordinator pass or render a startup scene.
  [[nodiscard]] bool admitted() const noexcept { return true; }

 private:
  explicit ReviewedStartupCoordinatorPassContract(bool receipt) : receipt_(receipt) {}
  bool receipt_{};
};

}  // namespace off::graphics
