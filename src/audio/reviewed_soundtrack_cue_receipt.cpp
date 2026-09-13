#include "off/audio/reviewed_soundtrack_cue_receipt.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>

namespace off::audio {
namespace {
constexpr std::string_view kFilename{"reviewed-soundtrack-cue-bindings.json"};
constexpr std::size_t kMaximumFileBytes = 256U << 10U;
constexpr std::size_t kMaximumDepth = 5U;
constexpr std::size_t kMaximumBindings = 255U;

struct Json final {
  using Object = std::map<std::string, Json, std::less<>>;
  using Array = std::vector<Json>;
  std::variant<std::uint64_t, std::string, Object, Array> value;
};

class Parser final {
 public:
  explicit Parser(std::string_view source) : source_(source) {}
  [[nodiscard]] std::optional<Json> parse() {
    auto result = value(0U);
    spaces();
    return result && cursor_ == source_.size() ? result : std::nullopt;
  }
 private:
  void spaces() { while (cursor_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[cursor_]))) ++cursor_; }
  [[nodiscard]] bool consume(const char expected) {
    spaces();
    if (cursor_ == source_.size() || source_[cursor_] != expected) return false;
    ++cursor_;
    return true;
  }
  [[nodiscard]] std::optional<std::string> string() {
    spaces();
    if (cursor_ == source_.size() || source_[cursor_++] != '"') return std::nullopt;
    std::string result;
    while (cursor_ < source_.size()) {
      const char character = source_[cursor_++];
      if (character == '"') return result;
      const auto byte = static_cast<unsigned char>(character);
      if (character == '\\' || byte < 0x20U || byte > 0x7eU || result.size() >= 128U) return std::nullopt;
      result.push_back(character);
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<Json> number() {
    spaces();
    const auto begin = cursor_;
    while (cursor_ < source_.size() && source_[cursor_] >= '0' && source_[cursor_] <= '9') ++cursor_;
    if (begin == cursor_ || (cursor_ - begin > 1U && source_[begin] == '0')) return std::nullopt;
    std::uint64_t result{};
    for (std::size_t index = begin; index < cursor_; ++index) {
      const auto digit = static_cast<std::uint64_t>(source_[index] - '0');
      if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) return std::nullopt;
      result = result * 10U + digit;
    }
    return Json{result};
  }
  [[nodiscard]] std::optional<Json> object(const std::size_t depth) {
    if (!consume('{')) return std::nullopt;
    Json::Object result;
    spaces();
    if (cursor_ < source_.size() && source_[cursor_] == '}') { ++cursor_; return Json{std::move(result)}; }
    for (;;) {
      const auto key = string();
      if (!key || !consume(':')) return std::nullopt;
      const auto item = value(depth + 1U);
      if (!item || !result.emplace(*key, *item).second) return std::nullopt;
      spaces();
      if (cursor_ < source_.size() && source_[cursor_] == '}') { ++cursor_; return Json{std::move(result)}; }
      if (!consume(',')) return std::nullopt;
    }
  }
  [[nodiscard]] std::optional<Json> array(const std::size_t depth) {
    if (!consume('[')) return std::nullopt;
    Json::Array result;
    spaces();
    if (cursor_ < source_.size() && source_[cursor_] == ']') { ++cursor_; return Json{std::move(result)}; }
    for (;;) {
      if (result.size() == kMaximumBindings) return std::nullopt;
      const auto item = value(depth + 1U);
      if (!item) return std::nullopt;
      result.push_back(*item);
      spaces();
      if (cursor_ < source_.size() && source_[cursor_] == ']') { ++cursor_; return Json{std::move(result)}; }
      if (!consume(',')) return std::nullopt;
    }
  }
  [[nodiscard]] std::optional<Json> value(const std::size_t depth) {
    if (depth > kMaximumDepth) return std::nullopt;
    spaces();
    if (cursor_ == source_.size()) return std::nullopt;
    if (source_[cursor_] == '{') return object(depth);
    if (source_[cursor_] == '[') return array(depth);
    if (source_[cursor_] == '"') { const auto result = string(); return result ? std::optional<Json>{Json{*result}} : std::nullopt; }
    return number();
  }
  std::string_view source_;
  std::size_t cursor_{};
};

[[nodiscard]] bool regular_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status);
}
[[nodiscard]] bool directory_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
[[nodiscard]] const Json::Object* object(const Json& value) { return std::get_if<Json::Object>(&value.value); }
[[nodiscard]] const Json::Array* array(const Json& value) { return std::get_if<Json::Array>(&value.value); }
[[nodiscard]] const Json* member(const Json::Object& value, const std::string_view key) {
  const auto found = value.find(key);
  return found == value.end() ? nullptr : &found->second;
}
[[nodiscard]] const std::string* string(const Json& value) { return std::get_if<std::string>(&value.value); }
[[nodiscard]] const std::uint64_t* integer(const Json& value) { return std::get_if<std::uint64_t>(&value.value); }
template <std::size_t N>
[[nodiscard]] bool exact_keys(const Json::Object& value, const std::array<std::string_view, N>& keys) {
  return value.size() == keys.size() && std::ranges::all_of(keys, [&value](const auto key) { return value.contains(key); });
}
[[nodiscard]] bool equals(const Json::Object& value, const std::string_view key, const std::string_view expected) {
  const auto item = member(value, key);
  const auto actual = item ? string(*item) : nullptr;
  return actual && *actual == expected;
}
[[nodiscard]] bool sha256(const std::string_view value) {
  return value.size() == 64U && std::ranges::all_of(value, [](const unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}
[[nodiscard]] std::optional<SoundtrackFormat> format(const std::string_view value) {
  if (value == "flac") return SoundtrackFormat::flac;
  if (value == "mp3") return SoundtrackFormat::mp3;
  return std::nullopt;
}
[[nodiscard]] std::optional<std::vector<ReviewedSoundtrackCueBinding>> validated(const Json& root) {
  const auto top = object(root);
  constexpr std::array<std::string_view, 3U> top_keys{
      "format", "verified_data_manifest_fingerprint", "bindings"};
  if (!top || !exact_keys(*top, top_keys) ||
      !equals(*top, "format", "off.reviewed-soundtrack-cue-bindings/v1")) return std::nullopt;
  const auto fingerprint_item = member(*top, "verified_data_manifest_fingerprint");
  const auto binding_item = member(*top, "bindings");
  const auto fingerprint = fingerprint_item ? string(*fingerprint_item) : nullptr;
  const auto entries = binding_item ? array(*binding_item) : nullptr;
  if (!fingerprint || !sha256(*fingerprint) || !entries || entries->empty() || entries->size() > kMaximumBindings) return std::nullopt;
  constexpr std::array<std::string_view, 4U> entry_keys{
      "cue_token", "album_ordinal", "format", "expected_sha256"};
  std::vector<ReviewedSoundtrackCueBinding> result;
  result.reserve(entries->size());
  for (const auto& entry : *entries) {
    const auto item = object(entry);
    if (!item || !exact_keys(*item, entry_keys)) return std::nullopt;
    const auto token_item = member(*item, "cue_token");
    const auto ordinal_item = member(*item, "album_ordinal");
    const auto format_item = member(*item, "format");
    const auto digest_item = member(*item, "expected_sha256");
    const auto token = token_item ? integer(*token_item) : nullptr;
    const auto ordinal = ordinal_item ? integer(*ordinal_item) : nullptr;
    const auto format_text = format_item ? string(*format_item) : nullptr;
    const auto digest = digest_item ? string(*digest_item) : nullptr;
    const auto parsed_format = format_text ? format(*format_text) : std::nullopt;
    if (!token || *token == 0U || !ordinal || *ordinal == 0U || *ordinal > 255U || !parsed_format || !digest || !sha256(*digest) ||
        std::ranges::any_of(result, [token](const auto& prior) { return prior.cue_token == *token; })) return std::nullopt;
    result.push_back({.cue_token = *token, .album_ordinal = static_cast<std::uint8_t>(*ordinal), .format = *parsed_format, .expected_sha256 = *digest});
  }
  return result;
}
}  // namespace

std::optional<ReviewedSoundtrackCueReceipt> ReviewedSoundtrackCueReceipt::load_local(
    const std::filesystem::path& local_directory) {
  if (local_directory.empty() || !directory_non_link(local_directory)) return std::nullopt;
  const auto path = local_directory / std::string{kFilename};
  if (path.parent_path() != local_directory || !regular_non_link(path)) return std::nullopt;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0U || size > kMaximumFileBytes) return std::nullopt;
  std::ifstream input(path, std::ios::binary);
  std::string bytes(static_cast<std::size_t>(size), '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()) || input.peek() != std::char_traits<char>::eof()) return std::nullopt;
  const auto parsed = Parser{bytes}.parse();
  const auto bindings = parsed ? validated(*parsed) : std::nullopt;
  return bindings ? std::optional<ReviewedSoundtrackCueReceipt>{
                        ReviewedSoundtrackCueReceipt{std::move(bindings).value()}}
                  : std::nullopt;
}

}  // namespace off::audio
