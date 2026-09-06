#pragma once

#include "off/graphics/picture_submission_cache.hpp"

#include <array>
#include <cstdint>
#include <functional>

namespace off::graphics {

// Conditional size state for one picture owner and its paired submission cache.
// Construction supplies recovered size defaults, not a prepared transform.
class FadePictureSize final {
public:
    FadePictureSize() = default;
    FadePictureSize(const FadePictureSize&) = delete;
    FadePictureSize& operator=(const FadePictureSize&) = delete;
    FadePictureSize(FadePictureSize&&) = delete;
    FadePictureSize& operator=(FadePictureSize&&) = delete;
    using InvalidationHook = std::function<void()>;

    // Explicit engine dimensions, not texture extents or SDL window dimensions.
    // Positive inputs and ordinary nearest-even binary32 conversion are native
    // policies. Changes commit size and dirty cache before synchronous notification;
    // hook failure does not roll them back. Unchanged sizes do not notify.
    // Caller must supply this owner's cache; no reentry or owner destruction.
    void initialize(std::int32_t engine_width, std::int32_t engine_height,
                    PictureSubmissionCache& cache, const InvalidationHook& hook);
    [[nodiscard]] const std::array<float, 2>& scale() const noexcept { return scale_; }

private:
    std::array<float, 2> scale_{1.0F, 1.0F};
    bool initializing_{false};
};

} // namespace off::graphics
