#include "off/data/gms_image.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace off::data {
namespace {

constexpr std::uint32_t reference_flag_mask = 0xc0000000U;
constexpr std::uint32_t gms_reference_flag = 0x40000000U;
constexpr std::uint32_t reference_offset_mask = 0x3fffffffU;
constexpr std::uint32_t record_word_offset_mask = 0x00ffffffU;
constexpr std::uint32_t record_flag = 0x01000000U;
constexpr std::size_t fixed_header_size = 32;
constexpr std::size_t directory_entry_size = 8;
constexpr std::size_t minimum_record_size = 48;
constexpr std::size_t object_slot_size = 112;
constexpr std::size_t pool_class_count = 24;
constexpr std::size_t pool_group_size = pool_class_count * sizeof(std::uint32_t);
constexpr std::uint32_t auxiliary_size_mask = 0x3fffffffU;

[[nodiscard]] std::size_t checked_table_end(
    std::size_t offset,
    std::size_t count,
    std::size_t entry_size,
    std::size_t payload_size,
    const char* message
) {
    if (offset > payload_size || count > (payload_size - offset) / entry_size) {
        throw std::runtime_error(message);
    }
    return offset + count * entry_size;
}

[[nodiscard]] std::uint8_t base_pool_class(std::uint32_t source_type) {
    if (source_type == 0x04000022U) {
        return 1;
    }
    if ((source_type & 0x00200000U) != 0U) {
        if (source_type == 0x00200012U || source_type == 0x00200015U ||
            source_type == 0x0020001cU) {
            return 3;
        }
        return 1;
    }
    if ((source_type & 0x00100000U) != 0U) {
        return 0;
    }
    return (source_type & 0x00800000U) != 0U ? 2 : 3;
}

[[nodiscard]] float read_finite_float(const ByteReader& reader, std::size_t offset) {
    const auto value = std::bit_cast<float>(reader.u32(offset));
    if (!std::isfinite(value)) {
        throw std::runtime_error("GMS object source contains a non-finite component");
    }
    return value;
}

void require_optional_offset(
    std::uint32_t offset,
    std::size_t payload_size,
    const char* message
) {
    if (offset != 0U && offset >= payload_size) {
        throw std::runtime_error(message);
    }
}

}  // namespace

