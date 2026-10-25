#include "off/graphics/scene_runtime_binding.hpp"

#include <algorithm>
#include <stdexcept>

namespace off::graphics {

std::vector<SceneRuntimeInstanceBinding> bind_scene_instances_to_runtime(
    const SceneRenderAsset &asset, const IntroRuntime &runtime) {
  if (runtime.resource_load_stage() !=
      IntroResourceLoadStage::directory_construction_complete) {
    throw std::runtime_error(
        "Scene runtime binding requires completed directory construction");
  }

  const auto mapping = runtime.directory_resource_mapping();
  if (std::ranges::any_of(mapping, [](const auto &resource) {
        return !resource.has_value();
      })) {
    throw std::runtime_error(
        "Scene runtime binding requires a complete directory resource map");
  }

  std::vector<SceneRuntimeInstanceBinding> result;
  result.reserve(asset.instances.size());
  for (std::size_t instance_index = 0;
       instance_index < asset.instances.size(); ++instance_index) {
    const auto &instance = asset.instances[instance_index];
    const auto source_index = instance.source_directory_index;
    if (source_index >= mapping.size() || !mapping[source_index]) {
      throw std::runtime_error(
          "Scene instance source has no constructed runtime resource");
    }

    const auto owner = runtime.source_handle(source_index);
    const auto resource = *mapping[source_index];
    if (runtime.associated_resource_owner(resource) != owner ||
        runtime.resource_handle(owner) != resource) {
      throw std::runtime_error(
          "Scene instance source is not bound to its canonical runtime owner");
    }

    const auto pose = runtime.source_transform(owner);
    if (pose.basis != instance.source_basis ||
        pose.position != instance.source_position) {
      throw std::runtime_error(
          "Scene instance source pose does not match its runtime directory row");
    }

    std::optional<IntroRuntimeHandle> parent_owner;
    const auto parent_resource = runtime.resource_parent(owner);
    if (parent_resource.value != 0) {
      parent_owner = runtime.associated_resource_owner(parent_resource);
      if (!parent_owner) {
        throw std::runtime_error(
            "Scene instance runtime parent has no associated owner");
      }
    }
    if (instance.parent_source_directory_index) {
      const auto expected_parent =
          runtime.source_handle(*instance.parent_source_directory_index);
      if (parent_owner != expected_parent) {
        throw std::runtime_error(
            "Scene instance authored parent differs from its runtime parent");
      }
    }

    result.push_back({.scene_instance_index = instance_index,
                      .source_directory_index = source_index,
                      .owner = owner,
                      .resource = resource,
                      .parent_owner = parent_owner});
  }
  return result;
}

} // namespace off::graphics
