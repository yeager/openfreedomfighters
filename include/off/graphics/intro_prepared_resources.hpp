#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/picture_texture_binding.hpp"
#include "off/graphics/texture_decode.hpp"

#include <filesystem>
#include <utility>

namespace off::graphics {

// Native CPU preparation policy, not an original-engine limit.
inline constexpr std::size_t intro_decoded_byte_budget = 64U * 1024U * 1024U;

struct IntroPreparedPicture {
  std::size_t directory_index{};
  data::GmsWindowPictureSource source;
  data::PictureResource picture;
  data::PictureTextureBindings bindings;
};

struct IntroPreparedImage {
  std::size_t catalog_image_index{};
  std::uint32_t texture_id{};
  RgbaImage mip_zero;
};

// Owning authored resources only. No runtime objects, enabled camera, clock,
// lifecycle execution, mutable material state or admitted GPU submission.
class IntroPreparedResources final {
public:
  IntroPreparedResources(IntroPreparedResources &&) noexcept = default;
  IntroPreparedResources &
  operator=(IntroPreparedResources &&) noexcept = default;
  IntroPreparedResources(const IntroPreparedResources &) = delete;
  IntroPreparedResources &operator=(const IntroPreparedResources &) = delete;

  [[nodiscard]] const data::GmsImage &sources() const noexcept {
    return sources_;
  }
  [[nodiscard]] std::span<const std::byte> source_names() const noexcept {
    return names_;
  }
  [[nodiscard]] std::size_t controller_index() const noexcept {
    return controller_index_;
  }
  [[nodiscard]] const data::GmsIntroMovieControllerSource &
  controller() const noexcept {
    return controller_;
  }
  [[nodiscard]] std::span<const std::uint32_t> cut_references() const noexcept {
    return cuts_;
  }
  [[nodiscard]] std::span<const std::uint32_t>
  group_references() const noexcept {
    return groups_;
  }
  [[nodiscard]] std::size_t first_cut_index() const noexcept {
    return first_cut_index_;
  }
  [[nodiscard]] const data::GmsIntroFirstCutSource &first_cut() const noexcept {
    return first_cut_;
  }
  [[nodiscard]] std::size_t member_index() const noexcept {
    return member_index_;
  }
  [[nodiscard]] const data::GmsIntroCutSequenceSource &member() const noexcept {
    return member_;
  }
  [[nodiscard]] std::size_t camera_index() const noexcept {
    return camera_index_;
  }
  [[nodiscard]] const data::GmsIntroCameraSource &camera() const noexcept {
    return camera_;
  }
  [[nodiscard]] std::size_t window_index() const noexcept { return window_index_; }
  [[nodiscard]] const data::GmsIntroWindowSource &window() const noexcept { return window_; }
  [[nodiscard]] const std::array<std::optional<std::string>, 5> &
  command_events() const noexcept {
    return events_;
  }
  [[nodiscard]] std::span<const IntroPreparedPicture>
  pictures() const noexcept {
    return pictures_;
  }
  [[nodiscard]] std::span<const IntroPreparedImage> images() const noexcept {
    return images_;
  }

private:
  explicit IntroPreparedResources(data::GmsImage sources)
      : sources_(std::move(sources)) {}
  friend IntroPreparedResources
  build_intro_prepared_resources(data::GmsImage, std::span<const std::byte>,
                                 std::span<const std::byte>,
                                 const data::TextureCatalog &, std::size_t);
  data::GmsImage sources_;
  std::vector<std::byte> names_;
  std::size_t controller_index_{}, first_cut_index_{}, member_index_{},
      camera_index_{}, window_index_{};
  data::GmsIntroMovieControllerSource controller_;
  std::vector<std::uint32_t> cuts_, groups_;
  data::GmsIntroFirstCutSource first_cut_;
  data::GmsIntroCutSequenceSource member_;
  data::GmsIntroCameraSource camera_;
  data::GmsIntroWindowSource window_;
  std::array<std::optional<std::string>, 5> events_;
  std::vector<IntroPreparedPicture> pictures_;
  std::vector<IntroPreparedImage> images_;
};

// Inputs must be provenance-paired supported intro resources, not arbitrary GMS
// scenes. The bounded first-cut grammar is checked; later cuts remain source
// data.
[[nodiscard]] IntroPreparedResources build_intro_prepared_resources(
    data::GmsImage sources, std::span<const std::byte> names,
    std::span<const std::byte> primitives, const data::TextureCatalog &textures,
    std::size_t decoded_byte_budget = intro_decoded_byte_budget);

// Use after install verification, for the exact supported FF-Intro.ZIP archive.
[[nodiscard]] IntroPreparedResources
load_intro_prepared_resources(const std::filesystem::path &intro_archive);

} // namespace off::graphics
