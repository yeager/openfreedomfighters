#pragma once

#include "off/graphics/scene_render.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace off::graphics {

enum class SceneDepthPolicy : std::uint8_t {
  test_and_write,
  test_only,
  no_draw
};

struct SceneGpuVertex {
  std::array<float, 3> position{};
  std::array<float, 4> color{};
  std::array<float, 2> texture_coordinates{};
};

struct SceneGpuTexture {
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::vector<std::uint8_t> rgba8;
};

struct SceneGpuMesh {
  PrimitiveTopology topology{PrimitiveTopology::triangle_strip};
  VertexAlphaClass alpha_class{VertexAlphaClass::opaque};
  std::vector<SceneGpuVertex> vertices;
  std::vector<std::uint16_t> indices;
  std::vector<PrimitiveDrawRange> draws;
  std::optional<std::size_t> texture_index;
};

struct SceneGpuInstance {
  std::size_t scene_instance_index{0};
  std::size_t mesh_index{0};
  std::array<float, 9> source_basis{};
  std::array<float, 3> source_position{};
};

struct SceneGpuDraw {
  std::size_t instance_index{0};
  std::size_t mesh_index{0};
  std::optional<std::size_t> texture_index;
  PrimitiveTopology topology{PrimitiveTopology::triangle_strip};
  VertexAlphaClass alpha_class{VertexAlphaClass::opaque};
  SceneDepthPolicy depth_policy{SceneDepthPolicy::test_and_write};
  bool blend_enabled{false};
  std::size_t first_index{0};
  std::size_t index_count{0};
};

struct SceneDiagnosticProjection {
  std::uint8_t horizontal_axis{0};
  std::uint8_t vertical_axis{1};
  std::uint8_t depth_axis{2};
  std::array<float, 3> minimum{};
  std::array<float, 3> maximum{};
  float center_horizontal{0.0F};
  float center_vertical{0.0F};
  float xy_scale{1.0F};
};

struct SceneGpuPlan {
  SceneDiagnosticProjection projection;
  std::vector<SceneGpuTexture> textures;
  std::vector<SceneGpuMesh> meshes;
  std::vector<SceneGpuInstance> instances;
  std::vector<SceneGpuDraw> draws;
  bool source_only_diagnostic{true};
};

struct SceneDiagnosticUniform {
  std::array<float, 3> scale{};
  std::array<std::uint8_t, 3> axes{};
  float center_horizontal{0.0F};
  float center_vertical{0.0F};
  float minimum_depth{0.0F};
  bool degenerate_depth{false};
};

[[nodiscard]] std::array<float, 3> transform_scene_source_diagnostic_position(
    const SceneGpuInstance &instance,
    const std::array<float, 3> &local_position);

[[nodiscard]] SceneGpuPlan
prepare_scene_gpu_plan(const SceneRenderAsset &asset);

void validate_scene_gpu_plan(const SceneGpuPlan &plan);

[[nodiscard]] SceneDiagnosticUniform
make_scene_diagnostic_uniform(const SceneDiagnosticProjection &projection,
                              std::uint32_t pixel_width,
                              std::uint32_t pixel_height);

[[nodiscard]] std::array<float, 3> project_scene_diagnostic_position(
    const SceneGpuPlan &plan, std::size_t instance_index,
    const std::array<float, 3> &local_position, std::uint32_t pixel_width = 1,
    std::uint32_t pixel_height = 1);

} // namespace off::graphics
