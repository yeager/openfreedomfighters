#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

struct AdmittedView {
    std::uint64_t identity;
    std::uint32_t priority;
};
struct ViewCameraState {
    std::optional<std::uint64_t> camera_identity;
    // Result of the admitted camera's runtime enabled query, not source flags.
    bool enabled;
};

// Conditional view phase, not registration or startup admission. Caller supplies
// immutable frame inputs and keeps associated views/cameras alive throughout.
class AdmittedViewPass final {
public:
    enum class Stage { begin, traverse, end };
    using Visitor = std::function<void(Stage, const AdmittedView&,
                                      const ViewCameraState&, std::size_t)>;
    explicit AdmittedViewPass(std::span<const AdmittedView> insertion_order);
    AdmittedViewPass(const AdmittedViewPass&) = delete;
    AdmittedViewPass& operator=(const AdmittedViewPass&) = delete;
    AdmittedViewPass(AdmittedViewPass&&) = delete;
    AdmittedViewPass& operator=(AdmittedViewPass&&) = delete;

    // Camera states correspond to original insertion indices. Keys are cached
    // at construction, not re-sorted from mutable camera priorities each frame.
    // Required frame_begin models explicit bookkeeping preparation; it runs even
    // for an empty pass. Exceptions retain external effects and do not synthesize
    // end hooks. Recursive run is rejected as native safety policy.
    void run(std::span<const ViewCameraState> cameras,
             const std::function<void()>& frame_begin, const Visitor& visitor);

private:
    std::vector<AdmittedView> views_;
    std::vector<std::size_t> order_;
    bool running_{false};
};

} // namespace off::graphics
