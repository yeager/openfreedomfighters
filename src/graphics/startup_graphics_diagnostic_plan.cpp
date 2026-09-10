#include "off/graphics/startup_graphics_diagnostic_plan.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_map>

namespace off::graphics {
namespace {
[[nodiscard]] std::array<float, 4> rgba(std::uint32_t value) {
  return {static_cast<float>((value >> 16U) & 0xffU) / 255.0F,
          static_cast<float>((value >> 8U) & 0xffU) / 255.0F,
          static_cast<float>(value & 0xffU) / 255.0F,
          static_cast<float>((value >> 24U) & 0xffU) / 255.0F};
}
} // namespace

SceneGpuPlan make_startup_graphics_diagnostic_plan(
    const StartupGraphicsAsset &asset,
    const StartupGraphicsExpandedPlan &expanded) {
  if (expanded.resources().size() != asset.images().size() ||
      expanded.resources().size() != startup_graphics_image_count)
    throw std::invalid_argument("startup diagnostic resources are incomplete");
  SceneGpuPlan result;
  result.source_only_diagnostic = true;
  std::unordered_map<std::size_t, std::size_t> texture_by_catalog;
  for (const auto &resource : expanded.resources()) {
    if (resource.resource_index >= asset.images().size())
      throw std::invalid_argument("startup diagnostic resource index is invalid");
    const auto &image = asset.images()[resource.resource_index];
    if (image.catalog_image_index != resource.catalog_image_index ||
        image.texture_id != resource.texture_id ||
        image.mip_zero.width != resource.width || image.mip_zero.height != resource.height)
      throw std::invalid_argument("startup diagnostic resource identity disagrees");
    if (!texture_by_catalog.emplace(resource.catalog_image_index, result.textures.size()).second)
      throw std::invalid_argument("startup diagnostic texture identity is duplicated");
    result.textures.push_back({image.mip_zero.width, image.mip_zero.height,
                               image.mip_zero.pixels});
  }
  for (const auto &submission : expanded.submissions()) {
    if (submission.resource_index >= expanded.resources().size())
      throw std::invalid_argument("startup diagnostic submission resource is invalid");
    const auto texture = texture_by_catalog.find(
        expanded.resources()[submission.resource_index].catalog_image_index);
    if (texture == texture_by_catalog.end())
      throw std::invalid_argument("startup diagnostic submission texture is absent");
    SceneGpuMesh mesh;
    mesh.topology = PrimitiveTopology::triangle_strip;
    mesh.alpha_class = VertexAlphaClass::variable;
    mesh.texture_index = texture->second;
    mesh.indices = {0, 1, 3, 2}; // Equivalent winding to the source quad's two triangles.
    mesh.draws = {{0, mesh.indices.size()}};
    for (const auto &vertex : submission.vertices)
      mesh.vertices.push_back({vertex.position, rgba(vertex.color), vertex.uv});
    const auto mesh_index = result.meshes.size();
    result.meshes.push_back(std::move(mesh));
    const auto instance_index = result.instances.size();
    result.instances.push_back({instance_index, mesh_index,
                                {1, 0, 0, 0, 1, 0, 0, 0, 1}, {0, 0, 0}});
    result.draws.push_back({instance_index, mesh_index, texture->second,
                            PrimitiveTopology::triangle_strip,
                            VertexAlphaClass::variable,
                            SceneDepthPolicy::test_only, true, 0, 4});
  }
  if (result.draws.empty())
    throw std::invalid_argument("startup diagnostic has no source submissions");
  // Let the existing source-only plan builder compute bounds/projection under
  // the same checked diagnostic policy used for scene inspection.
  std::array<float, 3> low = result.meshes.front().vertices.front().position;
  std::array<float, 3> high = low;
  for (const auto &mesh : result.meshes)
    for (const auto index : mesh.indices)
      for (std::size_t axis = 0; axis < 3; ++axis) {
        low[axis] = std::min(low[axis], mesh.vertices[index].position[axis]);
        high[axis] = std::max(high[axis], mesh.vertices[index].position[axis]);
      }
  result.projection = {.horizontal_axis = 0, .vertical_axis = 1, .depth_axis = 2,
                       .minimum = low, .maximum = high,
                       .center_horizontal = (low[0] + high[0]) * 0.5F,
                       .center_vertical = (low[1] + high[1]) * 0.5F,
                       .xy_scale = 1.0F};
  validate_scene_gpu_plan(result);
  return result;
}
} // namespace off::graphics
