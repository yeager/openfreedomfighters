#include "off/platform/startup_preparation.hpp"

#include <exception>

namespace off::platform {

StartupPreparationResult
prepare_startup_cpu(const std::function<data::InstallVerification()> &verify,
                    const std::function<void()> &prepare_assets,
                    const std::atomic_bool &cancelled,
                    StartupPreparationStageObserver observe_stage,
                    const std::function<data::InstallVerification()> &reverify) {
  StartupPreparationResult result;
  const auto stop = [&] {
    if (!cancelled.load())
      return false;
    result.outcome = StartupPreparationOutcome::cancelled;
    result.message = "Startup cancelled";
    return true;
  };
  if (stop())
    return result;
  try {
    if (observe_stage)
      observe_stage(StartupPreparationStage::verifying_game_data);
    result.verification = verify();
  } catch (const std::exception &error) {
    result.verification.error = data::InstallError::io_error;
    result.verification.message = error.what();
  } catch (...) {
    result.verification.error = data::InstallError::io_error;
    result.verification.message =
        "Game-data verification could not be completed";
  }
  if (stop())
    return result;
  if (!result.verification) {
    result.outcome = StartupPreparationOutcome::verification_error;
    result.message = result.verification.message;
    return result;
  }
  try {
    if (observe_stage)
      observe_stage(StartupPreparationStage::preparing_assets);
    prepare_assets();
  } catch (const std::exception &error) {
    result.message =
        "Runtime data loading failed: " + std::string(error.what());
    if (!stop())
      result.outcome = StartupPreparationOutcome::preparation_error;
    return result;
  } catch (...) {
    result.message = "Runtime data loading failed with an unknown error";
    if (!stop())
      result.outcome = StartupPreparationOutcome::preparation_error;
    return result;
  }
  if (stop())
    return result;
  if (reverify) {
    try {
      if (observe_stage)
        observe_stage(StartupPreparationStage::verifying_game_data);
      result.verification = reverify();
    } catch (const std::exception &error) {
      result.verification.error = data::InstallError::io_error;
      result.verification.message = error.what();
    } catch (...) {
      result.verification.error = data::InstallError::io_error;
      result.verification.message =
          "Game-data handoff verification could not be completed";
    }
    if (stop())
      return result;
    if (!result.verification) {
      result.outcome = StartupPreparationOutcome::verification_error;
      result.message = result.verification.message;
      return result;
    }
  }
  result.outcome = StartupPreparationOutcome::ready;
  result.message = result.verification.message;
  return result;
}

} // namespace off::platform
