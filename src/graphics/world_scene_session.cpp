#include "off/graphics/world_scene_session.hpp"

#include "off/data/packed_resource.hpp"
#include "off/data/primitive_catalog.hpp"
#include "off/data/render_map.hpp"
#include "off/data/scene_package_family.hpp"
#include "off/data/scene_support.hpp"
#include "off/data/texture_catalog.hpp"
#include "off/data/zgf_bundle.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace off::graphics {
namespace {

[[nodiscard]] WorldSceneSessionId next_world_scene_session_id() {
  static std::atomic<std::uint64_t> next{1U};
  const auto value = next.fetch_add(1U, std::memory_order_relaxed);
  if (value == 0U)
    throw std::runtime_error("world scene session identity space is exhausted");
  return {.value = value};
}

[[nodiscard]] std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

[[nodiscard]] bool is_ancestor(
    std::span<const data::GmsHierarchyNode> hierarchy,
    std::size_t ancestor, std::size_t source) {
  if (ancestor >= hierarchy.size() || source >= hierarchy.size())
    return false;
  auto current = source;
  for (std::size_t step = 0; step <= hierarchy.size(); ++step) {
    if (current == ancestor)
      return true;
    const auto parent = hierarchy[current].parent_directory_index;
    if (!parent)
      return false;
    if (*parent >= hierarchy.size())
      return false;
    current = *parent;
  }
  return false;
}

[[nodiscard]] std::filesystem::path checked_owned_archive_path(
    const std::filesystem::path &install_root,
    const std::filesystem::path &relative_archive_path) {
  if (relative_archive_path.empty() || relative_archive_path.has_root_path() ||
      lowercase(relative_archive_path.extension().string()) != ".zip") {
    throw std::runtime_error("world scene archive path is invalid");
  }
  for (const auto &component : relative_archive_path) {
    if (component == "..")
      throw std::runtime_error("world scene archive path is invalid");
  }

  std::error_code error;
  const auto scenes_root = install_root / "Scenes";
  const auto scenes_status = std::filesystem::symlink_status(scenes_root, error);
  if (error || std::filesystem::is_symlink(scenes_status) ||
      !std::filesystem::is_directory(scenes_status)) {
    throw std::runtime_error("world scene archive is unavailable");
  }
  const auto canonical_scenes_root = std::filesystem::canonical(scenes_root, error);
  if (error)
    throw std::runtime_error("world scene archive is unavailable");
  const auto archive_path =
      canonical_scenes_root / relative_archive_path.lexically_normal();
  const auto status = std::filesystem::symlink_status(archive_path, error);
  if (error || std::filesystem::is_symlink(status) ||
      !std::filesystem::is_regular_file(status)) {
    throw std::runtime_error("world scene archive is unavailable");
  }
  const auto canonical_archive = std::filesystem::canonical(archive_path, error);
  if (error || canonical_archive != archive_path.lexically_normal()) {
    throw std::runtime_error("world scene archive is unavailable");
  }
  return archive_path;
}

} // namespace

void validate_world_scene_asset_provenance(
    const SceneRenderAsset &asset,
    std::span<const data::GmsDirectoryEntry> source_directory,
    std::span<const data::GmsHierarchyNode> source_hierarchy) {
  validate_scene_render_asset(asset);
  if (source_hierarchy.size() != source_directory.size()) {
    throw std::runtime_error(
        "world scene source hierarchy does not match its GMS directory");
  }
  for (std::size_t row = 0; row < source_hierarchy.size(); ++row) {
    const auto &node = source_hierarchy[row];
    if (node.directory_index != row ||
        (node.parent_directory_index &&
         *node.parent_directory_index >= source_directory.size())) {
      throw std::runtime_error("world scene GMS hierarchy is inconsistent");
    }
  }

  for (const auto &instance : asset.instances) {
    const auto source_index = instance.source_directory_index;
    if (source_index >= source_directory.size() ||
        !instance.map_source_directory_index ||
        *instance.map_source_directory_index >= source_directory.size()) {
      throw std::runtime_error(
          "world scene candidate references a missing GMS source row");
    }
    const auto &source = source_directory[source_index];
    const auto &node = source_hierarchy[source_index];
    if (instance.source_type != source.source_type ||
        instance.source_local_slot_index != source.local_slot_index ||
        instance.source_basis != source.basis ||
        instance.source_position != source.position ||
        instance.parent_source_directory_index != node.parent_directory_index ||
        (!source_hierarchy.empty() && !instance.source_hierarchy_composed)) {
      throw std::runtime_error(
          "world scene candidate does not match its GMS source provenance");
    }

    const auto &resolution = asset.resolutions.at(instance.resolution_index);
    const auto &geometry = resolution.geometry;
    const auto map_source_index = *instance.map_source_directory_index;
    if (source_directory[map_source_index].local_slot_index !=
            geometry.requested_handle_slot_index ||
        !is_ancestor(source_hierarchy, map_source_index, source_index)) {
      throw std::runtime_error(
          "world scene map handle does not select the candidate's GMS ancestry");
    }
  }
}