GmsImage GmsImage::parse(PackedResource resource) {
    GmsImage result;
    result.resource_ = std::move(resource);
    const auto payload = result.resource_.payload();
    if (payload.size() < fixed_header_size) {
        throw std::runtime_error("GMS image is missing its fixed header");
    }
    const ByteReader reader(payload);
    const auto directory_offset = static_cast<std::size_t>(reader.u32(0));
    const auto identifier_table_offset = static_cast<std::size_t>(reader.u32(4));
    const auto pool_table_offset = static_cast<std::size_t>(reader.u32(20));
    if ((directory_offset & 3U) != 0U || (identifier_table_offset & 3U) != 0U ||
        (pool_table_offset & 3U) != 0U ||
        directory_offset > payload.size() - sizeof(std::uint32_t) ||
        identifier_table_offset > payload.size() - sizeof(std::uint32_t) ||
        pool_table_offset > payload.size() - sizeof(std::uint32_t) ||
        reader.u32(12) != 4U) {
        throw std::runtime_error("invalid GMS image header");
    }

    const auto pool_group_count = static_cast<std::size_t>(reader.u32(pool_table_offset));
    const auto pool_table_start = pool_table_offset + sizeof(std::uint32_t);
    static_cast<void>(checked_table_end(
        pool_table_start,
        pool_group_count,
        pool_group_size,
        payload.size(),
        "GMS pool-count table exceeds the decoded image"
    ));
    result.pool_groups_.reserve(pool_group_count);
    std::size_t declared_slot_count = 0;
    for (std::size_t group_index = 0; group_index < pool_group_count; ++group_index) {
        GmsPoolGroup group;
        std::size_t group_slot_count = 0;
        for (std::size_t class_index = 0; class_index < pool_class_count; ++class_index) {
            const auto count = reader.u32(
                pool_table_start + group_index * pool_group_size +
                class_index * sizeof(std::uint32_t)
            );
            group.class_counts[class_index] = count;
            group_slot_count += count;
        }
        if (group_slot_count > std::numeric_limits<std::uint32_t>::max() ||
            declared_slot_count > std::numeric_limits<std::size_t>::max() -
                group_slot_count) {
            throw std::runtime_error("GMS pool counts overflow their portable model");
        }
        group.slot_count = static_cast<std::uint32_t>(group_slot_count);
        declared_slot_count += group_slot_count;
        result.pool_groups_.push_back(group);
    }

    const auto directory_count = static_cast<std::size_t>(reader.u32(directory_offset));
    const auto directory_start = directory_offset + sizeof(std::uint32_t);
    static_cast<void>(checked_table_end(
        directory_start,
        directory_count,
        directory_entry_size,
        payload.size(),
        "GMS object-source directory exceeds the decoded image"
    ));
    if (result.pool_groups_.empty() || declared_slot_count != directory_count) {
        throw std::runtime_error("GMS pool counts do not cover the object-source directory");
    }
    result.directory_.reserve(directory_count);
    result.local_slot_to_directory_.assign(
        directory_count,
        std::numeric_limits<std::size_t>::max()
    );
    std::vector<std::uint32_t> group_base_slots;
    group_base_slots.reserve(pool_group_count);
    std::uint32_t group_base_slot = 0;
    for (const auto& group : result.pool_groups_) {
        group_base_slots.push_back(group_base_slot);
        group_base_slot += group.slot_count;
    }
    std::vector<std::array<std::uint32_t, pool_class_count>> observed_counts(
        pool_group_count
    );
    std::vector<std::size_t> active_groups{0};
    std::size_t next_group_ordinal = 1;
    for (std::size_t index = 0; index < directory_count; ++index) {
        const auto entry_offset = directory_start + index * directory_entry_size;
        const auto packed_record = reader.u32(entry_offset);
        const auto record_word_offset = packed_record & record_word_offset_mask;
        const auto record_offset = static_cast<std::size_t>(record_word_offset) * 4U;
        if (record_offset > payload.size() ||
            minimum_record_size > payload.size() - record_offset) {
            throw std::runtime_error("GMS directory references a truncated object source");
        }
        const auto parent_steps = static_cast<std::uint8_t>(packed_record >> 25U);
        if (parent_steps >= active_groups.size()) {
            throw std::runtime_error("GMS object-source hierarchy underflows its root");
        }
        active_groups.resize(active_groups.size() - parent_steps);
        const auto pool_group = active_groups.back();
        const auto source_type = reader.u32(record_offset + 16U);
        const auto buf_object_offset = reader.u32(record_offset);
        const auto basis_offset = reader.u32(record_offset + 4U);
        const auto position_offset = reader.u32(record_offset + 8U);
        const auto linked_object_value = reader.u32(record_offset + 12U);
        const auto attachment_table_offset = reader.u32(record_offset + 20U);
        const auto object_flags = reader.u32(record_offset + 24U);
        const auto buf_auxiliary_offset = reader.u32(record_offset + 28U);
        const auto deferred_source_offset = reader.u32(record_offset + 32U);
        const auto child_value = reader.u32(record_offset + 36U);
        const auto post_load_source_offset = reader.u32(record_offset + 40U);
        if (basis_offset > payload.size() || 36U > payload.size() - basis_offset ||
            position_offset > payload.size() ||
            12U > payload.size() - position_offset) {
            throw std::runtime_error("GMS object transform exceeds the decoded image");
        }
        require_optional_offset(
            deferred_source_offset,
            payload.size(),
            "GMS deferred object source exceeds the decoded image"
        );
        require_optional_offset(
            post_load_source_offset,
            payload.size(),
            "GMS post-load object source exceeds the decoded image"
        );
        std::array<float, 9> basis{};
        for (std::size_t component = 0; component < basis.size(); ++component) {
            basis[component] = read_finite_float(
                reader,
                static_cast<std::size_t>(basis_offset) + component * 4U
            );
        }
        std::array<float, 3> position{};
        for (std::size_t component = 0; component < position.size(); ++component) {
            position[component] = read_finite_float(
                reader,
                static_cast<std::size_t>(position_offset) + component * 4U
            );
        }
        std::vector<GmsAttachment> attachments;
        if (attachment_table_offset != 0U) {
            if (attachment_table_offset > payload.size() - sizeof(std::uint32_t)) {
                throw std::runtime_error("GMS attachment table exceeds the decoded image");
            }
            const auto attachment_count = static_cast<std::size_t>(
                reader.u32(attachment_table_offset)
            );
            const auto attachment_start =
                static_cast<std::size_t>(attachment_table_offset) + sizeof(std::uint32_t);
            static_cast<void>(checked_table_end(
                attachment_start,
                attachment_count,
                8U,
                payload.size(),
                "GMS attachment table exceeds the decoded image"
            ));
            attachments.reserve(attachment_count);
            for (std::size_t attachment_index = 0;
                 attachment_index < attachment_count;
                 ++attachment_index) {
                const auto attachment_offset = attachment_start + attachment_index * 8U;
                const auto target_offset = reader.u32(attachment_offset);
                if (target_offset >= payload.size()) {
                    throw std::runtime_error(
                        "GMS attachment references outside the decoded image"
                    );
                }
                attachments.push_back({
                    .source_offset = target_offset,
                    .parameter = read_finite_float(reader, attachment_offset + 4U),
                });
            }
        }
        const auto source_variant = std::to_integer<std::uint8_t>(
            payload[record_offset + 45U]
        );
        const auto pool_class = static_cast<std::size_t>(
            base_pool_class(source_type) + source_variant * 8U
        );
        if (pool_group >= result.pool_groups_.size() ||
            pool_class >= pool_class_count) {
            throw std::runtime_error("GMS object source selects an invalid pool class");
        }
        const auto class_ordinal = observed_counts[pool_group][pool_class]++;
        if (class_ordinal >= result.pool_groups_[pool_group].class_counts[pool_class]) {
            throw std::runtime_error("GMS object source exceeds its declared pool class");
        }
        std::uint32_t group_slot_index = class_ordinal;
        for (std::size_t class_index = 0; class_index < pool_class; ++class_index) {
            group_slot_index += result.pool_groups_[pool_group].class_counts[class_index];
        }
        const auto local_slot_index = group_base_slots[pool_group] + group_slot_index;
        if (local_slot_index >= result.local_slot_to_directory_.size() ||
            result.local_slot_to_directory_[local_slot_index] !=
                std::numeric_limits<std::size_t>::max()) {
            throw std::runtime_error("GMS pool assignment reuses a local slot");
        }
        result.local_slot_to_directory_[local_slot_index] = index;
        const auto enters_child_pool = (packed_record & record_flag) != 0U;
        result.directory_.push_back({
            .packed_record_reference = packed_record,
            .auxiliary_value = reader.u32(entry_offset + sizeof(std::uint32_t)),
            .record_offset = static_cast<std::uint32_t>(record_offset),
            .source_type = source_type,
            .class_ordinal = class_ordinal,
            .group_slot_index = group_slot_index,
            .local_slot_index = local_slot_index,
            .pool_group = static_cast<std::uint32_t>(pool_group),
            .buf_object_offset = buf_object_offset,
            .basis_offset = basis_offset,
            .position_offset = position_offset,
            .linked_object_value = linked_object_value,
            .attachment_table_offset = attachment_table_offset,
            .object_flags = object_flags,
            .buf_auxiliary_offset = buf_auxiliary_offset,
            .deferred_source_offset = deferred_source_offset,
            .child_value = child_value,
            .post_load_source_offset = post_load_source_offset,
            .basis = basis,
            .position = position,
            .attachments = std::move(attachments),
            .parent_steps = parent_steps,
            .source_variant = source_variant,
            .pool_class = static_cast<std::uint8_t>(pool_class),
            .enters_child_pool = enters_child_pool,
        });
        if ((source_type & 0x00100000U) != 0U) {
            ++next_group_ordinal;
        }
        if (enters_child_pool) {
            if (next_group_ordinal == 0 ||
                next_group_ordinal > result.pool_groups_.size()) {
                throw std::runtime_error("GMS object-source hierarchy exceeds its pool groups");
            }
            active_groups.push_back(next_group_ordinal - 1U);
        }
    }
    if (next_group_ordinal != result.pool_groups_.size()) {
        throw std::runtime_error("GMS object-source hierarchy does not use every pool group");
    }
    for (std::size_t group_index = 0; group_index < pool_group_count; ++group_index) {
        if (observed_counts[group_index] != result.pool_groups_[group_index].class_counts) {
            throw std::runtime_error("GMS object-source classes do not match pool counts");
        }
    }
    if (std::ranges::find(
            result.local_slot_to_directory_,
            std::numeric_limits<std::size_t>::max()
        ) != result.local_slot_to_directory_.end()) {
        throw std::runtime_error("GMS pool assignment leaves an unpopulated local slot");
    }

    result.identifier_count_ = reader.u32(identifier_table_offset);
    const auto identifier_table_start = identifier_table_offset + sizeof(std::uint32_t);
    static_cast<void>(checked_table_end(
        identifier_table_start,
        result.identifier_count_,
        sizeof(std::uint32_t),
        payload.size(),
        "GMS identifier table exceeds the decoded image"
    ));
    for (std::size_t index = 0; index < result.identifier_count_; ++index) {
        const auto string_offset = static_cast<std::size_t>(
            reader.u32(identifier_table_start + index * sizeof(std::uint32_t))
        );
        if (string_offset >= payload.size() ||
            std::find(payload.begin() + static_cast<std::ptrdiff_t>(string_offset),
                      payload.end(), std::byte{0}) == payload.end()) {
            throw std::runtime_error("GMS identifier is not NUL-terminated");
        }
    }
    return result;
}

