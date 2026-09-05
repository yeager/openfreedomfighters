#include "off/platform/startup_preparation.hpp"

#include <future>
#include <iostream>
#include <stdexcept>

namespace {
int failures{};
void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
} // namespace

int main() {
  using namespace off::platform;
  std::atomic_bool cancelled{false};
  int verify_calls{};
  int prepare_calls{};
  const auto verify = [&] {
    ++verify_calls;
    return off::data::InstallVerification{.message = "unit verification"};
  };
  const auto prepare = [&] { ++prepare_calls; };
  auto result = prepare_startup_cpu(verify, prepare, cancelled);
  check(result.outcome == StartupPreparationOutcome::ready &&
            verify_calls == 1 && prepare_calls == 1 &&
            result.message == "unit verification",
        "successful verification precedes asset preparation");
  result = prepare_startup_cpu(
      [] {
        return off::data::InstallVerification{
            .error = off::data::InstallError::missing_root,
            .message = "test missing root"};
      },
      prepare, cancelled);
  check(result.outcome == StartupPreparationOutcome::verification_error &&
            prepare_calls == 1 && result.message == "test missing root",
        "verification failure never starts asset preparation");
  result = prepare_startup_cpu(
      []() -> off::data::InstallVerification {
        throw std::runtime_error("test verify failure");
      },
      prepare, cancelled);
  check(result.outcome == StartupPreparationOutcome::verification_error &&
            result.verification.error == off::data::InstallError::io_error &&
            prepare_calls == 1,
        "verification exception stays a verification failure");
  result = prepare_startup_cpu(
      verify, [] { throw std::runtime_error("test decode failure"); },
      cancelled);
  check(result.outcome == StartupPreparationOutcome::preparation_error &&
            static_cast<bool>(result.verification) &&
            result.message.find("test decode failure") != std::string::npos,
        "preparation exception preserves successful verification and useful "
        "diagnostic");
  result = prepare_startup_cpu(verify, [] { throw 42; }, cancelled);
  check(result.outcome == StartupPreparationOutcome::preparation_error,
        "unknown preparation exception is contained");
  cancelled.store(true);
  const int previous_verifies = verify_calls;
  result = prepare_startup_cpu(verify, prepare, cancelled);
  check(result.outcome == StartupPreparationOutcome::cancelled &&
            previous_verifies == verify_calls && prepare_calls == 1,
        "pre-cancelled work invokes neither callback");
  cancelled.store(false);
  result = prepare_startup_cpu(
      [&] {
        cancelled.store(true);
        return verify();
      },
      prepare, cancelled);
  check(result.outcome == StartupPreparationOutcome::cancelled &&
            prepare_calls == 1,
        "cancellation during verification prevents starting preparation");
  cancelled.store(false);
  result =
      prepare_startup_cpu(verify, [&] { cancelled.store(true); }, cancelled);
  check(result.outcome == StartupPreparationOutcome::cancelled,
        "cancellation during preparation dominates successful callback "
        "completion");

  // Deterministic worker handoff: no timing assumptions, synthetic retail data,
  // or supported-executable verification bypass in the production entry point.
  cancelled.store(false);
  std::promise<void> started;
  std::promise<void> release;
  auto release_future = release.get_future();
  int owned_payload{};
  auto worker = std::async(std::launch::async, [&] {
    return prepare_startup_cpu(
        verify,
        [&] {
          started.set_value();
          release_future.wait();
          owned_payload = 123;
        },
        cancelled);
  });
  started.get_future().wait();
  cancelled.store(true);
  release.set_value();
  const auto joined = worker.get();
  check(
      joined.outcome == StartupPreparationOutcome::cancelled &&
          owned_payload == 123,
      "cancelled worker joins before caller accesses owned preparation output");
  return failures == 0 ? 0 : 1;
}
