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
constexpr std::uint32_t tagged_block_size_mask = 0x00ffffffU;
constexpr std::uint8_t tagged_type_mask = 0x3fU;
constexpr std::uint8_t integer_tag_type = 3U;
constexpr std::uint8_t structural_tag_type = 6U;
constexpr std::uint8_t terminal_tag = 0xffU;
constexpr std::uint32_t window_picture_source_type = 0x00200046U;
constexpr std::array<std::pair<std::uint32_t, std::string_view>, 102>
    geometry_classes{{
        {0x00000000U, "ZGEOM"},
        {0x000000e3U, "ZDebugGeom"},
        {0x00100001U, "ZGROUP"},
        {0x00100020U, "ZTreeGroup"},
        {0x00100021U, "ZROOM"},
        {0x00100028U, "ZBOXGATEGROUP"},
        {0x0010002eU, "ZWINGROUP"},
        {0x00100030U, "ZWINDOWS"},
        {0x00100031U, "ZWINDOW"},
        {0x00100033U, "ZBUTTON"},
        {0x00100034U, "ZSlider"},
        {0x00100037U, "ZPANEL"},
        {0x0010003bU, "ZScrollArea"},
        {0x00100040U, "ZScrollbar"},
        {0x00100042U, "ZEditControl"},
        {0x00100047U, "ZWinGfxHandler"},
        {0x001000caU, "ZAllocMany"},
        {0x001000deU, "ZPATHGROUP"},
        {0x001000e9U, "ZTemplate"},
        {0x001000eaU, "ZInstance"},
        {0x001000f0U, "ZMaterialDescGroup"},
        {0x001007d1U, "ZItem"},
        {0x001007d2U, "ZItemWeapon"},
        {0x001007d3U, "ZItemTemplate"},
        {0x001007d4U, "ZItemTemplateWeapon"},
        {0x001007d5U, "ZItemTemplateAmmo"},
        {0x001007d7U, "ZItemButton"},
        {0x001007d8U, "ZItemAmmo"},
        {0x001007d9U, "ZItemTemplateContainer"},
        {0x001007daU, "ZItemContainer"},
        {0x00100bbdU, "ZMoveOnSpline"},
        {0x00100bbeU, "ZVehicle"},
        {0x00100bbfU, "ZCar"},
        {0x00100bc0U, "ZPmv"},
        {0x00100bc1U, "ZTank"},
        {0x00100bc2U, "ZHeli"},
        {0x00101389U, "ZLanguageVersion"},
        {0x00200002U, "ZSTDOBJ"},
        {0x00200005U, "ZHELPER"},
        {0x00200006U, "ZLNKOBJ"},
        {0x0020000bU, "Z2DOBJ"},
        {0x00200011U, "ZPICOBJ"},
        {0x00200012U, "ZSNDOBJ"},
        {0x00200015U, "ZGATE"},
        {0x00200016U, "ZPlayerTest"},
        {0x0020001cU, "ZBOUND"},
        {0x00200027U, "ZIKLNKOBJ"},
        {0x0020002cU, "ZWINOBJ"},
        {0x0020002dU, "ZCHAROBJ"},
        {0x0020002fU, "ZFONT"},
        {0x00200038U, "ZLINEOBJ"},
        {0x0020003aU, "ZTTFONT"},
        {0x00200045U, "ZFRAME"},
        {0x00200046U, "ZWINPIC"},
        {0x00200070U, "ZBOXSTDOBJ"},
        {0x00200073U, "ZLNKWHANDS"},
        {0x00200074U, "ZCTRLIKLNKOBJ"},
        {0x00200075U, "ZPathfinder3"},
        {0x002000cbU, "ZLightOverlay"},
        {0x002000e1U, "ZBoxPrimitive"},
        {0x002000e2U, "ZParticleBox"},
        {0x002000e4U, "ZParticleController"},
        {0x002000e5U, "ZParticleTemplate"},
        {0x002000e7U, "ZCubeGrid"},
        {0x002000e8U, "ZCubeGridParticles"},
        {0x002000ecU, "ZBreath"},
        {0x002000edU, "ZBloodSpray"},
        {0x002000eeU, "ZBulletMarks"},
        {0x002000efU, "ZBloodStains"},
        {0x002000f0U, "ZMaterialDesc"},
        {0x002000f1U, "ZBloodSplatters"},
        {0x002000f4U, "ZMotionBlur"},
        {0x002000f6U, "ZCloth"},
        {0x002000faU, "ZCameraOverlay"},
        {0x002000fdU, "ZStdObjBox"},
        {0x002003e8U, "ZFFHERO"},
        {0x0020044cU, "ZTVNoise"},
        {0x0020044dU, "ZCredits"},
        {0x0020047eU, "ZOnScreenDisplay"},
        {0x00200481U, "ZMapHelper"},
        {0x00200482U, "ZFFPRIVATESTDOBJ"},
        {0x002007d0U, "ZActor"},
        {0x002007f9U, "ZFFActor"},
        {0x00400003U, "ZCAMERA"},
        {0x00400480U, "ZFFCameraClass"},
        {0x0080000dU, "ZENVIRONMENT"},
        {0x00800020U, "ZSPOTLIGHTSQUARE"},
        {0x00800023U, "ZSPOTLIGHT"},
        {0x00800024U, "ZOMNILIGHT"},
        {0x008000e6U, "ZUSERLIGHT"},
        {0x008000f2U, "ZGateLightSquareSpot"},
        {0x008000f3U, "ZGateLightMaster"},
        {0x008000f5U, "ZGateLightOmni"},
        {0x008000f7U, "ZGateLightSquareSpotMaster"},
        {0x008000f8U, "ZGateLightSpotMaster"},
        {0x008000f9U, "ZGateLightSpot"},
        {0x04000022U, "ZSHAPE"},
        {0x0800001aU, "ZLIST"},
        {0x08000049U, "ZWINOBJSPRITEHOLDER"},
        {0x80100032U, "ZCONTROL"},
        {0x80200014U, "ZPlayer"},
        {0x80800004U, "ZLIGHT"},
    }};
