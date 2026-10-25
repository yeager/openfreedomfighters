#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/picture_texture_binding.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace off::data {

inline constexpr std::size_t maximum_startup_graphics_rows = 4096;
inline constexpr std::size_t maximum_startup_graphics_pictures = 4096;
inline constexpr std::size_t maximum_startup_graphics_draws = 16384;

struct StartupGraphicsLocalTransform {
    std::size_t directory_index{0};
    std::array<float, 9> basis{};
    std::array<float, 3> position{};
};

enum class StartupGraphicsCompositionRole : std::uint8_t {
    row_background,
    row_chrome,
};

struct StartupGraphicsCompositionInstance {
    StartupGraphicsCompositionRole role{StartupGraphicsCompositionRole::row_chrome};
    std::size_t directory_index{0};
    // Inclusive root-to-instance construction chain. The transform entry IDs
    // are required to match this vector element-for-element.
    std::vector<std::size_t> construction_chain;
    std::vector<StartupGraphicsLocalTransform> transform_chain;
    std::uint32_t picture_reference{0};
    std::uint8_t authored_state_mask{0};
    std::uint32_t base_render_property{0};
    std::uint8_t authored_alpha{255};
    std::uint8_t alignment_enum{0};
    std::optional<std::uint8_t> extension_control;
    PictureDrawPlan draw_plan;
};

struct StartupGraphicsRowComposition {
    std::size_t owner_directory_index{0};
    float slot_y{0.0F};
    std::size_t same_slot_ordinal{0};
    std::size_t same_slot_multiplicity{1};
    bool authored_hidden{false};
    std::vector<std::size_t> construction_chain;
    std::vector<StartupGraphicsLocalTransform> transform_chain;
    std::vector<StartupGraphicsCompositionInstance> pictures;
};

struct StartupGraphicsVisiblePicture {
    std::size_t row_index{0};
    std::size_t picture_index{0};
    std::size_t row_directory_index{0};
    std::size_t picture_directory_index{0};
    StartupGraphicsCompositionRole role{StartupGraphicsCompositionRole::row_chrome};
    std::size_t draw_emission_count{0};
};

struct StartupGraphicsVisibility final {
    std::uint8_t requested_state{0};
    std::uint8_t effective_state{0};
    std::vector<StartupGraphicsVisiblePicture> pictures;
};

struct StartupGraphicsDrawEmission {
    std::size_t row_index{0};
    std::size_t picture_index{0};
    std::size_t picture_directory_index{0};
    std::size_t group_index{0};
    std::size_t descriptor_index{0};
};

struct StartupGraphicsTraversalPicture : StartupGraphicsVisiblePicture {
    std::size_t first_draw_emission{0};
};

// CPU-side renderer-neutral traversal and immediate emission plan. This order
// does not promise GPU execution or completion order.
struct StartupGraphicsTraversalPlan final {
    std::uint8_t requested_state{0};
    std::uint8_t effective_state{0};
    std::vector<StartupGraphicsTraversalPicture> pictures;
    std::vector<StartupGraphicsDrawEmission> draw_emissions;
};

struct StartupGraphicsRowLocation {
    std::size_t owner_directory_index{0};
    std::size_t background_directory_index{0};
    std::vector<std::size_t> chrome_directory_indexes;
};

class StartupGraphicsComposition final {
public:
    [[nodiscard]] static StartupGraphicsComposition from_rows(
        std::vector<StartupGraphicsRowComposition> rows
    );
    // Locates the unique structurally valid overlay descended from the
    // registered graphics-window owner. Unrelated matching structures are
    // ignored; missing or ambiguous semantic owners/candidates are rejected.
    [[nodiscard]] static std::vector<StartupGraphicsRowLocation> locate(
        std::span<const GmsDirectoryEntry> directory,
        std::span<const GmsHierarchyNode> hierarchy,
        std::span<const std::size_t> registered_window_owner_sources
    );

    // Precondition: image, primitive allocation, and catalog are paired local
    // archive members. Resulting draw-plan image indexes are relative to the
    // supplied catalog.
    [[nodiscard]] static StartupGraphicsComposition build(
        const GmsImage& image,
        std::span<const std::byte> primitive_allocation,
        const TextureCatalog& textures
    );

    [[nodiscard]] const std::vector<StartupGraphicsRowComposition>& rows()
        const noexcept { return rows_; }

    // Evaluates the authored state-mask and hide contract. The result
    // preserves identities but deliberately does not assert GPU draw order.
    [[nodiscard]] StartupGraphicsVisibility visible_pictures(
        std::uint8_t requested_state
    ) const;

    // Emits typed chrome pictures before the row background in descending
    // row-owner order, preserving local hierarchy order within each role.
    [[nodiscard]] StartupGraphicsTraversalPlan traversal_emission_plan(
        std::uint8_t requested_state
    ) const;

private:
    std::vector<StartupGraphicsRowComposition> rows_;
};

}  // namespace off::data
