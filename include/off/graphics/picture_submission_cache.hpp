#pragma once

#include "off/data/picture_texture_binding.hpp"
#include "off/graphics/picture_transform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace off::graphics {

struct PictureSubmissionCachedState {
  std::array<float, 3> position;
  PictureCacheTransform transform;
};

class PictureSubmissionCache final {
public:
  PictureSubmissionCache() = default;
  PictureSubmissionCache(const PictureSubmissionCache &) = delete;
  PictureSubmissionCache &operator=(const PictureSubmissionCache &) = delete;
  PictureSubmissionCache(PictureSubmissionCache &&) = delete;
  PictureSubmissionCache &operator=(PictureSubmissionCache &&) = delete;
  using GroupTable =
      std::optional<std::span<const data::BoundPictureDrawGroup>>;
  using Visitor =
      std::function<void(std::size_t, const data::BoundPictureDrawGroup &,
                         const PictureCacheTransform &, std::uint32_t)>;

  void invalidate() noexcept { dirty_ = true; }
  [[nodiscard]] bool dirty() const noexcept { return dirty_; }
  [[nodiscard]] const std::optional<PictureSubmissionCachedState> &
  cached_state() const noexcept {
    return state_;
  }

  // Absent tables are a complete no-op; present empty tables still prepare.
  // Only finite submission position is automatically keyed, using numerical
  // equality (including signed zero). Other changed dependencies require an
  // explicit invalidate(); clean reuse does not validate those ignored inputs.
  // Replacement exception policy: invalid position/empty required visitor leave
  // state untouched; failed preparation retains old state but leaves dirty.
  // Successful preparation commits before ordered visitation. Visitor failure
  // propagates after its prefix and does not roll back committed cache state.
  // Callbacks receive current groups/control and a local transform snapshot.
  // invalidate() during visitation is retained. Reentrant present-table submit
  // on the same cache is rejected; this object is not thread-safe.
  void submit(GroupTable groups, const PictureCacheTransformInput &input,
              std::uint32_t control, const Visitor &visitor);

private:
  bool dirty_{true};
  bool submitting_{false};
  std::optional<PictureSubmissionCachedState> state_;
};

} // namespace off::graphics
