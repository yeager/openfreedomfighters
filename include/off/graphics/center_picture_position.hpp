#pragma once

#include "off/graphics/picture_submission_cache.hpp"

#include <array>
#include <cstdint>
#include <functional>

namespace off::graphics {

// Conditional Center phase-one operation on one live picture. Position is the
// current runtime value, initially copied from its authored source, not zeroed.
class CenterPicturePosition final {
public:
    CenterPicturePosition() = default;
    CenterPicturePosition(const CenterPicturePosition&) = delete;
    CenterPicturePosition& operator=(const CenterPicturePosition&) = delete;
    CenterPicturePosition(CenterPicturePosition&&) = delete;
    CenterPicturePosition& operator=(CenterPicturePosition&&) = delete;

    // Explicit engine dimensions, not texture or viewport extents. Finite
    // position, positive dimensions, nearest rounding and no reentry/alias
    // mutation/destruction are native admission policies, except the service may
    // apply its declared runtime-flag effects. Status and flags
    // must be distinct words. A changed position requires a synchronous service.
    // That service sees committed position/flags before cache/status updates.
    // It must not throw on the admitted path; unexpected exceptions propagate
    // without rollback or forced cache/status writes. Equal position only marks
    // component status and needs no service. This does not admit lifecycle phase
    // one, implement hierarchy propagation, or add a once-only latch.
    void initialize(std::array<float, 3>& position, std::uint32_t& runtime_flags,
                    std::uint32_t& component_status, std::int32_t engine_width,
                    std::int32_t engine_height, PictureSubmissionCache& cache,
                    const std::function<void()>& update_service);
private:
    bool initializing_{false};
};

} // namespace off::graphics
