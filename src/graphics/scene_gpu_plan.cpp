#include "off/graphics/scene_gpu_plan.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::graphics {
namespace {

constexpr float clip_margin_span = 1.8F;
constexpr float depth_minimum = 0.05F;
constexpr float depth_span = 0.9F;
constexpr float minimum_extent = 1.0e-6F;
constexpr std::size_t maximum_gpu_draws = 4'000'000;

[[nodiscard]] bool finite(const std::array<float, 3> &value) {
  return std::ranges::all_of(
      value, [](float component) { return std::isfinite(component); });
}

} // namespace

void validate_scene_gpu_plan(const SceneGpuPlan &plan) {
  if (!plan.source_only_diagnostic || plan.draws.size() > maximum_gpu_draws) {
    throw std::invalid_argument("scene GPU plan has an invalid contract");
  }
  for (const auto &texture : plan.textures) {
    if (texture.width == 0 || texture.height == 0 ||
        texture.width > std::numeric_limits<std::size_t>::max() / 4U ||
        texture.height > std::numeric_limits<std::size_t>::max() /
                             (static_cast<std::size_t>(texture.width) * 4U) ||
        texture.rgba8.size() !=
            static_cast<std::size_t>(texture.width) * texture.height * 4U) {
      throw std::invalid_argument("scene GPU texture storage is inconsistent");
    }
  }
  for (const auto &mesh : plan.meshes) {
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.draws.empty() ||
        (mesh.texture_index.has_value() &&
         *mesh.texture_index >= plan.textures.size())) {
      throw std::invalid_argument("scene GPU mesh resources are incomplete");
    }
    for (const auto &vertex : mesh.vertices) {
      if (!finite(vertex.position) ||
          !std::ranges::all_of(vertex.color,
                               [](float value) {
                                 return std::isfinite(value) && value >= 0.0F &&
                                        value <= 1.0F;
                               }) ||
          !std::ranges::all_of(vertex.texture_coordinates, [](float value) {
            return std::isfinite(value);
          })) {
        throw std::invalid_argument("scene GPU vertex data is invalid");
      }
    }
    if (std::ranges::any_of(mesh.indices, [&](const auto index) {
          return index >= mesh.vertices.size();
        })) {
      throw std::invalid_argument("scene GPU mesh index is invalid");
    }
    std::size_t next_index = 0;
    for (const auto &draw : mesh.draws) {
      const auto minimum_count =
          mesh.topology == PrimitiveTopology::triangle_strip ? 3U : 2U;
      if (draw.first_index != next_index || draw.index_count < minimum_count ||
          (mesh.topology == PrimitiveTopology::line_list &&
           draw.index_count != 2U) ||
          draw.first_index > mesh.indices.size() ||
          draw.index_count > mesh.indices.size() - draw.first_index) {
        throw std::invalid_argument("scene GPU mesh draw range is invalid");
      }
      next_index += draw.index_count;
    }
    if (next_index != mesh.indices.size()) {
      throw std::invalid_argument("scene GPU mesh draw coverage is incomplete");
    }
  }
  for (std::size_t index = 0; index < plan.instances.size(); ++index) {
    const auto &instance = plan.instances[index];
    if (instance.scene_instance_index != index ||
        !std::ranges::all_of(
            instance.source_basis,
            [](float value) { return std::isfinite(value); }) ||
        !finite(instance.source_position)) {
      throw std::invalid_argument("scene GPU instance data is invalid");
    }
  }
  if (!plan.draws.empty()) {
    const auto &projection = plan.projection;
    const auto axes_valid =
        projection.horizontal_axis < 3U && projection.vertical_axis < 3U &&
        projection.depth_axis < 3U &&
        projection.horizontal_axis != projection.vertical_axis &&
        projection.horizontal_axis != projection.depth_axis &&
        projection.vertical_axis != projection.depth_axis;
    const auto bounds_valid = projection.minimum[0] <= projection.maximum[0] &&
                              projection.minimum[1] <= projection.maximum[1] &&
                              projection.minimum[2] <= projection.maximum[2];
    if (!axes_valid || !bounds_valid || !finite(projection.minimum) ||
        !finite(projection.maximum) ||
        !std::isfinite(projection.center_horizontal) ||
        !std::isfinite(projection.center_vertical) ||
        !std::isfinite(projection.xy_scale)) {
      throw std::invalid_argument("scene GPU diagnostic projection is invalid");
    }
  }
  std::uint8_t previous_bucket = 0;
  for (const auto &draw : plan.draws) {
    if (draw.instance_index >= plan.instances.size() ||
        draw.mesh_index >= plan.meshes.size()) {
      throw std::invalid_argument("scene GPU draw reference is invalid");
    }
    const auto &mesh = plan.meshes[draw.mesh_index];
    if (draw.texture_index != mesh.texture_index ||
        draw.topology != mesh.topology ||
        draw.alpha_class != mesh.alpha_class ||
        std::ranges::none_of(mesh.draws, [&](const auto &range) {
          return range.first_index == draw.first_index &&
                 range.index_count == draw.index_count;
        })) {
      throw std::invalid_argument("scene GPU draw resources are inconsistent");
    }
    const auto bucket = draw.alpha_class == VertexAlphaClass::opaque
                            ? std::uint8_t{0}
                            : (draw.alpha_class == VertexAlphaClass::variable
                                   ? std::uint8_t{1}
                                   : std::uint8_t{2});
    const auto policy_valid =
        (bucket == 0 && draw.depth_policy == SceneDepthPolicy::test_and_write &&
         !draw.blend_enabled) ||
        (bucket == 1 && draw.depth_policy == SceneDepthPolicy::test_only &&
         draw.blend_enabled) ||
        (bucket == 2 && draw.depth_policy == SceneDepthPolicy::no_draw &&
         !draw.blend_enabled);
    if (bucket < previous_bucket || !policy_valid) {
      throw std::invalid_argument("scene GPU draw policy or order is invalid");
    }
    previous_bucket = bucket;
  }
}

