#include "off/data/zip_archive.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_set>

#include <zlib.h>

namespace off::data {
namespace {

constexpr std::uint32_t local_signature = 0x04034b50U;
constexpr std::uint32_t central_signature = 0x02014b50U;
constexpr std::uint32_t end_signature = 0x06054b50U;
constexpr std::size_t maximum_comment = 65'535;
constexpr std::size_t maximum_trailing_data = 4'096;
constexpr std::uint32_t maximum_entry_size = 128U * 1024U * 1024U;
constexpr std::uint64_t maximum_total_size = 1024ULL * 1024ULL * 1024ULL;

[[nodiscard]] std::string normalized(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        const auto slash = character == '\\' ? '/' : character;
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(slash))));
    }
    return result;
}

[[nodiscard]] std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open ZIP archive");
    }
    const auto end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("could not determine ZIP archive size");
    }
    const auto size = static_cast<std::uintmax_t>(end);
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("ZIP64 archives are not supported");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!input) {
        throw std::runtime_error("could not read ZIP archive");
    }
    return bytes;
}

}  // namespace

bool is_safe_archive_path(std::string_view name) noexcept {
    if (name.empty() || name.front() == '/' || name.front() == '\\' ||
        name.find('\0') != std::string_view::npos || name.find(':') != std::string_view::npos) {
        return false;
    }
    std::size_t start = 0;
    while (start <= name.size()) {
        const auto end = name.find_first_of("/\\", start);
        const auto segment = name.substr(start, end == std::string_view::npos ? name.size() - start : end - start);
        if (segment.empty() || segment == "." || segment == "..") {
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

ZipArchive ZipArchive::open(const std::filesystem::path& path) {
    ZipArchive archive;
    archive.bytes_ = read_file(path);
    const ByteReader reader(archive.bytes_);
    if (archive.bytes_.size() < 22) {
        throw std::runtime_error("ZIP archive is too short");
    }
    const auto search_start =
        archive.bytes_.size() > maximum_comment + maximum_trailing_data + 22
            ? archive.bytes_.size() - maximum_comment - maximum_trailing_data - 22
            : 0;
    auto position = archive.bytes_.size() - 22;
    auto end_offset = std::numeric_limits<std::size_t>::max();
    while (true) {
        if (reader.u32(position) == end_signature) {
            const auto comment_size = reader.u16(position + 20);
            const auto record_end = position + 22 + comment_size;
            if (record_end <= archive.bytes_.size() &&
                archive.bytes_.size() - record_end <= maximum_trailing_data) {
                end_offset = position;
                break;
            }
        }
        if (position == search_start) {
            break;
        }
        --position;
    }
    if (end_offset == std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("ZIP end-of-central-directory record was not found");
    }
    if (reader.u16(end_offset + 4) != 0 || reader.u16(end_offset + 6) != 0) {
        throw std::runtime_error("multi-disk ZIP archives are not supported");
    }
    const auto entries_on_disk = reader.u16(end_offset + 8);
    const auto entry_count = reader.u16(end_offset + 10);
    const auto central_size = reader.u32(end_offset + 12);
    const auto central_offset = reader.u32(end_offset + 16);
    if (entries_on_disk != entry_count || entry_count == 0xffffU ||
        central_size == 0xffffffffU || central_offset == 0xffffffffU) {
        throw std::runtime_error("ZIP64 or inconsistent directory is not supported");
    }
    if (static_cast<std::uint64_t>(central_offset) + central_size != end_offset) {
        throw std::runtime_error("ZIP central directory does not end at its footer");
    }

    position = central_offset;
    archive.entries_.reserve(entry_count);
    std::unordered_set<std::string> normalized_names;
    std::uint64_t total_uncompressed_size = 0;
    for (std::uint16_t index = 0; index < entry_count; ++index) {
        if (reader.u32(position) != central_signature) {
            throw std::runtime_error("invalid ZIP central-directory signature");
        }
        const auto flags = reader.u16(position + 8);
        const auto method = reader.u16(position + 10);
        const auto crc = reader.u32(position + 16);
        const auto compressed_size = reader.u32(position + 20);
        const auto uncompressed_size = reader.u32(position + 24);
        const auto name_size = reader.u16(position + 28);
        const auto extra_size = reader.u16(position + 30);
        const auto comment_size = reader.u16(position + 32);
        const auto disk = reader.u16(position + 34);
        const auto local_offset = reader.u32(position + 42);
        const auto record_size = static_cast<std::size_t>(46) + name_size + extra_size + comment_size;
        const auto name_bytes = reader.slice(position + 46, name_size);
        std::string name(reinterpret_cast<const char*>(name_bytes.data()), name_bytes.size());
        if (!is_safe_archive_path(name)) {
            throw std::runtime_error("unsafe path in ZIP archive");
        }
        if (!normalized_names.insert(normalized(name)).second) {
            throw std::runtime_error("duplicate normalized path in ZIP archive");
        }
        if (disk != 0 || (flags & 1U) != 0U) {
            throw std::runtime_error("split or encrypted ZIP entries are not supported");
        }
        if (method != 0 && method != 8) {
            throw std::runtime_error("unsupported ZIP compression method");
        }
        if (uncompressed_size > maximum_entry_size) {
            throw std::runtime_error("ZIP entry exceeds the safety size limit");
        }
        total_uncompressed_size += uncompressed_size;
        if (total_uncompressed_size > maximum_total_size) {
            throw std::runtime_error("ZIP archive exceeds the total safety size limit");
        }
        archive.entries_.push_back({
            .name = std::move(name),
            .flags = flags,
            .compression_method = method,
            .crc32 = crc,
            .compressed_size = compressed_size,
            .uncompressed_size = uncompressed_size,
            .local_header_offset = local_offset,
        });
        position += record_size;
    }
    if (position != static_cast<std::size_t>(central_offset) + central_size) {
        throw std::runtime_error("ZIP central-directory size mismatch");
    }
    return archive;
}

const ZipEntry* ZipArchive::find(std::string_view name) const noexcept {
    const auto wanted = normalized(name);
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const ZipEntry& entry) {
        return normalized(entry.name) == wanted;
    });
    return found == entries_.end() ? nullptr : &*found;
}

std::vector<std::byte> ZipArchive::read(const ZipEntry& entry) const {
    const ByteReader reader(bytes_);
    const auto local = static_cast<std::size_t>(entry.local_header_offset);
    if (reader.u32(local) != local_signature) {
        throw std::runtime_error("invalid ZIP local-header signature");
    }
    const auto local_flags = reader.u16(local + 6);
    const auto local_method = reader.u16(local + 8);
    const auto name_size = reader.u16(local + 26);
    const auto extra_size = reader.u16(local + 28);
    if (local_flags != entry.flags || local_method != entry.compression_method) {
        throw std::runtime_error("ZIP local and central headers disagree");
    }
    const auto local_name_bytes = reader.slice(local + 30, name_size);
    const std::string_view local_name(
        reinterpret_cast<const char*>(local_name_bytes.data()),
        local_name_bytes.size()
    );
    if (normalized(local_name) != normalized(entry.name)) {
        throw std::runtime_error("ZIP local and central filenames disagree");
    }
    const auto data_offset = local + 30 + name_size + extra_size;
    const auto compressed = reader.slice(data_offset, entry.compressed_size);
    std::vector<std::byte> output(entry.uncompressed_size);
    if (entry.compression_method == 0) {
        if (entry.compressed_size != entry.uncompressed_size) {
            throw std::runtime_error("stored ZIP entry has mismatched sizes");
        }
        std::copy(compressed.begin(), compressed.end(), output.begin());
    } else {
        std::array<std::byte, 1> empty_sink{};
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(compressed.data()));
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = reinterpret_cast<Bytef*>(
            output.empty() ? empty_sink.data() : output.data()
        );
        stream.avail_out = static_cast<uInt>(output.empty() ? empty_sink.size() : output.size());
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            throw std::runtime_error("could not initialize ZIP inflater");
        }
        const auto result = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if (result != Z_STREAM_END || stream.total_out != output.size()) {
            throw std::runtime_error("invalid deflated ZIP entry");
        }
    }
    const auto actual_crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef*>(output.data()), static_cast<uInt>(output.size()))
    );
    if (actual_crc != entry.crc32) {
        throw std::runtime_error("ZIP entry CRC mismatch");
    }
    return output;
}

}  // namespace off::data
