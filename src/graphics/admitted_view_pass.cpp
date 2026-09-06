#include "off/graphics/admitted_view_pass.hpp"

#include <bit>
#include <stdexcept>

namespace off::graphics {
namespace {
std::int32_t key(const AdmittedView& view) {
    return std::bit_cast<std::int32_t>(0U - view.priority);
}
}

AdmittedViewPass::AdmittedViewPass(std::span<const AdmittedView> insertion_order) {
    if (insertion_order.size() > 16U)
        throw std::runtime_error("admitted view capacity exceeded");
    views_.assign(insertion_order.begin(), insertion_order.end());
    for (std::size_t index = 0; index < views_.size(); ++index) {
        auto position = order_.begin();
        while (position != order_.end() && key(views_[*position]) >= key(views_[index])) ++position;
        order_.insert(position, index);
    }
}

void AdmittedViewPass::run(std::span<const ViewCameraState> cameras,
                           const std::function<void()>& frame_begin, const Visitor& visitor) {
    if (running_ || cameras.size() != views_.size() || !frame_begin || !visitor)
        throw std::runtime_error("admitted view pass input or reentrancy is unsupported");
    struct Guard {
        bool& value;
        explicit Guard(bool& flag) : value(flag) { value = true; }
        ~Guard() { value = false; }
    } guard(running_);
    frame_begin();
    for (const auto index : order_) {
        const auto& camera = cameras[index];
        if (!camera.camera_identity || !camera.enabled) continue;
        visitor(Stage::begin, views_[index], camera, index);
        visitor(Stage::traverse, views_[index], camera, index);
        visitor(Stage::end, views_[index], camera, index);
    }
}

} // namespace off::graphics
