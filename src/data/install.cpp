#include "off/data/install.hpp"

#include "off/audio/decode.hpp"
#include "off/crypto/sha256.hpp"
#include "off/data/archive_vfs.hpp"
#include "off/data/audio_bank_header.hpp"
#include "off/data/byte_reader.hpp"
#include "off/data/scene_support.hpp"
#include "off/data/texture_catalog.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <system_error>

namespace off::data {
namespace {

InstallVerification failure(
    InstallError error,
    const std::filesystem::path& root,
    std::string message
) {
    return {
        .error = error,
        .root = root,
        .executable = {},
        .executable_sha256 = {},
        .message = std::move(message),
    };
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

}  // namespace

InstallVerification verify_install(const std::filesystem::path& root) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        return failure(InstallError::missing_root, root, "game-data directory does not exist");
    }

    std::filesystem::path executable;
    for (const auto* name : {"Freedom.Exe", "Freedom.exe"}) {
        const auto candidate = root / name;
        if (std::filesystem::is_regular_file(candidate, error)) {
            executable = candidate;
            break;
        }
    }
    if (executable.empty()) {
        return failure(InstallError::missing_executable, root, "Freedom.Exe was not found");
    }
    const auto size = std::filesystem::file_size(executable, error);
    if (error) {
        return failure(InstallError::io_error, root, "could not read Freedom.Exe metadata");
    }
    if (size != supported_executable_size) {
        return failure(
            InstallError::unsupported_executable_size,
            root,
            "Freedom.Exe is not the supported Steam build"
        );
    }

    std::string digest;
    try {
        digest = crypto::to_hex(crypto::sha256_file(executable));
    } catch (const std::exception&) {
        return failure(InstallError::io_error, root, "could not hash Freedom.Exe");
    }
    if (digest != supported_executable_sha256) {
        auto result = failure(
            InstallError::unsupported_executable_hash,
            root,
            "Freedom.Exe hash is not supported"
        );
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
        for (const auto* relative : required_paths) {
            if (!installation_vfs.contains(relative)) {
                return failure(
                    InstallError::incomplete_game_data,
                    root,
                    std::string{"required game-data file is missing: "} + relative
                );
            }
        }
        const auto global_stream = installation_vfs.open_stream("streams.wav");
        if (global_stream.size() == 0) {
            return failure(
                InstallError::incomplete_game_data,
                root,
                "global audio stream bank is empty"
            );
        }
        std::array<std::byte, 16> stream_probe{};
        global_stream.read_at(0, stream_probe);

        std::size_t audio_header_count = 0;
        std::size_t audio_record_count = 0;
        bool decoded_pcm_reference = false;
        bool decoded_ima_reference = false;
        bool decoded_vorbis_reference = false;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "Scenes")) {
            if (!entry.is_regular_file() || lowercase(entry.path().extension().string()) != ".whd") {
                continue;
            }
            std::error_code relative_error;
            const auto relative = std::filesystem::relative(entry.path(), root, relative_error);
            if (relative_error) {
                throw std::runtime_error("could not resolve audio header path");
            }
            const auto header = AudioBankHeader::parse(
                installation_vfs.read(relative.generic_string())
            );
            auto local_bank = relative;
            local_bank.replace_extension(".WAV");
            const auto local_stream = installation_vfs.open_stream(local_bank.generic_string());
            header.validate_payload_ranges(local_stream.size(), global_stream.size());
            for (const auto& record : header.records()) {
                const auto format = record.format_flags & 0x7fffffffU;
                const auto needs_reference =
                    (format == 1 && !decoded_pcm_reference) ||
                    (format == 0x11 && !decoded_ima_reference) ||
                    (format == 0x1000 && !decoded_vorbis_reference);
                if (!needs_reference) {
                    continue;
                }
                const auto& bank = record.uses_global_bank() ? global_stream : local_stream;
                const auto decoded = audio::decode_stream(
                    record,
                    bank.read(record.data_offset, record.encoded_size)
                );
                if (decoded.frame_count() == 0) {
                    throw std::runtime_error("audio reference stream decoded to no frames");
                }
                decoded_pcm_reference = decoded_pcm_reference || format == 1;
                decoded_ima_reference = decoded_ima_reference || format == 0x11;
                decoded_vorbis_reference = decoded_vorbis_reference || format == 0x1000;
            }
            ++audio_header_count;
            audio_record_count += header.records().size();
        }
        if (audio_header_count != 45 || audio_record_count != 121'187 ||
            !decoded_pcm_reference || !decoded_ima_reference || !decoded_vorbis_reference) {
            return failure(
                InstallError::incomplete_game_data,
                root,
                "audio header corpus does not match the supported build"
            );
        }

        std::size_t scene_archive_count = 0;
        std::size_t scene_support_count = 0;
        std::size_t texture_catalog_count = 0;
        std::size_t texture_image_count = 0;
        std::size_t texture_sequence_count = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "Scenes")) {
            if (!entry.is_regular_file() || lowercase(entry.path().extension().string()) != ".zip") {
                continue;
            }
            const auto archive = ZipArchive::open(entry.path());
            std::size_t support_files_in_archive = 0;
            std::size_t texture_files_in_archive = 0;
            for (const auto& member : archive.entries()) {
                const auto extension = lowercase(
                    std::filesystem::path(member.name).extension().string()
                );
                if (extension == ".sup") {
                    const auto support = SceneSupport::parse(archive.read(member));
                    if (support.dependencies().empty()) {
                        throw std::runtime_error("scene-support dependency list is empty");
                    }
                    ++support_files_in_archive;
                    ++scene_support_count;
                } else if (extension == ".tex") {
                    const auto catalog = TextureCatalog::parse(archive.read(member));
                    texture_image_count += catalog.images().size();
                    texture_sequence_count += catalog.sequences().size();
                    ++texture_files_in_archive;
                    ++texture_catalog_count;
                }
            }
            if (support_files_in_archive != 1 || texture_files_in_archive != 1) {
                throw std::runtime_error(
                    "scene archive does not contain exactly one support and texture file"
                );
            }
            ++scene_archive_count;
        }
        if (scene_archive_count != 90 || scene_support_count != scene_archive_count ||
            texture_catalog_count != scene_archive_count || texture_image_count != 23'522 ||
            texture_sequence_count != 19) {
            return failure(
                InstallError::incomplete_game_data,
                root,
                "scene resource corpus does not match the supported build"
            );
        }

        const auto startup_archive = ZipArchive::open(root / "Scenes/StartLoader.ZIP");
        const auto* scene_graph = startup_archive.find("SCENES/StartLoader.ZGF");
        if (startup_archive.entries().size() != 12 || scene_graph == nullptr) {
            return failure(
                InstallError::incomplete_game_data,
                root,
                "startup archive does not match the supported resource layout"
            );
        }
        const auto payload = startup_archive.read(*scene_graph);
        const ByteReader payload_reader(payload);
        if (payload_reader.u32(4) != payload.size()) {
            return failure(
                InstallError::incomplete_game_data,
                root,
                "startup scene graph failed structural validation"
            );
        }
    } catch (const std::exception& exception) {
        return failure(
            InstallError::incomplete_game_data,
            root,
            std::string{"game data failed integrity validation: "} + exception.what()
        );
    }

    return {
        .error = InstallError::none,
        .root = root,
        .executable = executable,
        .executable_sha256 = digest,
        .message = "supported Steam installation verified",
    };
}

}  // namespace off::data