std::vector<WorldSceneSourceOwner> build_world_scene_source_owner_table(
    std::span<const data::GmsDirectoryEntry> source_directory,
    std::span<const data::GmsHierarchyNode> source_hierarchy,
    WorldSceneSessionId session_id) {
  if (session_id.value == 0U ||
      source_hierarchy.size() != source_directory.size()) {
    throw std::invalid_argument(
        "world scene owner table has invalid session or hierarchy scope");
  }
  std::vector<WorldSceneSourceOwner> result;
  result.reserve(source_directory.size());
  for (std::size_t row = 0; row < source_directory.size(); ++row) {
    const auto &node = source_hierarchy[row];
    if (node.directory_index != row ||
        (node.parent_directory_index &&
         *node.parent_directory_index >= source_directory.size())) {
      throw std::invalid_argument(
          "world scene owner table has invalid authored ancestry");
    }
    const WorldSceneOwnerId id{.session = session_id,
                               .source_directory_index = row};
    std::optional<WorldSceneOwnerId> parent_owner;
    if (node.parent_directory_index) {
      parent_owner = WorldSceneOwnerId{
          .session = session_id,
          .source_directory_index = *node.parent_directory_index};
    }
    result.push_back({.id = id,
                      .parent_owner = parent_owner,
                      .source_type = source_directory[row].source_type,
                      .local_slot_index =
                          source_directory[row].local_slot_index});
  }
  return result;
}

void bind_world_scene_candidate_owners(
    SceneGpuPlan &plan,
    std::span<const WorldSceneSourceOwner> source_owners,
    WorldSceneSessionId session_id) {
  if (session_id.value == 0U || !plan.source_only_diagnostic) {
    throw std::invalid_argument(
        "world scene candidate binding requires a scoped diagnostic plan");
  }
  for (std::size_t row = 0; row < source_owners.size(); ++row) {
    const auto &owner = source_owners[row];
    if (owner.id.session != session_id ||
        owner.id.source_directory_index != row ||
        (owner.parent_owner &&
         (owner.parent_owner->session != session_id ||
          owner.parent_owner->source_directory_index >= source_owners.size() ||
          source_owners[owner.parent_owner->source_directory_index].id !=
              *owner.parent_owner))) {
      throw std::invalid_argument(
          "world scene owner table contains a foreign or reordered identity");
    }
  }
  for (auto &instance : plan.instances) {
    const auto map_row = instance.map_source_directory_index;
    const auto source_row = instance.source_directory_index;
    if (!map_row || *map_row >= source_owners.size() ||
        source_row >= source_owners.size()) {
      throw std::invalid_argument(
          "world scene GPU candidate references an absent source owner");
    }
    const auto &geometry_owner = source_owners[source_row];
    if (geometry_owner.id.session != session_id ||
        geometry_owner.id.source_directory_index != source_row ||
        geometry_owner.parent_owner !=
            (instance.parent_source_directory_index
                 ? std::optional<WorldSceneOwnerId>{WorldSceneOwnerId{
                       .session = session_id,
                       .source_directory_index =
                           *instance.parent_source_directory_index}}
                 : std::optional<WorldSceneOwnerId>{})) {
      throw std::invalid_argument(
          "world scene GPU candidate parent owner does not match its GMS row");
    }
    auto ancestor = geometry_owner.id;
    bool map_owner_is_ancestor = false;
    for (std::size_t step = 0; step <= source_owners.size(); ++step) {
      if (ancestor == source_owners[*map_row].id) {
        map_owner_is_ancestor = true;
        break;
      }
      const auto parent = source_owners[ancestor.source_directory_index].parent_owner;
      if (!parent)
        break;
      if (parent->source_directory_index >= source_owners.size())
        break;
      ancestor = *parent;
    }
    if (!map_owner_is_ancestor) {
      throw std::invalid_argument(
          "world scene map owner is not in the candidate geometry ancestry");
    }
    instance.map_owner_id = source_owners[*map_row].id;
    instance.geometry_owner_id = geometry_owner.id;
    instance.geometry_parent_owner_id = geometry_owner.parent_owner;
  }
  validate_scene_gpu_plan(plan);
}