constexpr std::array<std::uint32_t, 16> primitive_source_types{
    0x00200002U,
    0x0020000bU,
    0x00200015U,
    0x0020001cU,
    0x0020002dU,
    0x0020003aU,
    0x00200046U,
    0x00200075U,
    0x002000cbU,
    0x002000e1U,
    0x002000e2U,
    0x002000e5U,
    0x002000eeU,
    0x002000f6U,
    0x002000fdU,
    0x00200482U,
};
static_assert(std::ranges::is_sorted(
    geometry_classes,
    {},
    &std::pair<std::uint32_t, std::string_view>::first
));
static_assert(std::ranges::is_sorted(primitive_source_types));

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

[[nodiscard]] GmsWindowPictureSource parse_startup_window_picture_source(
    const ByteReader& reader,
    std::span<const std::byte> payload,
    std::size_t block_offset
) {
    if (block_offset > payload.size() ||
        sizeof(std::uint32_t) > payload.size() - block_offset) {
        throw std::runtime_error("GMS window-picture source is truncated");
    }
    const auto block_size = static_cast<std::size_t>(
        reader.u32(block_offset) & tagged_block_size_mask
    );
    if (block_size < sizeof(std::uint32_t) ||
        block_size > payload.size() - block_offset) {
        throw std::runtime_error("GMS window-picture source has an invalid size");
    }
    const auto end = block_offset + block_size;
    auto cursor = block_offset + sizeof(std::uint32_t);

    const auto current_type = [&]() -> std::uint8_t {
        if (cursor >= end) {
            throw std::runtime_error("GMS window-picture tag stream is truncated");
        }
        return std::to_integer<std::uint8_t>(payload[cursor]) & tagged_type_mask;
    };
    const auto read_integer = [&]() -> std::uint32_t {
        if (current_type() != integer_tag_type ||
            sizeof(std::uint32_t) > end - cursor - 1U) {
            throw std::runtime_error("GMS window-picture integer tag is invalid");
        }
        const auto value = reader.u32(cursor + 1U);
        cursor += 1U + sizeof(std::uint32_t);
        return value;
    };
    const auto read_structure = [&]() {
        if (current_type() != structural_tag_type) {
            throw std::runtime_error("GMS window-picture structure tag is invalid");
        }
        ++cursor;
    };

    const auto authored_state_exponent = read_integer();
    if (authored_state_exponent >= std::numeric_limits<std::uint8_t>::digits) {
        throw std::runtime_error("GMS window-picture state exponent is out of range");
    }
    const auto base_render_property = read_integer();
    const auto authored_alpha = read_integer();
    const auto alignment_enum = read_integer();
    if (alignment_enum > 15U) {
        throw std::runtime_error("GMS window-picture alignment enum is out of range");
    }
    std::optional<std::uint8_t> extension_control;
    if (current_type() == integer_tag_type) {
        extension_control = static_cast<std::uint8_t>(
            std::min(read_integer(), std::uint32_t{16}));
    }
    read_structure();
    const auto picture_asset_reference = read_integer();
    read_structure();
    if (cursor >= end ||
        std::to_integer<std::uint8_t>(payload[cursor]) != terminal_tag) {
        throw std::runtime_error("GMS window-picture terminal tag is invalid");
    }
    ++cursor;
    if (cursor != end) {
        throw std::runtime_error("GMS window-picture source has trailing data");
    }
    return {
        .authored_state_exponent =
            static_cast<std::uint8_t>(authored_state_exponent),
        .base_render_property = base_render_property,
        .authored_alpha = static_cast<std::uint8_t>(
            std::min(authored_alpha, std::uint32_t{255})),
        .alignment_enum = static_cast<std::uint8_t>(alignment_enum),
        .extension_control = extension_control,
        .picture_asset_reference = picture_asset_reference,
    };
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
    result.hierarchy_.reserve(directory_count);
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
    std::vector<std::optional<std::size_t>> active_parents{std::nullopt};
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
        active_parents.resize(active_parents.size() - parent_steps);
        const auto pool_group = active_groups.back();
        const auto parent_directory_index = active_parents.back();
        const auto source_type = reader.u32(record_offset + 16U);
        const auto buf_name_offset = reader.u32(record_offset);
        const auto basis_offset = reader.u32(record_offset + 4U);
        const auto position_offset = reader.u32(record_offset + 8U);
        const auto class_data_value = reader.u32(record_offset + 12U);
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
            .buf_name_offset = buf_name_offset,
            .basis_offset = basis_offset,
            .position_offset = position_offset,
            .class_data_value = class_data_value,
            .attachment_table_offset = attachment_table_offset,
            .object_flags = object_flags,
            .buf_auxiliary_offset = buf_auxiliary_offset,
            .deferred_source_offset = deferred_source_offset,
            .child_value = child_value,
            .post_load_source_offset = post_load_source_offset,
            .basis = basis,
            .position = position,
            .attachments = std::move(attachments),
            .primitive_reference =
                class_data_value != 0U &&
                        std::ranges::binary_search(primitive_source_types, source_type)
                    ? std::optional{class_data_value}
                    : std::nullopt,
            .parent_steps = parent_steps,
            .source_variant = source_variant,
            .pool_class = static_cast<std::uint8_t>(pool_class),
            .enters_child_pool = enters_child_pool,
        });
        result.hierarchy_.push_back({
            .directory_index = index,
            .parent_directory_index = parent_directory_index,
            .children_in_directory_order = {},
        });
        if (parent_directory_index.has_value()) {
            if (*parent_directory_index >= index) {
                throw std::runtime_error(
                    "GMS object-source hierarchy has a non-preceding parent");
            }
            result.hierarchy_[*parent_directory_index]
                .children_in_directory_order.push_back(index);
        }
        if ((source_type & 0x00100000U) != 0U) {
            ++next_group_ordinal;
        }
        if (enters_child_pool) {
            if (next_group_ordinal == 0 ||
                next_group_ordinal > result.pool_groups_.size()) {
                throw std::runtime_error("GMS object-source hierarchy exceeds its pool groups");
            }
            active_groups.push_back(next_group_ordinal - 1U);
            active_parents.push_back(index);
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
        if (entry.buf_name_offset >= bytes.size()) {
            throw std::runtime_error("GMS object name exceeds its BUF resource");
        }
        const auto name_start = bytes.begin() +
                                static_cast<std::ptrdiff_t>(entry.buf_name_offset);
        if (std::find(name_start, bytes.end(), std::byte{0}) == bytes.end()) {
            throw std::runtime_error("GMS object name is not NUL-terminated");
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

std::optional<std::string_view> GmsImage::source_class_name(
    std::uint32_t source_type
) noexcept {
    const auto found = std::ranges::lower_bound(
        geometry_classes,
        source_type,
        {},
        &std::pair<std::uint32_t, std::string_view>::first
    );
    if (found == geometry_classes.end() || found->first != source_type) {
        return std::nullopt;
    }
    return found->second;
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

GmsIntroMovieControllerSource GmsImage::intro_movie_controller_source(
    std::size_t directory_index
) const {
    const auto fail = []() {
        throw std::runtime_error("GMS intro movie-controller source is unsupported or malformed");
    };
    if (directory_index >= directory_.size()) {
        fail();
    }
    const auto& entry = directory_[directory_index];
    if (entry.source_type != 0x0800001aU || entry.class_data_value != 0U ||
        entry.attachments.size() != 1U || entry.deferred_source_offset == 0U) {
        fail();
    }
    const auto payload = resource_.payload();
    const auto& attachment = entry.attachments.front();
    constexpr std::string_view identity = "ZGEOM_MovieControl";
    const auto identity_offset = static_cast<std::size_t>(attachment.source_offset);
    if (!std::isfinite(attachment.parameter) || attachment.parameter != 0.0F ||
        identity_offset > payload.size() ||
        identity.size() + 1U > payload.size() - identity_offset) {
        fail();
    }
    for (std::size_t i = 0; i < identity.size(); ++i) {
        if (payload[identity_offset + i] != static_cast<std::byte>(identity[i])) {
            fail();
        }
    }
    if (payload[identity_offset + identity.size()] != std::byte{0}) {
        fail();
    }
    const ByteReader reader(payload);
    const auto offset = static_cast<std::size_t>(entry.deferred_source_offset);
    if (offset > payload.size() || 4U > payload.size() - offset) {
        fail();
    }
    const auto header = reader.u32(offset);
    const auto size = static_cast<std::size_t>(header & tagged_block_size_mask);
    if ((header >> 24U) != 0U || size < 4U || size > payload.size() - offset) {
        fail();
    }
    const auto end = offset + size;
    auto cursor = offset + 4U;
    const auto tag = [&](std::uint8_t expected) {
        if (cursor == end || payload[cursor] != static_cast<std::byte>(expected)) {
            fail();
        }
        ++cursor;
    };
    const auto scalar = [&](std::uint8_t expected) {
        tag(expected);
        if (4U > end - cursor) {
            fail();
        }
        const auto value = reader.u32(cursor);
        cursor += 4U;
        return value;
    };
    if (scalar(0x09U) != 4U) {
        fail();
    }
    tag(0x06U);
    GmsIntroMovieControllerSource result;
    result.sequence_reference = scalar(0x88U);
    result.group_reference = scalar(0x88U);
    result.additional_reference = scalar(0x08U);
    tag(0x84U);
    while (cursor < end && payload[cursor] != std::byte{0}) {
        if (result.destination.size() == 99U) {
            fail();
        }
        result.destination.push_back(static_cast<char>(
            std::to_integer<unsigned char>(payload[cursor++])));
    }
    tag(0U);
    result.authored_option = scalar(0x83U);
    // Only the full two-token form is observed in the supported corpus. Prefix
    // absence is inferred from the reader's non-advancing closing-marker policy.
    if (cursor < end && payload[cursor] == std::byte{0x88}) {
        result.first_optional_reference = scalar(0x88U);
        if (cursor < end && payload[cursor] == std::byte{0x08}) {
            result.second_optional_reference = scalar(0x08U);
        }
    }
    tag(0x06U);
    tag(0xffU);
    if (cursor != end) {
        fail();
    }
    return result;
}

GmsWindowPictureSource GmsImage::startup_window_picture_source(
    std::size_t directory_index
) const {
    if (directory_index >= directory_.size()) {
        throw std::runtime_error("GMS window-picture directory index is out of range");
    }
    const auto& entry = directory_[directory_index];
    if (entry.source_type != window_picture_source_type) {
        throw std::runtime_error("GMS source is not a window-picture source");
    }
    if (entry.deferred_source_offset == 0U) {
        throw std::runtime_error(
            "GMS window-picture source has no deferred serialization"
        );
    }
    const auto payload = resource_.payload();
    return parse_startup_window_picture_source(
        ByteReader(payload), payload, entry.deferred_source_offset
    );
}

}  // namespace off::data
