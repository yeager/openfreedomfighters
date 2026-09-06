#include "off/graphics/picture_bounds.hpp"
#include "off/graphics/picture_submission_cache.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace off::graphics {
bool query_zero_renderer_resource_bounds(std::uint64_t, std::uint64_t renderer_resource_id,
                                         std::array<float,3>&, std::array<float,3>&) {
    if (renderer_resource_id != 0U)
        throw std::runtime_error("renderer bounds query requires the verified zero-identifier path");
    return false;
}

namespace {
float finite(float value) {
    if (!std::isfinite(value)) throw std::runtime_error("picture bounds arithmetic must be finite");
    return value;
}
float add(float a, float b) { const volatile float value = a + b; return finite(value); }
float subtract(float a, float b) { const volatile float value = a - b; return finite(value); }
float multiply(float a, float b) { const volatile float value = a * b; return finite(value); }
float radius_from(std::array<float,3> raw) {
    const auto square_sum = add(add(multiply(raw[0],raw[0]),multiply(raw[1],raw[1])),multiply(raw[2],raw[2]));
    const volatile float root = static_cast<float>(std::sqrt(static_cast<double>(square_sum)));
    return add(finite(root),1.0F);
}
}

PictureBounds compute_picture_bounds(std::span<const data::PictureResourceDescriptor> descriptors,
                                     std::span<const data::PictureDrawGroup> groups,
                                     std::array<float, 2> scale) {
    if (std::fegetround() != FE_TONEAREST)
        throw std::runtime_error("picture bounds require nearest rounding");
    for (float value : scale)
        if (!std::isfinite(value) || value <= 0.0F)
            throw std::runtime_error("picture bounds require positive finite scales");
    for (const auto& group : groups)
        if (group.first_descriptor_index > descriptors.size() ||
            group.descriptor_span_count > descriptors.size() - group.first_descriptor_index)
            throw std::runtime_error("picture bounds group exceeds descriptors");
    constexpr float sentinel = std::bit_cast<float>(0x7e967699U);
    std::array<float, 3> minimum{sentinel,sentinel,sentinel};
    std::array<float, 3> maximum{-sentinel,-sentinel,-sentinel};
    bool visited = false;
    for (const auto& group : groups) {
        for (std::size_t i = 0; i < group.descriptor_span_count; ++i) {
            const auto& descriptor = descriptors[group.first_descriptor_index + i];
            finite(descriptor.local_center_x);
            finite(descriptor.local_center_y);
            if (!std::isfinite(descriptor.horizontal_edge_span) || descriptor.horizontal_edge_span < 0.0F ||
                !std::isfinite(descriptor.vertical_edge_span) || descriptor.vertical_edge_span < 0.0F)
                throw std::runtime_error("picture bounds require finite nonnegative spans");
            const auto half_x = multiply(descriptor.horizontal_edge_span, 0.5F);
            const auto half_y = multiply(descriptor.vertical_edge_span, 0.5F);
            const std::array<float,3> lower{subtract(descriptor.local_center_x,half_x),
                                             subtract(descriptor.local_center_y,half_y),0.0F};
            const std::array<float,3> upper{add(descriptor.local_center_x,half_x),
                                             add(descriptor.local_center_y,half_y),0.0F};
            for (std::size_t component = 0; component < 3; ++component) {
                if (lower[component] < minimum[component]) minimum[component] = lower[component];
                if (upper[component] > maximum[component]) maximum[component] = upper[component];
            }
            visited = true;
        }
    }
    if (!visited) throw std::runtime_error("picture bounds require at least one descriptor visit");
    std::array<float,3> difference{}, sum{};
    for (std::size_t i = 0; i < 3; ++i) {
        difference[i] = subtract(maximum[i],minimum[i]);
        sum[i] = add(maximum[i],minimum[i]);
    }
    const auto width = multiply(scale[0],0.5F);
    const auto height = multiply(scale[1],0.5F);
    const std::array<float,3> center{multiply(width,sum[0]),multiply(height,-sum[1]),sum[2]};
    const std::array<float,3> raw{multiply(width,difference[0]),multiply(height,difference[1]),difference[2]};
    auto extents = raw;
    constexpr auto minimum_extent = std::bit_cast<float>(0x39000000U);
    for (auto& extent : extents) if (extent < minimum_extent) extent = minimum_extent;
    const auto radius = radius_from(raw);
    return {center,raw,extents,radius};
}

