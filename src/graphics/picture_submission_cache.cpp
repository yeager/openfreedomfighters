#include "off/graphics/picture_submission_cache.hpp"

#include <cmath>
#include <stdexcept>

namespace off::graphics {

void PictureSubmissionCache::submit(GroupTable groups,
                                    const PictureCacheTransformInput &input,
                                    std::uint32_t control,
                                    const Visitor &visitor) {
  if (!groups)
    return;
  if (submitting_)
    throw std::runtime_error("Picture cache submission cannot be reentrant");
  if (!groups->empty() && !visitor)
    throw std::runtime_error(
        "Picture cache submission requires a group visitor");
  for (float value : input.submission_position)
    if (!std::isfinite(value))
      throw std::runtime_error("Picture cache position must be finite");
  struct SubmissionGuard {
    bool &active;
    ~SubmissionGuard() { active = false; }
  } guard{submitting_};
  submitting_ = true;
  if (dirty_ || !state_ || state_->position != input.submission_position) {
    dirty_ = true;
    const auto prepared = prepare_picture_cache_transform(input);
    state_ = PictureSubmissionCachedState{input.submission_position, prepared};
    dirty_ = false;
  }
  const auto transform_snapshot = state_->transform;
  for (std::size_t index = 0; index < groups->size(); ++index)
    visitor(index, (*groups)[index], transform_snapshot, control);
}

} // namespace off::graphics
