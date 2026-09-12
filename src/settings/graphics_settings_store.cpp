#include "off/settings/graphics_settings_store.hpp"
#include <array>
#include <atomic>
#include <charconv>
#include <exception>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace off::settings {
namespace {
constexpr std::string_view header{"off-graphics-settings=1"};
constexpr std::size_t max_document = 4096, max_line = 256;
constexpr unsigned attempts = 32;
std::atomic<unsigned long> counter{0};
template <class T> std::optional<T> number(std::string_view v) {
  T r{};
  auto [e, x] = std::from_chars(v.data(), v.data() + v.size(), r);
  return x == std::errc{} && e == v.data() + v.size() ? std::optional<T>{r}
                                                      : std::nullopt;
}
bool valid(const RequestedGraphicsSettings &s) {
  return (s.profile == Mode::original || s.profile == Mode::modern) &&
         (s.window_mode == WindowMode::windowed ||
          s.window_mode == WindowMode::borderless_desktop) &&
         (s.present_mode == PresentMode::vsync ||
          s.present_mode == PresentMode::mailbox ||
          s.present_mode == PresentMode::immediate) &&
         (s.upscaler == Upscaler::native || s.upscaler == Upscaler::temporal ||
          s.upscaler == Upscaler::dlss || s.upscaler == Upscaler::fsr ||
          s.upscaler == Upscaler::xess) &&
         (s.shadow_quality == ShadowQuality::reference ||
          s.shadow_quality == ShadowQuality::high ||
          s.shadow_quality == ShadowQuality::ultra) &&
         s.windowed_size.width && s.windowed_size.height &&
         s.render_scale_percent >= 50 && s.render_scale_percent <= 200;
}
std::string serialize(const RequestedGraphicsSettings &s) {
  return std::string(header) +
         "\nprofile=" + std::to_string(static_cast<unsigned>(s.profile)) +
         "\nwindow_mode=" +
         std::to_string(static_cast<unsigned>(s.window_mode)) +
         "\nwidth=" + std::to_string(s.windowed_size.width) +
         "\nheight=" + std::to_string(s.windowed_size.height) +
         "\npresent_mode=" +
         std::to_string(static_cast<unsigned>(s.present_mode)) +
         "\nmodern_plus=" + (s.modern_plus ? "1" : "0") +
         "\nrender_scale_percent=" + std::to_string(s.render_scale_percent) +
         "\nupscaler=" + std::to_string(static_cast<unsigned>(s.upscaler)) +
         "\nshadow_quality=" +
         std::to_string(static_cast<unsigned>(s.shadow_quality)) + "\n";
}
#ifndef _WIN32
bool write_all(int fd, std::string_view v) {
  while (!v.empty()) {
    auto n = write(fd, v.data(), v.size());
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    v.remove_prefix(static_cast<std::size_t>(n));
  }
  return true;
}
#endif
bool create_temp(const std::filesystem::path &path, std::string_view body,
                 std::filesystem::path &tmp) {
  const auto dir = path.parent_path().empty() ? std::filesystem::path{"."}
                                              : path.parent_path();
  for (unsigned i = 0; i < attempts; ++i) {
    tmp = dir / (path.filename().string() + ".tmp-" +
                 std::to_string(counter.fetch_add(1)));
#ifdef _WIN32
    HANDLE f =
        CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (f == INVALID_HANDLE_VALUE)
      continue;
    DWORD n = 0;
    bool ok = body.size() <= MAXDWORD &&
              WriteFile(f, body.data(), static_cast<DWORD>(body.size()), &n,
                        nullptr) != 0 &&
              n == body.size() && FlushFileBuffers(f) != 0;
    CloseHandle(f);
    return ok;
#else
    int fd = open(tmp.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
    if (fd < 0) {
      if (errno == EEXIST)
        continue;
      return false;
    }
    bool ok = write_all(fd, body) && fsync(fd) == 0;
    close(fd);
    return ok;
#endif
  }
  return false;
}
bool replace(const std::filesystem::path &from,
             const std::filesystem::path &to) {
#ifdef _WIN32
  return MoveFileExW(from.c_str(), to.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code ec;
  std::filesystem::rename(from, to, ec);
  if (ec)
    return false;
  const auto dir =
      to.parent_path().empty() ? std::filesystem::path{"."} : to.parent_path();
  int fd = open(dir.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd < 0)
    return false;
  bool ok = fsync(fd) == 0;
  close(fd);
  return ok;
#endif
}
} // namespace
GraphicsSettingsLoadResult
load_graphics_settings(const std::filesystem::path &path) {
  std::error_code status_error;
  const auto status = std::filesystem::status(path, status_error);
  if (status_error == std::errc::no_such_file_or_directory)
    return {GraphicsSettingsLoadStatus::missing, {}};
  if (status_error)
    return {GraphicsSettingsLoadStatus::io_error, {}};
  if (!std::filesystem::exists(status))
    return {GraphicsSettingsLoadStatus::missing, {}};
  // Opening a directory is platform-dependent: some standard-library
  // implementations accept it and report an empty stream, while others fail
  // immediately. Settings are a regular file on every supported platform.
  if (!std::filesystem::is_regular_file(status))
    return {GraphicsSettingsLoadStatus::io_error, {}};
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {GraphicsSettingsLoadStatus::io_error, {}};
  }
  std::string d(max_document + 1, '\0');
  in.read(d.data(), static_cast<std::streamsize>(d.size()));
  d.resize(static_cast<std::size_t>(in.gcount()));
  if (!in.eof() && !in.good())
    return {GraphicsSettingsLoadStatus::io_error, {}};
  if (d.size() > max_document)
    return {GraphicsSettingsLoadStatus::invalid, {}};
  std::array<std::string, 9> values;
  std::array<bool, 9> seen{};
  std::size_t pos = 0;
  unsigned row = 0;
  const std::array<std::string_view, 9> keys{"profile",
                                             "window_mode",
                                             "width",
                                             "height",
                                             "present_mode",
                                             "modern_plus",
                                             "render_scale_percent",
                                             "upscaler",
                                             "shadow_quality"};
  while (pos < d.size()) {
    const auto nl = d.find('\n', pos),
               end = nl == std::string::npos ? d.size() : nl;
    if (end - pos > max_line)
      return {GraphicsSettingsLoadStatus::invalid, {}};
    const std::string_view line(d.data() + pos, end - pos);
    pos = nl == std::string::npos ? d.size() : nl + 1;
    if (row++ == 0) {
      if (line != header)
        return {GraphicsSettingsLoadStatus::invalid, {}};
      continue;
    }
    if (line.empty())
      return {GraphicsSettingsLoadStatus::invalid, {}};
    const auto eq = line.find('=');
    if (eq == std::string_view::npos || eq == 0 ||
        line.find('=', eq + 1) != std::string_view::npos)
      return {GraphicsSettingsLoadStatus::invalid, {}};
    unsigned index = 9;
    for (unsigned i = 0; i < keys.size(); ++i)
      if (line.substr(0, eq) == keys[i]) {
        index = i;
        break;
      }
    const auto v = line.substr(eq + 1);
    if (index == 9 || seen[index] || v.empty())
      return {GraphicsSettingsLoadStatus::invalid, {}};
    seen[index] = true;
    values[index] = v;
  }
  if (row == 0)
    return {GraphicsSettingsLoadStatus::invalid, {}};
  for (bool s : seen)
    if (!s)
      return {GraphicsSettingsLoadStatus::invalid, {}};
  const auto p = number<unsigned>(values[0]);
  const auto w = number<unsigned>(values[1]);
  const auto x = number<std::uint32_t>(values[2]);
  const auto y = number<std::uint32_t>(values[3]);
  const auto pm = number<unsigned>(values[4]);
  const auto scale = number<std::uint16_t>(values[6]);
  const auto u = number<unsigned>(values[7]);
  const auto sh = number<unsigned>(values[8]);
  if (!p || !w || !x || !y || !pm || !scale || !u || !sh ||
      (values[5] != "0" && values[5] != "1"))
    return {GraphicsSettingsLoadStatus::invalid, {}};
  RequestedGraphicsSettings s{static_cast<Mode>(*p),
                              static_cast<WindowMode>(*w),
                              {*x, *y},
                              static_cast<PresentMode>(*pm),
                              values[5] == "1",
                              *scale,
                              static_cast<Upscaler>(*u),
                              static_cast<ShadowQuality>(*sh)};
  return valid(s)
             ? GraphicsSettingsLoadResult{GraphicsSettingsLoadStatus::loaded, s}
             : GraphicsSettingsLoadResult{GraphicsSettingsLoadStatus::invalid,
                                          {}};
}
RequestedGraphicsSettings load_initial_graphics_settings(
    const std::filesystem::path &path, Mode command_line_mode,
    bool command_line_mode_explicit) noexcept {
  RequestedGraphicsSettings selected;
  if (!path.empty()) {
    try {
      const auto loaded = load_graphics_settings(path);
      if (loaded.status == GraphicsSettingsLoadStatus::loaded &&
          loaded.settings.has_value())
        selected = *loaded.settings;
    } catch (const std::exception &) {
      // Preferences are optional. A filesystem exception must not make a
      // verified game installation unstartable.
    }
  }
  if (command_line_mode_explicit)
    selected.profile = command_line_mode;
  return selected;
}
bool save_graphics_settings(const std::filesystem::path &path,
                            const RequestedGraphicsSettings &s) {
  if (!valid(s) || path.empty())
    return false;
  std::filesystem::path tmp;
  if (!create_temp(path, serialize(s), tmp))
    return false;
  if (!replace(tmp, path)) {
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    return false;
  }
  return true;
}
} // namespace off::settings
