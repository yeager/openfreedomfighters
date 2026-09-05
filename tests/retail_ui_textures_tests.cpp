#include "off/ui/retail_ui_textures.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

off::data::TextureImage image(std::string name, std::uint32_t width,
                              std::uint32_t height, std::byte value = {}) {
  return {
      .encoding = off::data::TextureEncoding::abgr32,
      .width = width,
      .height = height,
      .name = std::move(name),
      .mips = {{.width = width,
                .height = height,
                .encoded = std::vector<std::byte>(
                    static_cast<std::size_t>(width) * height * 4U, value)}}};
}

std::vector<off::data::TextureImage> complete_set() {
  std::vector<off::data::TextureImage> images;
  images.push_back(image("fixture_00", 128, 128));
  images.push_back(image("fixture_01", 128, 128));
  images.push_back(image("fixture_02", 16, 16));
  images.push_back(image("fixture_03", 16, 16));
  for (int ordinal = 1; ordinal <= 8; ++ordinal) {
    images.push_back(image("fixture_" + std::to_string(ordinal + 3), 2, 2,
                           static_cast<std::byte>(ordinal)));
  }
  images.push_back(image("fixture_12", 16, 16));
  images.push_back(image("fixture_13", 16, 16));
  images.push_back(image("fixture_14", 16, 16));
  images.push_back(image("fixture_15", 16, 16));
  return images;
}

std::vector<off::ui::RetailUiTextureBinding> complete_bindings() {
  std::vector<off::ui::RetailUiTextureBinding> bindings;
  for (std::size_t index = 0; index < 16; ++index) {
    bindings.push_back(
        {static_cast<off::ui::RetailUiTextureRole>(index), index});
  }
  return bindings;
}

bool rejects(const std::vector<off::data::TextureImage> &images,
             const std::vector<off::ui::RetailUiTextureBinding> &bindings) {
  try {
    static_cast<void>(off::ui::resolve_retail_ui_textures(images, bindings));
    return false;
  } catch (const std::runtime_error &) {
    return true;
  }
}

} // namespace

int main() {
  auto images = complete_set();
  auto bindings = complete_bindings();
  const auto textures = off::ui::resolve_retail_ui_textures(images, bindings);
  check(textures.textures().size() == 16, "resolve every required role");
  check(textures.find(off::ui::RetailUiTextureRole::arrow_left) != nullptr,
        "look up a resolved role without exposing a retail identifier");

  auto reversed = bindings;
  std::ranges::reverse(reversed);
  const auto resolved_reversed =
      off::ui::resolve_retail_ui_textures(images, reversed);
  check(resolved_reversed.textures().size() == bindings.size() &&
            resolved_reversed.textures().front().role ==
                off::ui::RetailUiTextureRole::scanlines_top &&
            resolved_reversed.textures().back().role ==
                off::ui::RetailUiTextureRole::arrow_down,
        "canonicalize output independently of binding order");

  auto duplicate_names = images;
  for (auto &candidate : duplicate_names)
    candidate.name = "duplicate_fixture_name";
  const auto resolved_duplicate_names =
      off::ui::resolve_retail_ui_textures(duplicate_names, bindings);
  check(resolved_duplicate_names.textures().size() == 16,
        "resolve explicit bindings independently of duplicate image names");

  auto shared_fill = bindings;
  shared_fill[3].image_index = shared_fill[2].image_index;
  check(rejects(images, shared_fill),
        "reject reuse between distinct top and bottom fill roles");

  auto missing = bindings;
  missing.pop_back();
  check(rejects(images, missing), "reject a partial role set");

  auto ambiguous = bindings;
  ambiguous.back().role = ambiguous.front().role;
  check(rejects(images, ambiguous), "reject an ambiguous exclusive role");

  auto reused = bindings;
  reused.back().image_index = reused[reused.size() - 2].image_index;
  check(rejects(images, reused), "reject reuse between exclusive roles");

  auto wrong_size = images;
  wrong_size[0] = image("fixture_wrong_size", 64, 64);
  check(rejects(wrong_size, bindings), "enforce recovered role dimensions");

  auto out_of_range_role = bindings;
  out_of_range_role[0].role = static_cast<off::ui::RetailUiTextureRole>(255);
  check(rejects(images, out_of_range_role), "reject an unknown semantic role");

  auto out_of_range_image = bindings;
  out_of_range_image[0].image_index = images.size();
  check(rejects(images, out_of_range_image),
        "reject an out-of-range image binding");

  auto empty_mips = images;
  empty_mips[0].mips.clear();
  check(rejects(empty_mips, bindings), "reject an image without mip zero");

  auto mismatched_mip = images;
  mismatched_mip[0].mips.front().width = 64;
  check(rejects(mismatched_mip, bindings),
        "reject disagreement between image and mip dimensions");

  auto malformed_mip = images;
  malformed_mip[0].mips.front().encoded.pop_back();
  check(rejects(malformed_mip, bindings),
        "reject a truncated encoded retail UI mip");
}
