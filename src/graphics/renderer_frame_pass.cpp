#include "off/graphics/renderer_frame_pass.hpp"

#include <stdexcept>
#include <vector>

namespace off::graphics {

void RendererFramePass::run(bool renderer_admitted, bool backend_ready,
                            std::uint64_t renderer_identity,
                            std::span<const RendererStateEntry> states,
                            const RendererFrameHooks& hooks) {
    if (running_) throw std::runtime_error("renderer frame reentrancy is unsupported");
    if (!renderer_admitted) return;
    if (!hooks.state_frame || !hooks.state_maintenance || !hooks.backend_maintenance)
        throw std::runtime_error("renderer frame hooks are required");
    std::vector<std::uint64_t> selected;
    if (backend_ready) {
        selected.reserve(states.size());
        for (const auto& state : states)
            if (state.renderer_identity == renderer_identity) selected.push_back(state.state_identity);
    }
    struct Guard {
        bool& flag;
        explicit Guard(bool& value) : flag(value) { flag = true; }
        ~Guard() { flag = false; }
    } guard(running_);
    for (const auto identity : selected) hooks.state_frame(identity);
    for (const auto identity : selected) hooks.state_maintenance(identity);
    hooks.backend_maintenance();
}

} // namespace off::graphics
