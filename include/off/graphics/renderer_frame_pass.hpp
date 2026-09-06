#pragma once

#include <cstdint>
#include <functional>
#include <span>

namespace off::graphics {

struct RendererStateEntry {
    std::uint64_t renderer_identity;
    std::uint64_t state_identity;
};
struct RendererFrameHooks {
    std::function<void(std::uint64_t)> state_frame;
    std::function<void(std::uint64_t)> state_maintenance;
    std::function<void()> backend_maintenance;
};

// Explicit admitted renderer/backend boundary, not an application frame loop.
// State identities refer to live caller-owned objects, not authored references.
class RendererFramePass final {
public:
    RendererFramePass() = default;
    RendererFramePass(const RendererFramePass&) = delete;
    RendererFramePass& operator=(const RendererFramePass&) = delete;
    RendererFramePass(RendererFramePass&&) = delete;
    RendererFramePass& operator=(RendererFramePass&&) = delete;

    // Snapshots matching state identities before invoking any frame callback.
    // Registration storage may change afterward, but referenced states must live
    // through both phases. All frames precede all per-state maintenance calls.
    // Native policies: reject reentry and missing hooks before effects; exceptions
    // propagate with their prefix and do not synthesize later maintenance.
    void run(bool renderer_admitted, bool backend_ready, std::uint64_t renderer_identity,
             std::span<const RendererStateEntry> states, const RendererFrameHooks& hooks);

    // Complete the same preparation phase, then invoke ordered drawing with the
    // exact captured matching-state sequence, after backend maintenance. Never
    // recollect from a registry that callbacks may have changed. The snapshot
    // span is valid only during this synchronous callback. An empty snapshot
    // still reaches drawing when renderer admission passed.
    using OrderedDrawing = std::function<void(std::span<const std::uint64_t>)>;
    void run_and_draw(bool renderer_admitted, bool backend_ready, std::uint64_t renderer_identity,
             std::span<const RendererStateEntry> states, const RendererFrameHooks& hooks,
             const OrderedDrawing& ordered_drawing);

private:
    void run_impl(bool renderer_admitted, bool backend_ready, std::uint64_t renderer_identity,
             std::span<const RendererStateEntry> states, const RendererFrameHooks& hooks,
             const OrderedDrawing& ordered_drawing);
    bool running_{false};
};

} // namespace off::graphics
