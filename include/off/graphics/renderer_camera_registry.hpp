#pragma once
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <vector>

namespace off::graphics {
struct RegisteredCamera {std::uint64_t owner;float key;};
struct CameraRegistrationServices {
  std::function<bool(std::uint64_t)> live_owner;
  std::function<void(std::uint64_t)> notify_dimensions;
  std::function<bool()> backend_ready;
  // Required only when the actual renderer backend is present and ready.
  std::function<void(std::uint64_t)> admit_view;
};
// Canonical renderer membership, distinct from backend states/views. Stable
// owner services and no reentry are native policies. Post-insertion failures
// preserve their prefix and poison the registry; they are not retryable success.
class RendererCameraRegistry final {
public:
  RendererCameraRegistry()=default;
  RendererCameraRegistry(const RendererCameraRegistry&)=delete;
  RendererCameraRegistry& operator=(const RendererCameraRegistry&)=delete;
  void register_camera(std::uint64_t owner,float key,const CameraRegistrationServices& services);
  [[nodiscard]] std::uint64_t camera_at(std::size_t index,
      const std::function<bool(std::uint64_t)>& live_owner);
  // Snapshot for diagnostics/explicit callers, not a replacement for live sweeps.
  [[nodiscard]] std::vector<RegisteredCamera> entries() const;
  [[nodiscard]] bool failed() const noexcept {return failed_;}
private:
  std::list<RegisteredCamera> entries_;
  std::optional<std::list<RegisteredCamera>::iterator> last_inserted_;
  bool busy_{},failed_{};
  void check_idle() const;
};
} // namespace off::graphics