void GmsImage::validate_buf(std::span<const std::byte> bytes) const {
    if (bytes.empty() && !directory_.empty()) {
        throw std::runtime_error("GMS object sources require a non-empty BUF resource");
    }
    const ByteReader reader(bytes);
    for (const auto& entry : directory_) {
        if (entry.buf_object_offset >= bytes.size()) {
            throw std::runtime_error("GMS object source exceeds its BUF resource");
        }
        if (entry.buf_auxiliary_offset == 0U) {
            continue;
        }
        const auto offset = static_cast<std::size_t>(entry.buf_auxiliary_offset);
        if (offset > bytes.size() || 8U > bytes.size() - offset) {
            throw std::runtime_error("GMS auxiliary BUF header exceeds its resource");
        }
        const auto size = static_cast<std::size_t>(reader.u32(offset + 4U) &
                                                   auxiliary_size_mask);
        if (size < 8U || size > bytes.size() - offset) {
            throw std::runtime_error("GMS auxiliary BUF block exceeds its resource");
        }
    }
}

GmsObjectHandle GmsImage::decode_object_handle(std::uint32_t packed_reference) {
    if ((packed_reference & reference_flag_mask) != gms_reference_flag) {
        throw std::runtime_error("invalid GMS object-handle tag");
    }
    const auto offset = packed_reference & reference_offset_mask;
    if (offset % object_slot_size != 0U) {
        throw std::runtime_error("misaligned GMS object handle");
    }
    return {
        .byte_offset = offset,
        .slot_index = static_cast<std::uint32_t>(offset / object_slot_size),
    };
}

std::optional<std::size_t> GmsImage::local_source_for_handle(
    std::uint32_t packed_reference
) const {
    const auto handle = decode_object_handle(packed_reference);
    if (handle.slot_index >= local_slot_to_directory_.size()) {
        return std::nullopt;
    }
    return local_slot_to_directory_[handle.slot_index];
}

}  // namespace off::data
