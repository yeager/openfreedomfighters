#include "off/graphics/scene_runtime_binding.hpp"

#include <algorithm>
#include <stdexcept>

namespace off::graphics {

std::vector<SceneRuntimeInstanceBinding> bind_scene_instances_to_runtime(
    const SceneRenderAsset &asset, const IntroRuntime &runtime) {
  validate_scene_render_asset(asset);

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
  const auto &source_hierarchy = runtime.resources().sources().hierarchy();
  for (std::size_t instance_index = 0;
       instance_index < asset.instances.size(); ++instance_index) {
    const auto &instance = asset.instances[instance_index];
    const auto source_index = instance.source_directory_index;
    if (source_index >= mapping.size() || !mapping[source_index]) {
      throw std::runtime_error(
          "Scene instance source has no constructed runtime resource");
    }

    const auto &source_entry =
        runtime.resources().sources().directory().at(source_index);
    if (source_entry.local_slot_index != instance.source_local_slot_index ||
        source_entry.source_type != instance.source_type) {
      throw std::runtime_error(
          "Scene instance source metadata differs from its "
          "runtime directory row");
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

    if (source_index >= source_hierarchy.size() ||
        source_hierarchy[source_index].directory_index != source_index) {
      throw std::runtime_error(
          "Scene instance source has no canonical runtime hierarchy row");
    }
    const auto &source_node = source_hierarchy[source_index];
    if (instance.parent_source_directory_index !=
        source_node.parent_directory_index &&
        instance.parent_source_directory_index.has_value()) {
      throw std::runtime_error(
          "Scene instance authored parent differs from runtime GMS hierarchy");
    }

    std::optional<IntroRuntimeHandle> parent_owner;
    auto ancestor_index = source_index;
    std::size_t ancestry_steps = 0;
    while (true) {
      if (ancestor_index >= source_hierarchy.size() ||
          source_hierarchy[ancestor_index].directory_index != ancestor_index ||
          ancestry_steps++ >= source_hierarchy.size()) {
        throw std::runtime_error(
            "Scene instance runtime GMS hierarchy is invalid");
      }
      const auto &ancestor = source_hierarchy[ancestor_index];
      if (ancestor.parent_directory_index &&
          *ancestor.parent_directory_index >= source_hierarchy.size()) {
        throw std::runtime_error(
            "Scene instance runtime GMS parent is out of range");
      }
      const auto expected_parent =
          ancestor.parent_directory_index
              ? runtime.source_handle(*ancestor.parent_directory_index)
              : runtime.root_handle();
      const auto ancestor_owner = runtime.source_handle(ancestor_index);
      const auto parent_resource = runtime.resource_parent(ancestor_owner);
      const auto actual_parent = parent_resource.value != 0
                                     ? runtime.associated_resource_owner(
                                           parent_resource)
                                     : std::optional<IntroRuntimeHandle>{};
      if (actual_parent != expected_parent ||
          parent_resource != runtime.resource_handle(expected_parent)) {
        throw std::runtime_error(
            "Scene instance authored ancestry differs from runtime parents");
      }
      if (ancestor_index == source_index) {
        parent_owner = actual_parent;
      }
      if (!ancestor.parent_directory_index) {
        break;
      }
      ancestor_index = *ancestor.parent_directory_index;
    }

    result.push_back({.scene_instance_index = instance_index,
                      .map_kind = instance.map_kind,
                      .map_layer_index = instance.map_layer_index,
                      .map_entry_index = instance.map_entry_index,
                      .geometry_role = instance.role,
                      .map_descriptor_offset =
                          instance.map_descriptor_offset,
                      .geometry_reference = instance.geometry_reference,
                      .source_directory_index = source_index,
                      .owner = owner,
                      .resource = resource,
                      .parent_owner = parent_owner});
  }
  return result;
}

} // namespace off::graphics
