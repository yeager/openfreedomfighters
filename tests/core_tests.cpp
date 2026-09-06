#include "off/crypto/sha256.hpp"
#include "off/data/archive_vfs.hpp"
#include "off/data/install.hpp"
#include "off/data/zip_archive.hpp"
#include "off/mode.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void append_u16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xffU));
    bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
}

void append_text(std::vector<std::byte>& bytes, std::string_view value) {
    const auto source = std::as_bytes(std::span{value.data(), value.size()});
    bytes.insert(bytes.end(), source.begin(), source.end());
}

std::vector<std::byte> raw_deflate(std::span<const std::byte> input) {
    std::vector<std::byte> output(compressBound(static_cast<uLong>(input.size())));
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    if (deflateInit2(
            &stream,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -MAX_WBITS,
            8,
            Z_DEFAULT_STRATEGY
        ) != Z_OK) {
        throw std::runtime_error("test deflate initialization failed");
    }
    const auto result = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (result != Z_STREAM_END) {
        throw std::runtime_error("test deflate failed");
    }
    output.resize(stream.total_out);
    return output;
}

void write_test_zip(
    const std::filesystem::path& path,
    std::string_view name,
    std::string_view payload,
    std::uint16_t method,
    bool glacier_footer
) {
    const auto source = std::as_bytes(std::span{payload.data(), payload.size()});
    const auto compressed = method == 8
        ? raw_deflate(source)
        : std::vector<std::byte>{source.begin(), source.end()};
    const auto crc = static_cast<std::uint32_t>(::crc32(
        0,
        reinterpret_cast<const Bytef*>(source.data()),
        static_cast<uInt>(source.size())
    ));
    std::vector<std::byte> archive;
    append_u32(archive, 0x04034b50U);
    append_u16(archive, 20);
    append_u16(archive, 0);
    append_u16(archive, method);
    append_u16(archive, 0);
    append_u16(archive, 0);
    append_u32(archive, crc);
    append_u32(archive, static_cast<std::uint32_t>(compressed.size()));
    append_u32(archive, static_cast<std::uint32_t>(source.size()));
    append_u16(archive, static_cast<std::uint16_t>(name.size()));
    append_u16(archive, 0);
    append_text(archive, name);
    archive.insert(archive.end(), compressed.begin(), compressed.end());

    const auto central_offset = static_cast<std::uint32_t>(archive.size());
    append_u32(archive, 0x02014b50U);
    append_u16(archive, 20);
    append_u16(archive, 20);
    append_u16(archive, 0);
    append_u16(archive, method);
    append_u16(archive, 0);
    append_u16(archive, 0);
    append_u32(archive, crc);
    append_u32(archive, static_cast<std::uint32_t>(compressed.size()));
    append_u32(archive, static_cast<std::uint32_t>(source.size()));
    append_u16(archive, static_cast<std::uint16_t>(name.size()));
    append_u16(archive, 0);
    append_u16(archive, 0);
    append_u16(archive, 0);
    append_u16(archive, 0);
    append_u32(archive, 0);
    append_u32(archive, 0);
    append_text(archive, name);
    const auto central_size = static_cast<std::uint32_t>(archive.size()) - central_offset;
    append_u32(archive, 0x06054b50U);
    append_u16(archive, 0);
    append_u16(archive, 0);
    append_u16(archive, 1);
    append_u16(archive, 1);
    append_u32(archive, central_size);
    append_u32(archive, central_offset);
    append_u16(archive, 0);
    if (glacier_footer) {
        archive.insert(archive.end(), 22, std::byte{0});
    }
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(archive.data()), static_cast<std::streamsize>(archive.size()));
    if (!output) {
        throw std::runtime_error("could not write synthetic ZIP fixture");
    }
}