std::array<float, 3> transform_scene_source_diagnostic_position(
    const SceneGpuInstance &instance,
    const std::array<float, 3> &local_position) {
  const auto &basis = instance.source_basis;
  std::array<float, 3> result;
  for (std::size_t row = 0; row < 3; ++row) {
    const auto value =
        static_cast<double>(basis[row * 3]) * local_position[0] +
        static_cast<double>(basis[row * 3 + 1]) * local_position[1] +
        static_cast<double>(basis[row * 3 + 2]) * local_position[2] +
        instance.source_position[row];
    if (!std::isfinite(value) ||
        std::abs(value) > std::numeric_limits<float>::max()) {
      throw std::invalid_argument(
          "scene diagnostic transform produced an invalid position");
    }
    result[row] = static_cast<float>(value);
  }
  if (!finite(result)) {
    throw std::invalid_argument(
        "scene diagnostic transform produced a non-finite position");
  }
  return result;
}

SceneGpuPlan prepare_scene_gpu_plan(const SceneRenderAsset &asset) {
  validate_scene_render_asset(asset);
  SceneGpuPlan result;
  result.textures.reserve(asset.textures.size());
  for (const auto &texture : asset.textures) {
    result.textures.push_back({
        .width = texture.mip_zero.width,
        .height = texture.mip_zero.height,
        .rgba8 = texture.mip_zero.pixels,
    });
  }
  result.meshes.reserve(asset.meshes.size());
  for (const auto &mesh : asset.meshes) {
    SceneGpuMesh gpu_mesh{
        .topology = mesh.topology,
        .alpha_class = mesh.alpha_class,
        .vertices = {},
        .indices = mesh.indices,
        .draws = mesh.draws,
        .texture_index = mesh.texture_index,
    };
    gpu_mesh.vertices.reserve(mesh.vertices.size());
    for (const auto &vertex : mesh.vertices) {
      gpu_mesh.vertices.push_back({
          .position = vertex.position,
          .color = {vertex.color_rgba[0] / 255.0F,
                    vertex.color_rgba[1] / 255.0F,
                    vertex.color_rgba[2] / 255.0F,
                    vertex.color_rgba[3] / 255.0F},
          .texture_coordinates = vertex.texture_coordinates,
      });
    }
    result.meshes.push_back(std::move(gpu_mesh));
  }
  result.instances.reserve(asset.instances.size());
  for (std::size_t index = 0; index < asset.instances.size(); ++index) {
    result.instances.push_back({
        .scene_instance_index = index,
        .source_basis = asset.instances[index].source_basis,
        .source_position = asset.instances[index].source_position,
    });
  }
  if (asset.instances.empty()) {
    validate_scene_gpu_plan(result);
    return result;
  }

  std::array minimum{std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};
  std::array maximum{std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest()};
  std::array<double, 3> projected_areas{};
  for (std::size_t instance_index = 0; instance_index < asset.instances.size();
       ++instance_index) {
    const auto &scene_instance = asset.instances[instance_index];
    const auto &instance = result.instances[instance_index];
    const auto &mesh = asset.meshes[scene_instance.mesh_index];
    std::vector<std::array<float, 3>> positions;
    positions.reserve(mesh.vertices.size());
    for (const auto &vertex : mesh.vertices) {
      positions.push_back(transform_scene_source_diagnostic_position(
          instance, vertex.position));
    }
    for (const auto index : mesh.indices) {
      for (std::size_t axis = 0; axis < 3; ++axis) {
        minimum[axis] = std::min(minimum[axis], positions[index][axis]);
        maximum[axis] = std::max(maximum[axis], positions[index][axis]);
      }
    }
    if (mesh.topology != PrimitiveTopology::triangle_strip) {
      continue;
    }
    for (const auto &draw : mesh.draws) {
      for (std::size_t offset = 2; offset < draw.index_count; ++offset) {
        const auto &a = positions[mesh.indices[draw.first_index + offset - 2]];
        const auto &b = positions[mesh.indices[draw.first_index + offset - 1]];
        const auto &c = positions[mesh.indices[draw.first_index + offset]];
        const std::array<double, 3> ab{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const std::array<double, 3> ac{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        projected_areas[0] += std::abs(ab[1] * ac[2] - ab[2] * ac[1]);
        projected_areas[1] += std::abs(ab[2] * ac[0] - ab[0] * ac[2]);
        projected_areas[2] += std::abs(ab[0] * ac[1] - ab[1] * ac[0]);
      }
    }
  }

  const std::array extents{maximum[0] - minimum[0], maximum[1] - minimum[1],
                           maximum[2] - minimum[2]};
  std::size_t depth_axis;
  if (std::ranges::any_of(projected_areas,
                          [](double area) { return area > 0.0; })) {
    depth_axis = static_cast<std::size_t>(std::distance(
        projected_areas.begin(),
        std::max_element(projected_areas.begin(), projected_areas.end())));
  } else {
    depth_axis = static_cast<std::size_t>(std::distance(
        extents.begin(), std::min_element(extents.begin(), extents.end())));
  }
  constexpr std::array<std::array<std::uint8_t, 2>, 3> projection_axes{
      std::array<std::uint8_t, 2>{1, 2}, {0, 2}, {0, 1}};
  const auto horizontal = projection_axes[depth_axis][0];
  const auto vertical = projection_axes[depth_axis][1];
  const auto visible_extent = std::max(extents[horizontal], extents[vertical]);
  result.projection = {
      .horizontal_axis = horizontal,
      .vertical_axis = vertical,
      .depth_axis = static_cast<std::uint8_t>(depth_axis),
      .minimum = minimum,
      .maximum = maximum,
      .center_horizontal = (minimum[horizontal] + maximum[horizontal]) * 0.5F,
      .center_vertical = (minimum[vertical] + maximum[vertical]) * 0.5F,
      .xy_scale = visible_extent > minimum_extent
                      ? clip_margin_span / visible_extent
                      : 1.0F,
  };

  const auto append_draws = [&](VertexAlphaClass alpha_class) {
    for (std::size_t instance_index = 0;
         instance_index < asset.instances.size(); ++instance_index) {
      const auto &instance = asset.instances[instance_index];
      const auto &mesh = asset.meshes[instance.mesh_index];
      if (mesh.alpha_class != alpha_class) {
        continue;
      }
      const auto is_opaque = alpha_class == VertexAlphaClass::opaque;
      const auto is_visible =
          alpha_class != VertexAlphaClass::fully_transparent;
      for (const auto &draw : mesh.draws) {
        result.draws.push_back({
            .instance_index = instance_index,
            .mesh_index = instance.mesh_index,
            .texture_index = mesh.texture_index,
            .topology = mesh.topology,
            .alpha_class = mesh.alpha_class,
            .depth_policy = is_opaque
                                ? SceneDepthPolicy::test_and_write
                                : (is_visible ? SceneDepthPolicy::test_only
                                              : SceneDepthPolicy::no_draw),
            .blend_enabled = !is_opaque && is_visible,
            .first_index = draw.first_index,
            .index_count = draw.index_count,
        });
      }
    }
  };
  append_draws(VertexAlphaClass::opaque);
  append_draws(VertexAlphaClass::variable);
  append_draws(VertexAlphaClass::fully_transparent);
  validate_scene_gpu_plan(result);
  return result;
}

SceneDiagnosticUniform
make_scene_diagnostic_uniform(const SceneDiagnosticProjection &projection,
                              std::uint32_t pixel_width,
                              std::uint32_t pixel_height) {
  if (pixel_width == 0 || pixel_height == 0) {
    throw std::invalid_argument(
        "scene diagnostic viewport dimensions must be nonzero");
  }
  const auto horizontal_extent =
      projection.maximum[projection.horizontal_axis] -
      projection.minimum[projection.horizontal_axis];
  const auto vertical_extent = projection.maximum[projection.vertical_axis] -
                               projection.minimum[projection.vertical_axis];
  const auto depth_extent = projection.maximum[projection.depth_axis] -
                            projection.minimum[projection.depth_axis];
  const auto aspect = static_cast<double>(pixel_width) / pixel_height;
  const auto fit_span = std::max(static_cast<double>(vertical_extent),
                                 horizontal_extent / aspect);
  const auto vertical_scale =
      fit_span > minimum_extent ? clip_margin_span / fit_span : 1.0;
  const auto horizontal_scale = vertical_scale / aspect;
  const auto depth_scale =
      depth_extent > minimum_extent ? depth_span / depth_extent : 0.0;
  if (!std::isfinite(horizontal_scale) || !std::isfinite(vertical_scale) ||
      !std::isfinite(depth_scale)) {
    throw std::invalid_argument(
        "scene diagnostic viewport produces a non-finite scale");
  }
  return {
      .scale = {static_cast<float>(horizontal_scale),
                static_cast<float>(vertical_scale),
                static_cast<float>(depth_scale)},
      .axes = {projection.horizontal_axis, projection.vertical_axis,
               projection.depth_axis},
      .center_horizontal = projection.center_horizontal,
      .center_vertical = projection.center_vertical,
      .minimum_depth = projection.minimum[projection.depth_axis],
      .degenerate_depth = depth_extent <= minimum_extent,
  };
}

std::array<float, 3> project_scene_diagnostic_position(
    const SceneGpuPlan &plan, std::size_t instance_index,
    const std::array<float, 3> &local_position, std::uint32_t pixel_width,
    std::uint32_t pixel_height) {
  if (instance_index >= plan.instances.size()) {
    throw std::invalid_argument(
        "scene diagnostic projection has an invalid instance index");
  }
  const auto source = transform_scene_source_diagnostic_position(
      plan.instances[instance_index], local_position);
  const auto &projection = plan.projection;
  const auto uniform =
      make_scene_diagnostic_uniform(projection, pixel_width, pixel_height);
  const auto depth =
      uniform.degenerate_depth
          ? 0.5F
          : depth_minimum + (source[uniform.axes[2]] - uniform.minimum_depth) *
                                uniform.scale[2];
  const std::array result{
      (source[uniform.axes[0]] - uniform.center_horizontal) * uniform.scale[0],
      -(source[uniform.axes[1]] - uniform.center_vertical) * uniform.scale[1],
      depth,
  };
  if (!finite(result)) {
    throw std::invalid_argument(
        "scene diagnostic projection produced a non-finite position");
  }
  return result;
}

} // namespace off::graphics
