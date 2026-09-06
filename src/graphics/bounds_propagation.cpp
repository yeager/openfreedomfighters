#include "off/graphics/bounds_propagation.hpp"

#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <vector>

namespace off::graphics {
namespace {
float finite(float value) {
    if (!std::isfinite(value)) throw std::runtime_error("bounds propagation requires finite arithmetic");
    return value;
}
void nonnegative(float value) {
    if (finite(value)<0.0F) throw std::runtime_error("bounds propagation requires nonnegative bounds");
}
float add(float a,float b) { const volatile float v=a+b; return finite(v); }
float sub(float a,float b) { const volatile float v=a-b; return finite(v); }
float mul(float a,float b) { const volatile float v=a*b; return finite(v); }
void rounding() {
    if (std::fegetround()!=FE_TONEAREST)
        throw std::runtime_error("bounds propagation requires nearest rounding");
}
}

std::optional<ParentSpaceBounds> get_identity_parent_bounds(const BoundsNode& node,std::int32_t suppression) {
    if (suppression>0) return std::nullopt;
    if (!node.resource) throw std::runtime_error("child resource must be live");
    if ((node.resource->flags&0x40000U)!=0U) return std::nullopt;
    if (!node.bounds) throw std::runtime_error("child bounds must be live");
    if (node.bounds->radius==0.0F || !node.parent) return std::nullopt;
    rounding();
    nonnegative(node.bounds->radius);
    constexpr std::array<float,9> identity{0,0,1,0,1,0,1,0,0};
    if (node.basis!=identity) throw std::runtime_error("parent bounds support identity bases only");
    ParentSpaceBounds result;
    for (std::size_t i=0;i<3;++i) {
        finite(node.bounds->center[i]); finite(node.position[i]);
        nonnegative(node.bounds->extents[i]);
        result.center[i]=add(node.bounds->center[i],node.position[i]);
        result.extents[i]=node.bounds->extents[i];
    }
    return result;
}

std::size_t IdentityBoundsPropagation::apply(BoundsNode& changed,std::int32_t suppression) {
    if (running_ || failed_) throw std::runtime_error("bounds propagation state or reentry is unsupported");
    struct Guard {
        bool& running; bool& failed; int exceptions{std::uncaught_exceptions()};
        Guard(bool& active,bool& poisoned):running(active),failed(poisoned) { running=true; }
        ~Guard() { if (std::uncaught_exceptions()>exceptions) failed=true; running=false; }
    } guard(running_,failed_);
    BoundsNode* current=&changed;
    std::vector<BoundsNode*> reached{current};
    std::size_t updated=0;
    for (;;) {
        if (suppression>0) return updated;
        if (!current->resource) throw std::runtime_error("propagation resource must be live");
        if ((current->resource->flags&0x40000U)!=0U || !current->parent) return updated;
        if (current->owner_opt_out) return updated;
        const auto child=get_identity_parent_bounds(*current,suppression);
        if (!child) return updated;
        auto* parent=current->parent;
        if (std::find(reached.begin(),reached.end(),parent)!=reached.end())
            throw std::runtime_error("bounds propagation reached a cycle");
        if (!parent->resource || !parent->bounds)
            throw std::runtime_error("parent bounds resource must be live");
        if (std::any_of(reached.begin(),reached.end(),[&](const auto* node) {
                return node->resource==parent->resource || node->bounds==parent->bounds;
            }))
            throw std::runtime_error("bounds hierarchy aliases a reached resource or bounds object");
        reached.push_back(parent);
        nonnegative(parent->bounds->radius);
        std::array<float,3> lower{},upper{};
        bool changed_parent=parent->bounds->radius==0.0F;
        for (std::size_t i=0;i<3;++i) {
            const auto child_lower=sub(child->center[i],child->extents[i]);
            const auto child_upper=add(child->center[i],child->extents[i]);
            if (parent->bounds->radius==0.0F) {
                lower[i]=child_lower; upper[i]=child_upper;
            } else {
                finite(parent->bounds->center[i]); nonnegative(parent->bounds->extents[i]);
                lower[i]=sub(parent->bounds->center[i],parent->bounds->extents[i]);
                upper[i]=add(parent->bounds->center[i],parent->bounds->extents[i]);
                if (child_lower<lower[i]) { lower[i]=child_lower; changed_parent=true; }
                if (child_upper>upper[i]) { upper[i]=child_upper; changed_parent=true; }
            }
        }
        if (!changed_parent) return updated;
        if (!parent->owner_bounds) throw std::runtime_error("updated parent owner bounds must be live");
        std::array<float,3> center{},raw{};
        for (std::size_t i=0;i<3;++i) center[i]=mul(add(upper[i],lower[i]),0.5F);
        for (std::size_t i=0;i<3;++i) raw[i]=sub(upper[i],center[i]);
        const auto q=add(add(mul(raw[0],raw[0]),mul(raw[1],raw[1])),mul(raw[2],raw[2]));
        const volatile float root=static_cast<float>(std::sqrt(static_cast<double>(q)));
        const auto radius=add(finite(root),1.0F);
        constexpr auto min_extent=std::bit_cast<float>(0x39000000U);
        for (std::size_t i=0;i<3;++i) parent->bounds->center[i]=center[i];
        parent->resource->flags|=0x100000U;
        for (std::size_t i=0;i<3;++i) parent->bounds->extents[i]=raw[i];
        for (std::size_t i=0;i<2;++i)
            if (parent->bounds->extents[i]<min_extent) parent->bounds->extents[i]=min_extent;
        parent->resource->flags|=0x100000U;
        if (parent->bounds->extents[2]<min_extent) parent->bounds->extents[2]=min_extent;
        parent->resource->flags|=0x100000U;
        parent->bounds->radius=radius;
        for (std::size_t i=0;i<3;++i) parent->owner_bounds->extents[i]=parent->bounds->extents[i];
        for (std::size_t i=0;i<3;++i) parent->owner_bounds->center[i]=parent->bounds->center[i];
        ++updated;
        current=parent;
    }
}
} // namespace off::graphics