void test_zip_reader() {
    const std::filesystem::path work = OFF_TEST_WORK_DIR;
    std::error_code error;
    std::filesystem::remove_all(work, error);
    std::filesystem::create_directories(work);

    for (const auto method : {std::uint16_t{0}, std::uint16_t{8}}) {
        const auto path = work / (method == 0 ? "stored.zip" : "deflated.zip");
        write_test_zip(path, "SCENES/Test.ZGF", "synthetic-payload", method, method == 8);
        const auto archive = off::data::ZipArchive::open(path);
        check(archive.entries().size() == 1, "read synthetic ZIP directory");
        const auto* entry = archive.find("scenes\\test.zgf");
        check(entry != nullptr, "case-insensitive normalized ZIP lookup");
        if (entry != nullptr) {
            const auto bytes = archive.read(*entry);
            const std::string value(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            check(value == "synthetic-payload", "read and CRC-check ZIP payload");
        }
    }
    write_test_zip(work / "unsafe.zip", "../escape", "x", 0, false);
    bool rejected = false;
    try {
        static_cast<void>(off::data::ZipArchive::open(work / "unsafe.zip"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, "reject archive traversal path");
    check(!off::data::is_safe_archive_path("/absolute"), "reject absolute archive path");
    check(!off::data::is_safe_archive_path("C:/drive"), "reject drive archive path");
    check(!off::data::is_safe_archive_path("empty//segment"), "reject empty archive path segment");

    const auto directory_path = work / "directory.zip";
    write_test_zip(directory_path, "SCENES/", "", 0, false);
    check(
        off::data::ZipArchive::open(directory_path).entries().empty(),
        "accept and omit a safe empty ZIP directory entry"
    );
    const auto directory_data_path = work / "directory-data.zip";
    write_test_zip(directory_data_path, "SCENES/", "x", 0, false);
    bool directory_data_rejected = false;
    try {
        static_cast<void>(off::data::ZipArchive::open(directory_data_path));
    } catch (const std::runtime_error&) {
        directory_data_rejected = true;
    }
    check(directory_data_rejected, "reject ZIP directory entries containing data");

    const auto corrupt_path = work / "corrupt.zip";
    constexpr std::string_view corrupt_name = "SCENES/Corrupt.ZGF";
    write_test_zip(corrupt_path, corrupt_name, "synthetic-payload", 0, false);
    {
        std::fstream corrupt(corrupt_path, std::ios::binary | std::ios::in | std::ios::out);
        corrupt.seekp(static_cast<std::streamoff>(30 + corrupt_name.size()));
        corrupt.put('X');
    }
    bool crc_rejected = false;
    try {
        const auto archive = off::data::ZipArchive::open(corrupt_path);
        static_cast<void>(archive.read(archive.entries().front()));
    } catch (const std::runtime_error&) {
        crc_rejected = true;
    }
    check(crc_rejected, "reject corrupted ZIP payload by CRC");

    const auto base_path = work / "base.zip";
    const auto override_path = work / "override.zip";
    const auto loose_path = work / "loose";
    std::filesystem::create_directories(loose_path / "Scenes");
    {
        std::ofstream loose(loose_path / "Scenes/Shared.ZGF", std::ios::binary);
        loose << "loose";
    }
    {
        std::ofstream stream(loose_path / "Stream.bin", std::ios::binary);
        stream << "0123456789abcdef";
    }
    write_test_zip(base_path, "SCENES/Shared.ZGF", "base", 0, false);
    write_test_zip(override_path, "scenes/shared.zgf", "override", 8, true);
    off::data::ArchiveVfs vfs;
    const auto loose_mount = vfs.mount_directory(loose_path);
    const auto base_mount = vfs.mount_archive(base_path);
    const auto override_mount = vfs.mount_archive(override_path);
    check(vfs.mount_count() == 3, "mount directories and archives in the VFS");
    check(vfs.contains("Scenes\\Shared.zgf"), "find a normalized VFS path");
    const auto stream = vfs.open_stream("stream.BIN");
    check(stream.size() == 16, "report the streaming file size");
    std::array<std::byte, 4> range{};
    stream.read_at(3, range);
    const std::string range_value(reinterpret_cast<const char*>(range.data()), range.size());
    check(range_value == "3456", "read a bounded range from a loose file");
    bool range_rejected = false;
    try {
        stream.read_at(15, range);
    } catch (const std::runtime_error&) {
        range_rejected = true;
    }
    check(range_rejected, "reject a streaming read beyond end of file");
    {
        std::ofstream changed(loose_path / "Stream.bin", std::ios::binary | std::ios::app);
        changed << '!';
    }
    bool changed_source_rejected = false;
    try {
        stream.read_at(0, range);
    } catch (const std::runtime_error&) {
        changed_source_rejected = true;
    }
    check(changed_source_rejected, "reject a streaming source changed after mount");
    bool archive_stream_rejected = false;
    try {
        static_cast<void>(vfs.open_stream("Scenes/Shared.ZGF"));
    } catch (const std::runtime_error&) {
        archive_stream_rejected = true;
    }
    check(archive_stream_rejected, "do not bypass an overlaid archive for streaming");
    const auto read_text = [&vfs]() {
        const auto bytes = vfs.read("SCENES/SHARED.ZGF");
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    check(read_text() == "override", "prefer the most recently mounted archive");
    check(vfs.unmount(override_mount), "unmount the active scene archive");
    check(read_text() == "base", "reveal the previous archive after unmount");
    check(vfs.unmount(base_mount), "unmount the base archive");
    check(read_text() == "loose", "fall back to the loose-file mount");
    check(vfs.open_stream("Scenes/Shared.ZGF").size() == 5, "stream the revealed loose file");
    check(!vfs.unmount(base_mount), "reject a duplicate unmount");
    check(vfs.unmount(loose_mount), "unmount the loose-file directory");
    check(!vfs.contains("Scenes/Shared.ZGF"), "remove paths with their mount");
    vfs.clear();

    const auto excluded_root = work / "excluded-loose";
    std::filesystem::create_directories(excluded_root / "SoundTrack");
    std::filesystem::create_directories(excluded_root / "Scenes/SoundTrack");
    {
        std::ofstream(excluded_root / "SoundTrack/track.bin", std::ios::binary) << "album";
        std::ofstream(excluded_root / "Scenes/SoundTrack/keep.bin", std::ios::binary) << "game";
    }
    const std::array<std::string_view, 1> exclusions{"soundtrack"};
    off::data::ArchiveVfs filtered;
    static_cast<void>(filtered.mount_directory(excluded_root, exclusions));
    check(!filtered.contains("SoundTrack/track.bin") &&
              filtered.contains("Scenes/SoundTrack/keep.bin"),
          "case-folded exclusion omits only the named top-level subtree");
    bool excluded_read_rejected = false;
    try { static_cast<void>(filtered.open_stream("SoundTrack/track.bin")); }
    catch (const std::runtime_error&) { excluded_read_rejected = true; }
    check(excluded_read_rejected, "excluded files cannot be opened through the VFS");
    off::data::ArchiveVfs unfiltered;
    static_cast<void>(unfiltered.mount_directory(excluded_root));
    check(unfiltered.contains("soundtrack/track.bin"), "default mount still indexes optional directories");

    // Sparse fixture exceeds the per-file limit without allocating its payload.
    std::filesystem::resize_file(excluded_root / "SoundTrack/track.bin", 256ULL * 1024ULL * 1024ULL + 1);
    off::data::ArchiveVfs without_large_optional;
    static_cast<void>(without_large_optional.mount_directory(excluded_root, exclusions));
    check(without_large_optional.contains("Scenes/SoundTrack/keep.bin"),
          "excluded payload sizes do not consume VFS limits");
    bool default_limit_rejected = false;
    try { static_cast<void>(unfiltered.mount_directory(excluded_root)); }
    catch (const std::runtime_error&) { default_limit_rejected = true; }
    check(default_limit_rejected, "default mount retains its file-size safety limit");

    for (const auto invalid : std::array<std::string_view, 10>{
             "", ".", "..", "../SoundTrack", "/SoundTrack", "SoundTrack/child",
             "SoundTrack\\child", "SoundTrack/", "C:SoundTrack", std::string_view{"a\0b", 3}}) {
        off::data::ArchiveVfs rejected_mount;
        const std::array<std::string_view, 1> names{invalid};
        bool invalid_rejected = false;
        try { static_cast<void>(rejected_mount.mount_directory(excluded_root, names)); }
        catch (const std::runtime_error&) { invalid_rejected = true; }
        check(invalid_rejected && rejected_mount.mount_count() == 0,
              "invalid top-level exclusion rejects without publishing a mount");
    }

    const auto symlink_root = work / "excluded-links";
    std::filesystem::create_directories(symlink_root);
    std::ofstream(symlink_root / "keep.bin", std::ios::binary) << "kept";
    std::error_code directory_link_error, file_link_error;
    std::filesystem::create_directory_symlink(
        std::filesystem::absolute(excluded_root / "SoundTrack"),
        symlink_root / "DirectoryLink", directory_link_error);
    std::filesystem::create_symlink(
        std::filesystem::absolute(work / "missing-link-target"),
        symlink_root / "FileLink", file_link_error);
    const std::array<std::string_view, 2> excluded_links{"directorylink", "FILELINK"};
    off::data::ArchiveVfs without_links;
    static_cast<void>(without_links.mount_directory(symlink_root, excluded_links));
    check(without_links.contains("keep.bin") && !without_links.contains("DirectoryLink/track.bin") &&
              !without_links.contains("FileLink"),
          "excluded directory and dangling file links are not followed or indexed");
    if (!directory_link_error || !file_link_error) {
        bool default_symlink_rejected = false;
        try { static_cast<void>(unfiltered.mount_directory(symlink_root)); }
        catch (const std::runtime_error&) { default_symlink_rejected = true; }
        check(default_symlink_rejected, "default mount still rejects symbolic links");
    }
    if (directory_link_error || file_link_error) {
        std::cout << "SKIP: unavailable directory/file symlink fixture: "
                  << directory_link_error.message() << "; " << file_link_error.message() << '\n';
    }
}

}  // namespace

int main() {
    check(
        off::crypto::to_hex(off::crypto::sha256("")) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA-256 empty vector"
    );
    check(
        off::crypto::to_hex(off::crypto::sha256("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc vector"
    );
    check(off::parse_mode("original") == off::Mode::original, "parse Original mode");
    check(off::parse_mode("modern") == off::Mode::modern, "parse Modern mode");
    check(!off::parse_mode("other"), "reject unknown mode");
    const auto missing = off::data::verify_install("path-that-must-not-exist");
    check(
        missing.error == off::data::InstallError::missing_root,
        "reject missing game-data directory"
    );
    test_zip_reader();
    return failures == 0 ? 0 : 1;
}
