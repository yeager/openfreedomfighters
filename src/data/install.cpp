#include "off/data/install.hpp"

#include "off/audio/decode.hpp"
#include "off/crypto/sha256.hpp"
#include "off/data/archive_vfs.hpp"
#include "off/data/audio_bank_header.hpp"
#include "off/data/gms_image.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/primitive_catalog.hpp"
#include "off/data/render_map.hpp"
#include "off/data/scene_support.hpp"
#include "off/data/texture_catalog.hpp"
#include "off/data/zgf_bundle.hpp"
#include "off/data/zip_archive.hpp"
#include "off/graphics/render_assets.hpp"
#include "off/graphics/render_preview.hpp"
#include "off/graphics/texture_decode.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace off::data {
namespace {

InstallVerification failure(InstallError error,
                            const std::filesystem::path &root,
                            std::string message) {
  return {
      .error = error,
      .root = root,
      .executable = {},
      .executable_sha256 = {},
      .message = std::move(message),
  };
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

} // namespace

InstallVerification verify_install(const std::filesystem::path &root) {
  std::error_code error;
  if (!std::filesystem::is_directory(root, error)) {
    return failure(InstallError::missing_root, root,
                   "game-data directory does not exist");
  }

  std::filesystem::path executable;
  for (const auto *name : {"Freedom.Exe", "Freedom.exe"}) {
    const auto candidate = root / name;
    if (std::filesystem::is_regular_file(candidate, error)) {
      executable = candidate;
      break;
    }
  }
  if (executable.empty()) {
    return failure(InstallError::missing_executable, root,
                   "Freedom.Exe was not found");
  }
  const auto size = std::filesystem::file_size(executable, error);
  if (error) {
    return failure(InstallError::io_error, root,
                   "could not read Freedom.Exe metadata");
  }
  if (size != supported_executable_size) {
    return failure(InstallError::unsupported_executable_size, root,
                   "Freedom.Exe is not the supported Steam build");
  }

  std::string digest;
  try {
    digest = crypto::to_hex(crypto::sha256_file(executable));
  } catch (const std::exception &) {
    return failure(InstallError::io_error, root, "could not hash Freedom.Exe");
  }
  if (digest != supported_executable_sha256) {
    auto result = failure(InstallError::unsupported_executable_hash, root,
                          "Freedom.Exe hash is not supported");
    result.executable = executable;
    result.executable_sha256 = digest;
    return result;
  }

  try {
    ArchiveVfs installation_vfs;
    const auto installation_mount = installation_vfs.mount_directory(root);
    static_cast<void>(installation_mount);
    constexpr std::array required_paths{
        "Scenes/StartLoader.ZIP",
        "Scenes/FF-StartUp.ZIP",
        "streams.wav",
    };
    for (const auto *relative : required_paths) {
      if (!installation_vfs.contains(relative)) {
        return failure(InstallError::incomplete_game_data, root,
                       std::string{"required game-data file is missing: "} +
                           relative);
      }
    }
    const auto global_stream = installation_vfs.open_stream("streams.wav");
    if (global_stream.size() == 0) {
      return failure(InstallError::incomplete_game_data, root,
                     "global audio stream bank is empty");
    }
    std::array<std::byte, 16> stream_probe{};
    global_stream.read_at(0, stream_probe);

    std::size_t audio_header_count = 0;
    std::size_t audio_record_count = 0;
    bool decoded_pcm_reference = false;
    bool decoded_ima_reference = false;
    bool decoded_vorbis_reference = false;
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(root / "Scenes")) {
      if (!entry.is_regular_file() ||
          lowercase(entry.path().extension().string()) != ".whd") {
        continue;
      }
      std::error_code relative_error;
      const auto relative =
          std::filesystem::relative(entry.path(), root, relative_error);
      if (relative_error) {
        throw std::runtime_error("could not resolve audio header path");
      }
      const auto header = AudioBankHeader::parse(
          installation_vfs.read(relative.generic_string()));
      auto local_bank = relative;
      local_bank.replace_extension(".WAV");
      const auto local_stream =
          installation_vfs.open_stream(local_bank.generic_string());
      header.validate_payload_ranges(local_stream.size(), global_stream.size());
      for (const auto &record : header.records()) {
        const auto format = record.format_flags & 0x7fffffffU;
        const auto needs_reference =
            (format == 1 && !decoded_pcm_reference) ||
            (format == 0x11 && !decoded_ima_reference) ||
            (format == 0x1000 && !decoded_vorbis_reference);
        if (!needs_reference) {
          continue;
        }
        const auto &bank =
            record.uses_global_bank() ? global_stream : local_stream;
        const auto decoded = audio::decode_stream(
            record, bank.read(record.data_offset, record.encoded_size));
        if (decoded.frame_count() == 0) {
          throw std::runtime_error(
              "audio reference stream decoded to no frames");
        }
        decoded_pcm_reference = decoded_pcm_reference || format == 1;
        decoded_ima_reference = decoded_ima_reference || format == 0x11;
        decoded_vorbis_reference = decoded_vorbis_reference || format == 0x1000;
      }
      ++audio_header_count;
      audio_record_count += header.records().size();
    }
    if (audio_header_count != 45 || audio_record_count != 121'187 ||
        !decoded_pcm_reference || !decoded_ima_reference ||
        !decoded_vorbis_reference) {
      return failure(InstallError::incomplete_game_data, root,
                     "audio header corpus does not match the supported build");
    }

    std::size_t scene_archive_count = 0;
    std::size_t scene_support_count = 0;
    std::size_t scene_graph_count = 0;
    std::size_t scene_graph_payload_bytes = 0;
    std::size_t stored_scene_graph_count = 0;
    std::size_t scene_graph_entry_count = 0;
    std::size_t scene_graph_entry_payload_bytes = 0;
    std::size_t ttf_entry_count = 0;
    std::size_t ppo_entry_count = 0;
    std::size_t decoded_gms_object_handle_count = 0;
    std::size_t local_gms_object_handle_count = 0;
    std::size_t external_gms_object_handle_count = 0;
    std::size_t gms_resource_count = 0;
    std::size_t gms_payload_bytes = 0;
    std::size_t gms_directory_entry_count = 0;
    std::size_t gms_identifier_count = 0;
    std::size_t gms_pool_group_count = 0;
    std::size_t buf_resource_count = 0;
    std::size_t gms_attachment_table_count = 0;
    std::size_t gms_attachment_count = 0;
    std::size_t gms_buf_auxiliary_count = 0;
    std::size_t texture_catalog_count = 0;
    std::size_t texture_image_count = 0;
    std::size_t texture_sequence_count = 0;
    std::size_t primitive_catalog_count = 0;
    std::size_t primitive_entry_count = 0;
    std::size_t primitive_reference_count = 0;
    std::size_t primitive_texture_reference_count = 0;
    std::size_t flagged_texture_selector_count = 0;
    std::size_t opaque_vertex_alpha_count = 0;
    std::size_t variable_vertex_alpha_count = 0;
    std::size_t fully_transparent_vertex_alpha_count = 0;
    std::size_t unflagged_variable_vertex_alpha_count = 0;
    std::size_t triangle_strip_primitive_count = 0;
    std::size_t line_list_primitive_count = 0;
    std::size_t render_draw_count = 0;
    std::size_t render_index_count = 0;
    std::size_t gms_primitive_reference_count = 0;
    std::size_t primitive_vertex_count = 0;
    std::size_t primitive_batch_count = 0;
    std::size_t primitive_index_count = 0;
    std::size_t render_map_count = 0;
    std::size_t render_map_entry_count = 0;
    std::size_t render_map_node_count = 0;
    std::size_t render_instance_map_count = 0;
    std::size_t render_instance_entry_count = 0;
    std::size_t render_instance_node_count = 0;
    std::size_t scene_resolution_count = 0;
    std::size_t primary_scene_resolution_count = 0;
    std::size_t secondary_scene_resolution_count = 0;
    std::size_t local_primitive_resolution_count = 0;
    std::size_t no_local_source_resolution_count = 0;
    std::size_t source_without_primitive_resolution_count = 0;
    std::size_t missing_primitive_resolution_count = 0;
    std::size_t unresolved_alias_resolution_count = 0;
    bool decoded_dxt1_reference = false;
    bool decoded_dxt3_reference = false;
    bool decoded_abgr_reference = false;
    bool decoded_palette_reference = false;
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(root / "Scenes")) {
      if (!entry.is_regular_file() ||
          lowercase(entry.path().extension().string()) != ".zip") {
        continue;
      }
      const auto archive = ZipArchive::open(entry.path());
      std::size_t support_files_in_archive = 0;
      std::size_t scene_graph_files_in_archive = 0;
      std::size_t gms_files_in_archive = 0;
      std::size_t buf_files_in_archive = 0;
      std::size_t texture_files_in_archive = 0;
      std::size_t primitive_files_in_archive = 0;
      std::size_t render_map_files_in_archive = 0;
      std::size_t render_instance_files_in_archive = 0;
      std::optional<GmsImage> gms_image;
      std::optional<std::vector<std::byte>> buf_resource;
      std::optional<TextureCatalog> texture_catalog;
      std::optional<PrimitiveCatalog> primitive_catalog;
      std::optional<RenderMap> render_map;
      std::optional<RenderMap> render_instance_map;
      std::unordered_set<std::uint32_t> primitive_indices;
      std::vector<std::uint32_t> scene_geometry_references;
      for (const auto &member : archive.entries()) {
        const auto extension =
            lowercase(std::filesystem::path(member.name).extension().string());
        if (extension == ".sup") {
          const auto support = SceneSupport::parse(archive.read(member));
          if (support.dependencies().empty()) {
            throw std::runtime_error("scene-support dependency list is empty");
          }
          ++support_files_in_archive;
          ++scene_support_count;
        } else if (extension == ".buf") {
          if (buf_resource.has_value()) {
            throw std::runtime_error(
                "scene archive contains multiple BUF resources");
          }
          buf_resource = archive.read(member);
          ++buf_files_in_archive;
          ++buf_resource_count;
        } else if (extension == ".zgf" || extension == ".gms") {
          auto resource = PackedResource::parse(archive.read(member));
          if (extension == ".zgf") {
            scene_graph_payload_bytes += resource.payload().size();
            stored_scene_graph_count +=
                resource.encoding() == PackedResourceEncoding::stored ? 1U : 0U;
            auto bundle = ZgfBundle::parse(std::move(resource));
            for (std::size_t index = 0; index < bundle.entries().size();
                 ++index) {
              const auto &bundled_entry = bundle.entries()[index];
              scene_graph_entry_payload_bytes +=
                  bundle.entry_payload(index).size();
              const auto bundled_extension =
                  lowercase(std::filesystem::path(bundled_entry.name)
                                .extension()
                                .string());
              ttf_entry_count += bundled_extension == ".ttf" ? 1U : 0U;
              ppo_entry_count += bundled_extension == ".ppo" ? 1U : 0U;
            }
            scene_graph_entry_count += bundle.entries().size();
            ++scene_graph_files_in_archive;
            ++scene_graph_count;
          } else {
            gms_payload_bytes += resource.payload().size();
            gms_image = GmsImage::parse(std::move(resource));
            gms_directory_entry_count += gms_image->directory().size();
            for (const auto &source : gms_image->directory()) {
              if (!GmsImage::source_class_name(source.source_type)
                       .has_value()) {
                throw std::runtime_error(
                    "GMS object source has an unknown geometry class");
              }
              if (!source.attachments.empty()) {
                ++gms_attachment_table_count;
                gms_attachment_count += source.attachments.size();
              }
              gms_buf_auxiliary_count +=
                  source.buf_auxiliary_offset != 0U ? 1U : 0U;
            }
            gms_identifier_count += gms_image->identifier_count();
            gms_pool_group_count += gms_image->pool_groups().size();
            ++gms_files_in_archive;
            ++gms_resource_count;
          }
        } else if (extension == ".tex") {
          auto catalog = TextureCatalog::parse(archive.read(member));
          for (const auto &image : catalog.images()) {
            bool *decoded_reference = nullptr;
            switch (image.encoding) {
            case TextureEncoding::dxt1:
              decoded_reference = &decoded_dxt1_reference;
              break;
            case TextureEncoding::dxt3:
              decoded_reference = &decoded_dxt3_reference;
              break;
            case TextureEncoding::abgr32:
              decoded_reference = &decoded_abgr_reference;
              break;
            case TextureEncoding::paletted8:
              decoded_reference = &decoded_palette_reference;
              break;
            }
            if (*decoded_reference) {
              continue;
            }
            const auto decoded = graphics::decode_texture_mip(image, 0);
            const auto required_bytes =
                static_cast<std::size_t>(decoded.width) * decoded.height * 4U;
            if (decoded.pixels.size() != required_bytes) {
              throw std::runtime_error(
                  "texture reference decoded to an invalid pixel count");
            }
            *decoded_reference = true;
          }
          texture_image_count += catalog.images().size();
          texture_sequence_count += catalog.sequences().size();
          texture_catalog = std::move(catalog);
          ++texture_files_in_archive;
          ++texture_catalog_count;
        } else if (extension == ".prm") {
          auto catalog = PrimitiveCatalog::parse(archive.read(member));
          for (const auto &primitive : catalog.entries()) {
            primitive_indices.insert(primitive.packed_index);
            if (primitive.flagged_reference) {
              ++primitive_reference_count;
              continue;
            }
            primitive_vertex_count += primitive.vertices.size();
            primitive_batch_count += primitive.batches.size();
            for (const auto &batch : primitive.batches) {
              primitive_index_count += batch.indices.size();
            }
          }
          primitive_entry_count += catalog.entries().size();
          primitive_catalog = std::move(catalog);
          ++primitive_files_in_archive;
          ++primitive_catalog_count;
        } else if (extension == ".rmc" || extension == ".rmi") {
          auto map = RenderMap::parse(archive.read(member));
          for (const auto &map_entry : map.entries()) {
            scene_geometry_references.push_back(
                map_entry.object.primary_geometry_reference);
            if (map_entry.object.secondary_geometry_reference != 0) {
              scene_geometry_references.push_back(
                  map_entry.object.secondary_geometry_reference);
            }
          }
          if (extension == ".rmc") {
            render_map_entry_count += map.entries().size();
            render_map_node_count += map.nodes().size();
            ++render_map_files_in_archive;
            ++render_map_count;
            render_map = std::move(map);
          } else {
            render_instance_entry_count += map.entries().size();
            render_instance_node_count += map.nodes().size();
            ++render_instance_files_in_archive;
            ++render_instance_map_count;
            render_instance_map = std::move(map);
          }
        }
      }
      if (support_files_in_archive != 1 || scene_graph_files_in_archive != 1 ||
          gms_files_in_archive != 1 || texture_files_in_archive != 1 ||
          primitive_files_in_archive != 1 || render_map_files_in_archive != 1 ||
          render_instance_files_in_archive != 1 || !gms_image.has_value() ||
          !texture_catalog.has_value() || !primitive_catalog.has_value() ||
          !render_map.has_value() || !render_instance_map.has_value()) {
        throw std::runtime_error("scene archive does not contain every "
                                 "required resource exactly once");
      }
      if (gms_image->directory().empty()) {
        if (buf_files_in_archive != 0) {
          throw std::runtime_error(
              "empty GMS image unexpectedly has a BUF resource");
        }
      } else {
        if (buf_files_in_archive != 1 || !buf_resource.has_value()) {
          throw std::runtime_error(
              "GMS object sources do not have exactly one BUF resource");
        }
        gms_image->validate_buf(*buf_resource);
      }
      for (const auto &source : gms_image->directory()) {
        if (!source.primitive_reference.has_value()) {
          continue;
        }
        if (!primitive_indices.contains(*source.primitive_reference)) {
          throw std::runtime_error(
              "GMS object source references a missing PRM primitive");
        }
        ++gms_primitive_reference_count;
      }
      const auto render_assets = graphics::RenderAssetBindings::build(
          primitive_catalog->entries(), texture_catalog->images());
      const auto count_scene_resolutions = [&](const RenderMap &map) {
        const auto resolutions = graphics::resolve_scene_geometry_references(
            primitive_catalog->entries(), gms_image->directory(),
            map.entries());
        scene_resolution_count += resolutions.size();
        for (const auto &resolution : resolutions) {
          if (resolution.role == graphics::SceneGeometryRole::primary) {
            ++primary_scene_resolution_count;
          } else {
            ++secondary_scene_resolution_count;
          }
          switch (resolution.status) {
          case graphics::SceneGeometryStatus::local_primitive:
            ++local_primitive_resolution_count;
            break;
          case graphics::SceneGeometryStatus::no_local_source:
            ++no_local_source_resolution_count;
            break;
          case graphics::SceneGeometryStatus::source_without_primitive:
            ++source_without_primitive_resolution_count;
            break;
          case graphics::SceneGeometryStatus::missing_primitive:
            ++missing_primitive_resolution_count;
            break;
          case graphics::SceneGeometryStatus::unresolved_primitive_alias:
            ++unresolved_alias_resolution_count;
            break;
          }
        }
      };
      count_scene_resolutions(*render_map);
      count_scene_resolutions(*render_instance_map);
      for (const auto &binding : render_assets.primitives()) {
        primitive_texture_reference_count +=
            binding.texture_image_index.has_value() ? 1U : 0U;
        flagged_texture_selector_count +=
            binding.texture_selector_flagged ? 1U : 0U;
        switch (binding.vertex_alpha_class) {
        case graphics::VertexAlphaClass::opaque:
          ++opaque_vertex_alpha_count;
          break;
        case graphics::VertexAlphaClass::variable:
          ++variable_vertex_alpha_count;
          unflagged_variable_vertex_alpha_count +=
              binding.texture_selector_flagged ? 0U : 1U;
          break;
        case graphics::VertexAlphaClass::fully_transparent:
          ++fully_transparent_vertex_alpha_count;
          break;
        }
        if (binding.topology == graphics::PrimitiveTopology::triangle_strip) {
          ++triangle_strip_primitive_count;
        } else {
          ++line_list_primitive_count;
        }
        render_draw_count += binding.draws.size();
        render_index_count += binding.indices.size();
      }
      for (const auto reference : scene_geometry_references) {
        static_cast<void>(GmsImage::decode_object_handle(reference));
        ++decoded_gms_object_handle_count;
        if (gms_image->local_source_for_handle(reference).has_value()) {
          ++local_gms_object_handle_count;
        } else {
          ++external_gms_object_handle_count;
        }
      }
      ++scene_archive_count;
    }
    if (scene_archive_count != 90 ||
        scene_support_count != scene_archive_count ||
        scene_graph_count != scene_archive_count ||
        scene_graph_payload_bytes != 34'221'064 ||
        stored_scene_graph_count != 2 || scene_graph_entry_count != 1'019 ||
        scene_graph_entry_payload_bytes != 34'161'792 ||
        ttf_entry_count != 430 || ppo_entry_count != 589 ||
        decoded_gms_object_handle_count != 3'002 ||
        local_gms_object_handle_count != 2'998 ||
        external_gms_object_handle_count != 4 ||
        gms_resource_count != scene_archive_count ||
        gms_payload_bytes != 33'436'872 ||
        gms_directory_entry_count != 179'838 ||
        gms_identifier_count != 154'941 || gms_pool_group_count != 29'450 ||
        buf_resource_count != 88 || gms_attachment_table_count != 34'218 ||
        gms_attachment_count != 39'885 || gms_buf_auxiliary_count != 5'765 ||
        texture_catalog_count != scene_archive_count ||
        texture_image_count != 23'522 || texture_sequence_count != 19 ||
        !decoded_dxt1_reference || !decoded_dxt3_reference ||
        !decoded_abgr_reference || !decoded_palette_reference ||
        primitive_catalog_count != scene_archive_count ||
        primitive_entry_count != 61'451 || primitive_reference_count != 27 ||
        primitive_texture_reference_count != 40'071 ||
        flagged_texture_selector_count != 18'731 ||
        opaque_vertex_alpha_count != 46'140 ||
        variable_vertex_alpha_count != 12'751 ||
        fully_transparent_vertex_alpha_count != 2'533 ||
        unflagged_variable_vertex_alpha_count != 0 ||
        triangle_strip_primitive_count != 57'284 ||
        line_list_primitive_count != 4'140 || render_draw_count != 461'344 ||
        render_index_count != 4'412'738 ||
        gms_primitive_reference_count != 115'977 ||
        primitive_vertex_count != 2'820'961 ||
        primitive_batch_count != 461'344 ||
        primitive_index_count != 4'412'738 ||
        render_map_count != scene_archive_count ||
        render_map_entry_count != 1'612 || render_map_node_count != 2'587 ||
        render_instance_map_count != scene_archive_count ||
        render_instance_entry_count != 1'189 ||
        render_instance_node_count != 1'359 ||
        scene_resolution_count != 3'002 ||
        primary_scene_resolution_count != 2'801 ||
        secondary_scene_resolution_count != 201 ||
        local_primitive_resolution_count != 220 ||
        no_local_source_resolution_count != 4 ||
        source_without_primitive_resolution_count != 2'778 ||
        missing_primitive_resolution_count != 0 ||
        unresolved_alias_resolution_count != 0) {
      return failure(
          InstallError::incomplete_game_data, root,
          "scene resource corpus does not match the supported build");
    }

    const auto startup_archive =
        ZipArchive::open(root / "Scenes/StartLoader.ZIP");
    const auto *scene_graph = startup_archive.find("SCENES/StartLoader.ZGF");
    if (startup_archive.entries().size() != 12 || scene_graph == nullptr) {
      return failure(
          InstallError::incomplete_game_data, root,
          "startup archive does not match the supported resource layout");
    }
    const auto payload = startup_archive.read(*scene_graph);
    static_cast<void>(ZgfBundle::parse(PackedResource::parse(payload)));
  } catch (const std::exception &exception) {
    return failure(InstallError::incomplete_game_data, root,
                   std::string{"game data failed integrity validation: "} +
                       exception.what());
  }

  return {
      .error = InstallError::none,
      .root = root,
      .executable = executable,
      .executable_sha256 = digest,
      .message = "supported Steam installation verified",
  };
}

} // namespace off::data