void PictureBoundsApplication::apply(ResourceBounds& bounds, std::uint32_t& runtime_flags,
                                     std::uint64_t runtime_identity, std::uint64_t renderer_resource_id,
                                     std::span<const data::PictureResourceDescriptor> descriptors,
                                     std::span<const data::PictureDrawGroup> groups,
                                     std::array<float,2> scale, const Query& query) {
    if (running_ || failed_ || !query)
        throw std::runtime_error("picture bounds application state or query is unsupported");
    const auto computed = compute_picture_bounds(descriptors,groups,scale);
    struct Guard {
        bool& running;
        bool& failed;
        int exceptions{std::uncaught_exceptions()};
        Guard(bool& active, bool& poisoned) : running(active), failed(poisoned) { running = true; }
        ~Guard() {
            if (std::uncaught_exceptions() > exceptions) failed = true;
            running = false;
        }
    } guard(running_,failed_);
    constexpr auto minimum_extent = std::bit_cast<float>(0x39000000U);
    if (query(runtime_identity,renderer_resource_id,bounds.center,bounds.extents)) {
        for (float value : bounds.center) finite(value);
        for (float value : bounds.extents) finite(value);
        const auto base_radius = radius_from(bounds.extents);
        runtime_flags |= 0x100000U;
        bounds.radius = base_radius;
    } else {
        runtime_flags |= 0x100000U;
        bounds.radius = 0.0F;
        for (auto& value : bounds.center) value = 0.0F;
        runtime_flags |= 0x100000U;
        for (auto& value : bounds.extents) value = 0.0F;
        for (auto& value : bounds.extents) if (value < minimum_extent) value = minimum_extent;
        runtime_flags |= 0x100000U;
    }
    for (std::size_t i = 0; i < 3; ++i) bounds.center[i] = computed.center[i];
    runtime_flags |= 0x100000U;
    for (std::size_t i = 0; i < 3; ++i) bounds.extents[i] = computed.raw_extents[i];
    for (std::size_t i = 0; i < 2; ++i)
        if (bounds.extents[i] < minimum_extent) bounds.extents[i] = minimum_extent;
    runtime_flags |= 0x100000U;
    if (bounds.extents[2] < minimum_extent) bounds.extents[2] = minimum_extent;
    runtime_flags |= 0x100000U;
    bounds.radius = computed.radius;
}

void PictureBoundsApplication::apply_materialized(
    ResourceBounds& bounds, std::uint32_t& runtime_flags,
    std::uint64_t runtime_identity, std::uint64_t renderer_resource_id,
    std::span<const data::PictureResourceDescriptor> descriptors,
    std::span<const data::PictureDrawGroup> groups, std::array<float,2> scale,
    const Query& query, std::uint32_t alignment_enum,
    std::array<float,2>& alignment, PictureSubmissionCache& cache) {
    if (running_ || failed_ || !query)
        throw std::runtime_error("picture bounds application state or query is unsupported");
    const auto computed = compute_picture_bounds(descriptors, groups, scale);
    const auto offset = picture_alignment_offset(alignment_enum,
        {computed.center[0], computed.center[1], computed.extents[0], computed.extents[1]});
    apply(bounds, runtime_flags, runtime_identity, renderer_resource_id,
          descriptors, groups, scale, query);
    alignment[0] = offset[0];
    alignment[1] = offset[1];
    cache.invalidate();
}

} // namespace off::graphics
