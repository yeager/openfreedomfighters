#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace off::graphics {

// A private, source-free receipt for a separately reviewed MovieControl to
// first-cut lifecycle observation.  This is deliberately inert: it carries no
// callback, scene, player, rendering, timing, or dispatch service and cannot
// enable normal startup by itself.
class ReviewedMovieControlLifecycleContract final {
 public:
  [[nodiscard]] static std::optional<ReviewedMovieControlLifecycleContract>
  load_local(const std::filesystem::path& local_directory);

  // A successful load proves only that the exact v1 structural contract was
  // independently reviewed.  Future runtime work must still bind it through a
  // separately reviewed behavior specification.
  [[nodiscard]] bool admitted() const noexcept { return true; }

 private:
  explicit ReviewedMovieControlLifecycleContract(std::uint64_t receipt)
      : receipt_(receipt) {}
  std::uint64_t receipt_{};
};

}  // namespace off::graphics