struct WorldSceneSession::Storage {
  data::ScenePackageFamily package;
  data::GmsImage sources;
  WorldSceneSessionId identity;
  std::vector<WorldSceneSourceOwner> source_owners;
  SceneGpuPlan plan;
  SceneRenderResolutionSummary summary;
};

WorldSceneSession::WorldSceneSession(std::unique_ptr<Storage> storage) noexcept
    : storage_(std::move(storage)) {}
WorldSceneSession::WorldSceneSession(WorldSceneSession &&) noexcept = default;
WorldSceneSession &WorldSceneSession::operator=(WorldSceneSession &&) noexcept =
    default;
WorldSceneSession::~WorldSceneSession() = default;

WorldSceneSession WorldSceneSession::load_owned_checked(
    const std::filesystem::path &install_root,
    const std::filesystem::path &relative_archive_path) {
  const auto archive_path =
      checked_owned_archive_path(install_root, relative_archive_path);
  const auto expected_stem = relative_archive_path.stem().string();
  auto package = data::ScenePackageFamily::open_complete_checked(
      archive_path, expected_stem);

  const auto zgf_bytes = package.read(data::SceneResourceKind::zgf);
  static_cast<void>(data::ZgfBundle::parse(
      data::PackedResource::parse(zgf_bytes)));
  const auto support = data::SceneSupport::parse(
      package.read(data::SceneResourceKind::sup));
  if (support.dependencies().empty()) {
    throw std::runtime_error("world scene support has no dependencies");
  }

  const auto primitives = data::PrimitiveCatalog::parse(
      package.read(data::SceneResourceKind::prm));
  const auto textures = data::TextureCatalog::parse(
      package.read(data::SceneResourceKind::tex));
  auto sources = data::GmsImage::parse(data::PackedResource::parse(
      package.read(data::SceneResourceKind::gms)));
  if (package.optional_member(data::SceneResourceKind::buf) != nullptr) {
    sources.validate_buf(package.read(data::SceneResourceKind::buf));
  } else if (!sources.directory().empty()) {
    throw std::runtime_error("world scene GMS sources have no BUF resource");
  }

  const auto rmc = data::RenderMap::parse(
      package.read(data::SceneResourceKind::rmc));
  const auto rmi = data::RenderMap::parse(
      package.read(data::SceneResourceKind::rmi));
  const std::array maps{
      SceneRenderMapView{.kind = SceneRenderMapKind::rmc,
                         .entries = rmc.entries()},
      SceneRenderMapView{.kind = SceneRenderMapKind::rmi,
                         .entries = rmi.entries()},
  };
  auto asset = build_scene_render_asset(
      primitives.entries(), textures.images(), sources.directory(),
      sources.hierarchy(), maps);
  if (package.optional_member(data::SceneResourceKind::anm) != nullptr) {
    asset.animation = data::AnimationImage::parse(
        package.read(data::SceneResourceKind::anm));
  }
  validate_world_scene_asset_provenance(
      asset, sources.directory(), sources.hierarchy());
  if (asset.instances.empty()) {
    throw std::runtime_error(
        "selected world scene has no source-backed geometry candidates");
  }

  const auto identity = next_world_scene_session_id();
  auto source_owners = build_world_scene_source_owner_table(
      sources.directory(), sources.hierarchy(), identity);
  const auto summary = summarize_scene_render_resolutions(asset);
  auto plan = prepare_scene_gpu_plan(std::move(asset));
  bind_world_scene_candidate_owners(plan, source_owners, identity);
  auto storage = std::make_unique<Storage>(Storage{
      .package = std::move(package),
      .sources = std::move(sources),
      .identity = identity,
      .source_owners = std::move(source_owners),
      .plan = std::move(plan),
      .summary = summary,
  });
  return WorldSceneSession(std::move(storage));
}

const SceneGpuPlan &WorldSceneSession::diagnostic_plan() const noexcept {
  return storage_->plan;
}

std::span<const SceneGpuInstance>
WorldSceneSession::candidate_instances() const noexcept {
  return storage_->plan.instances;
}

std::span<const data::GmsDirectoryEntry>
WorldSceneSession::source_directory() const noexcept {
  return storage_->sources.directory();
}

std::span<const data::GmsHierarchyNode>
WorldSceneSession::source_hierarchy() const noexcept {
  return storage_->sources.hierarchy();
}

WorldSceneSessionId WorldSceneSession::identity() const noexcept {
  return storage_->identity;
}

std::span<const WorldSceneSourceOwner>
WorldSceneSession::source_owners() const noexcept {
  return storage_->source_owners;
}

const SceneRenderResolutionSummary &
WorldSceneSession::resolution_summary() const noexcept {
  return storage_->summary;
}

} // namespace off::graphics
