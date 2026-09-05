#pragma once

#include "off/data/packed_resource.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace off::data {

struct GmsDirectoryEntry {
    std::uint32_t packed_record_reference{0};
    std::uint32_t auxiliary_value{0};
    std::uint32_t record_offset{0};
    std::uint32_t source_type{0};
    std::uint32_t class_ordinal{0};
    std::uint32_t group_slot_index{0};
    std::uint32_t local_slot_index{0};
    std::uint32_t pool_group{0};
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
    [[nodiscard]] std::optional<std::size_t> local_source_for_handle(
        std::uint32_t packed_reference
    ) const;

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
