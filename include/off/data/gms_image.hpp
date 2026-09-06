#pragma once

#include "off/data/packed_resource.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace off::data {

struct GmsAttachment {
    std::uint32_t source_offset{0};
    float parameter{0.0F};
};

struct GmsWindowPictureSource {
    std::uint8_t authored_state_exponent{0};
    std::uint32_t base_render_property{0};
    std::uint8_t authored_alpha{255};
    std::uint8_t alignment_enum{0};
    std::optional<std::uint8_t> extension_control;
    std::uint32_t picture_asset_reference{0};
};

struct GmsIntroMovieControllerSource {
    std::uint32_t sequence_reference{0};
    std::uint32_t group_reference{0};
    std::uint32_t additional_reference{0};
    // Owned raw bytes, not an asserted UTF-8 string or a resolved scene path.
    std::string destination;
    std::uint32_t authored_option{0};
    std::optional<std::uint32_t> first_optional_reference;
    std::optional<std::uint32_t> second_optional_reference;
};

struct GmsIntroCutCommandSource {
    std::uint32_t timeline_position{0};
    std::uint32_t event_reference{0};
    std::uint32_t target_reference{0};
    std::uint32_t event_argument{0};
    std::string target_name;
};

struct GmsIntroFirstCutSource {
    std::uint32_t sequence_reference{0};
    std::array<std::uint32_t, 7> settings_words{};
    float final_value{0};
    std::array<GmsIntroCutCommandSource, 5> commands{};
};

struct GmsIntroCutSequenceSource {
    std::array<std::uint32_t, 6> references{};
    std::array<float, 2> values{};
    std::uint32_t authored_option{0};
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

struct GmsHierarchyNode {
    std::size_t directory_index{0};
    std::optional<std::size_t> parent_directory_index;
    std::vector<std::size_t> children_in_directory_order;
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
    // Callers must establish that this image came from the supported startup
    // archive; other scene-specific picture stream variants are not yet covered.
    [[nodiscard]] GmsWindowPictureSource startup_window_picture_source(
        std::size_t directory_index
    ) const;
    // Caller must establish supported intro provenance. This deliberately narrow
    // attachment grammar is not generic component dispatch or reference lookup.
    [[nodiscard]] GmsIntroMovieControllerSource intro_movie_controller_source(
        std::size_t directory_index
    ) const;
    // Supported deferred type-8/type-9 relocation with mode false only. Resolves
    // source identity, not a runtime handle or proof of a successful factory.
    // Rejects out-of-range references instead of the original diagnostic + zero.
    [[nodiscard]] std::optional<std::size_t> local_source_for_authored_reference(
        std::uint32_t raw_reference
    ) const;
    // Caller must establish supported intro provenance; returns raw words without
    // relocation, preserving ordering, duplicates, and unresolved values.
    [[nodiscard]] std::vector<std::uint32_t> intro_source_reference_list(
        std::size_t directory_index
    ) const;
    void validate_buf(std::span<const std::byte> bytes) const;
    // Restricted supported-intro forms; caller establishes archive provenance.
    // Values remain authored data, not runtime events, clock units or defaults.
    [[nodiscard]] GmsIntroFirstCutSource intro_first_cut_source(std::size_t index) const;
    [[nodiscard]] GmsIntroCutSequenceSource intro_cut_sequence_source(std::size_t index) const;
    // Zero has no join. Nonzero references are one-based, without bit masking.
    [[nodiscard]] std::optional<std::string> authored_event_identifier(std::uint32_t raw) const;

    [[nodiscard]] const std::vector<GmsDirectoryEntry>& directory() const noexcept {
        return directory_;
    }
    [[nodiscard]] const std::vector<GmsPoolGroup>& pool_groups() const noexcept {
        return pool_groups_;
    }
    [[nodiscard]] const std::vector<GmsHierarchyNode>& hierarchy() const noexcept {
        return hierarchy_;
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
    std::vector<GmsHierarchyNode> hierarchy_;
    std::vector<std::size_t> local_slot_to_directory_;
    std::size_t identifier_count_{0};
};

}  // namespace off::data
