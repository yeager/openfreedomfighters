#include "off/data/install.hpp"

#include "off/crypto/sha256.hpp"
#include "off/data/archive_vfs.hpp"
#include "off/data/byte_reader.hpp"
#include "off/data/zip_archive.hpp"

#include <array>
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
