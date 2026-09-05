#pragma once

#include "off/data/packed_resource.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace off::data {

struct GmsAttachment {
    std::uint32_t source_offset{0};
    float parameter{0.0F};
};

struct GmsDirectoryEntry {
    std::uint32_t packed_record_reference{0};
    std::uint32_t auxiliary_value{0};
    std::uint32_t record_offset{0};
    std::uint32_t source_type{0};
    std::uint32_t class_ordinal{0};
    std::uint32_t group_slot_index{0};
    std::uint32_t local_slot_index{0};
    std::uint32_t pool_group{0};
    std::uint32_t buf_name_offset{0};
    std::uint32_t basis_offset{0};
    std::uint32_t position_offset{0};
    std::uint32_t class_data_value{0};
    std::uint32_t attachment_table_offset{0};
    std::uint32_t object_flags{0};
    std::uint32_t buf_auxiliary_offset{0};
    std::uint32_t deferred_source_offset{0};
    std::uint32_t child_value{0};
    std::uint32_t post_load_source_offset{0};
    std::array<float, 9> basis{};
    std::array<float, 3> position{};
    std::vector<GmsAttachment> attachments;
    std::optional<std::uint32_t> primitive_reference;
    std::uint8_t parent_steps{0};
    std::uint8_t source_variant{0};
    std::uint8_t pool_class{0};
    bool enters_child_pool{false};
};

struct GmsPoolGroup {
    std::array<std::uint32_t, 24> class_counts{};
    std::uint32_t slot_count{0};
};

struct GmsObjectHandle {
    std::uint32_t byte_offset{0};
    std::uint32_t slot_index{0};
};

class GmsImage final {
public:
    [[nodiscard]] static GmsImage parse(PackedResource resource);
    [[nodiscard]] static GmsObjectHandle decode_object_handle(
        std::uint32_t packed_reference
    );
    [[nodiscard]] static std::optional<std::string_view> source_class_name(
        std::uint32_t source_type
    ) noexcept;
    [[nodiscard]] std::optional<std::size_t> local_source_for_handle(
        std::uint32_t packed_reference
    ) const;
    void validate_buf(std::span<const std::byte> bytes) const;

    [[nodiscard]] const std::vector<GmsDirectoryEntry>& directory() const noexcept {
        return directory_;
    }
    [[nodiscard]] const std::vector<GmsPoolGroup>& pool_groups() const noexcept {
        return pool_groups_;
    }
    [[nodiscard]] std::size_t identifier_count() const noexcept {
        return identifier_count_;
    }
    [[nodiscard]] std::size_t decoded_size() const noexcept {
        return resource_.payload().size();
    }

private:
    PackedResource resource_;
    std::vector<GmsDirectoryEntry> directory_;
    std::vector<GmsPoolGroup> pool_groups_;
    std::vector<std::size_t> local_slot_to_directory_;
    std::size_t identifier_count_{0};
};

}  // namespace off::data
