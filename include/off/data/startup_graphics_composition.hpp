#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/picture_texture_binding.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::data {

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
    PictureDrawPlan draw_plan;
};

struct StartupGraphicsRowComposition {
    std::size_t owner_directory_index{0};
    float slot_y{0.0F};
    std::size_t same_slot_ordinal{0};
    std::size_t same_slot_multiplicity{1};
    std::vector<std::size_t> construction_chain;
    std::vector<StartupGraphicsLocalTransform> transform_chain;
    std::array<StartupGraphicsCompositionInstance, 3> pictures;
};

struct StartupGraphicsRowLocation {
    std::size_t owner_directory_index{0};
    std::size_t background_directory_index{0};
    std::array<std::size_t, 2> chrome_directory_indexes{};
};

class StartupGraphicsComposition final {
public:
    [[nodiscard]] static StartupGraphicsComposition from_rows(
        std::array<StartupGraphicsRowComposition, 8> rows
    );
    // Exact binary32 X/Y comparisons are intentional: -0 equals +0, while an
    // adjacent nextafter value does not. Z is retained but is not a locator key.
    [[nodiscard]] static std::array<StartupGraphicsRowLocation, 8> locate(
        std::span<const GmsDirectoryEntry> directory,
        std::span<const GmsHierarchyNode> hierarchy
    );

    // Precondition: image, primitive allocation, and catalog are the paired
    // members of the already verified supported startup archive. Image indexes
    // in resulting draw plans are identities relative to the supplied catalog.
    [[nodiscard]] static StartupGraphicsComposition build(
        const GmsImage& image,
        std::span<const std::byte> primitive_allocation,
        const TextureCatalog& textures
    );

    [[nodiscard]] const std::array<StartupGraphicsRowComposition, 8>& rows()
        const noexcept { return rows_; }

private:
    std::array<StartupGraphicsRowComposition, 8> rows_;
};

}  // namespace off::data
