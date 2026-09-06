#include "off/graphics/group_bounds.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace off::graphics {
namespace {
constexpr float sentinel = std::bit_cast<float>(0x7e967699U);
constexpr float minimum_extent = std::bit_cast<float>(0x39000000U);
float finite(float value) {
    if (!std::isfinite(value)) throw std::runtime_error("group bounds arithmetic must be finite");
    return value;
}
float add(float a,float b) { const volatile float result=a+b; return finite(result); }
float sub(float a,float b) { const volatile float result=a-b; return finite(result); }
float mul(float a,float b) { const volatile float result=a*b; return finite(result); }
struct Union {
    std::array<float,3> lower{sentinel,sentinel,sentinel};
    std::array<float,3> upper{-sentinel,-sentinel,-sentinel};
    bool empty() const { return upper[0] == -sentinel; }
    void include(const std::array<float,3>& lo,const std::array<float,3>& hi) {
        for (std::size_t i=0;i<3;++i) {
            if (lo[i]<lower[i]) lower[i]=lo[i];
            if (hi[i]>upper[i]) upper[i]=hi[i];
        }
    }
    OwnerBounds geometry() const {
        if (empty()) return {{0,0,0},{0,0,0}};
        OwnerBounds result;
        for (std::size_t i=0;i<3;++i) result.center[i]=mul(add(upper[i],lower[i]),0.5F);
        for (std::size_t i=0;i<3;++i) result.extents[i]=sub(upper[i],result.center[i]);
        return result;
    }
};
}

void GroupBoundsRecompute::apply(ResourceBounds& resource_bounds,std::uint32_t& runtime_flags,
                                 OwnerBounds& owner_bounds,std::span<const GroupBoundsChild> children,
                                 bool parent_owner_present,std::uint32_t owner_status,
                                 const GroupBoundsHooks& hooks) {
    if (running_ || failed_ || std::fegetround()!=FE_TONEAREST ||
        !hooks.opt_out || !hooks.name || !hooks.get_bounds)
        throw std::runtime_error("group bounds state, rounding or hooks are unsupported");
    for (const auto& child:children)
        if (!child.resource) throw std::runtime_error("group bounds requires live child resources");
    struct Guard {
        bool& running; bool& failed; int exceptions{std::uncaught_exceptions()};
        Guard(bool& active,bool& poisoned):running(active),failed(poisoned) { running=true; }
        ~Guard() { if (std::uncaught_exceptions()>exceptions) failed=true; running=false; }
    } guard(running_,failed_);
    Union resource_union,child_union;
    if (!parent_owner_present && (owner_status&1U)!=0U) {
        resource_union.lower={-2500.0F,0.0F,-2500.0F};
        resource_union.upper={2500.0F,0.0F,2500.0F};
    }
    for (const auto& child:children) {
        if ((child.resource->flags&0x40000U)!=0U) continue;
        if (child.owner_present && hooks.opt_out(*child.resource)) continue;
        const auto name=child.owner_present ? hooks.name(*child.resource) : std::nullopt;
        if (name.value_or("<NONAME>")=="MOVETOCREATION") continue;
        const auto bounds=hooks.get_bounds(*child.resource);
        if (!bounds) continue;
        std::array<float,3> lower{},upper{};
        for (std::size_t i=0;i<3;++i) {
            finite(bounds->center[i]); finite(bounds->extents[i]);
            if (bounds->extents[i]<0.0F) throw std::runtime_error("group child extents must be nonnegative");
            lower[i]=sub(bounds->center[i],bounds->extents[i]);
            upper[i]=add(bounds->center[i],bounds->extents[i]);
        }
        resource_union.include(lower,upper);
        child_union.include(lower,upper);
    }
    const auto resource=resource_union.geometry();
    const auto owner=child_union.geometry();
    float radius=0.0F;
    if (!resource_union.empty()) {
        const auto& e=resource.extents;
        const auto sum=add(add(mul(e[0],e[0]),mul(e[1],e[1])),mul(e[2],e[2]));
        const volatile float root=static_cast<float>(std::sqrt(static_cast<double>(sum)));
        radius=add(finite(root),1.0F);
    }
    for (std::size_t i=0;i<3;++i) resource_bounds.center[i]=resource.center[i];
    runtime_flags|=0x100000U;
    for (std::size_t i=0;i<3;++i) resource_bounds.extents[i]=resource.extents[i];
    for (std::size_t i=0;i<2;++i)
        if (resource_bounds.extents[i]<minimum_extent) resource_bounds.extents[i]=minimum_extent;
    runtime_flags|=0x100000U;
    if (resource_bounds.extents[2]<minimum_extent) resource_bounds.extents[2]=minimum_extent;
    runtime_flags|=0x100000U;
    resource_bounds.radius=radius;
    if (child_union.empty()) {
        for (auto& value:owner_bounds.extents) value=0.0F;
        for (auto& value:owner_bounds.center) value=0.0F;
    } else {
        for (std::size_t i=0;i<3;++i) owner_bounds.center[i]=owner.center[i];
        for (std::size_t i=0;i<3;++i) owner_bounds.extents[i]=owner.extents[i];
    }
}
} // namespace off::graphics
