#include "off/graphics/normal_intro_scene_host_factory.hpp"

#include <stdexcept>
#include <utility>

namespace off::graphics {
namespace {
NormalIntroSceneHost construct(const MovieControlHostEvidence &evidence,
                               NormalIntroSceneHostLifecycleServices lifecycle,
                               NormalIntroSceneHostFirstCutConfig first_cut) {
  // Do not call or preflight the callbacks here. They are lifecycle work and
  // belong exclusively to NormalIntroSceneHost activation.
  return {
      evidence.movie_component_handle(),
      evidence.movie_owner_handle(),
      evidence.movie_delay(),
      {.reader_bracket = std::move(lifecycle.reader_bracket),
       .outer_loader_tail = std::move(lifecycle.outer_loader_tail),
       .enter_global_lifecycle = std::move(lifecycle.enter_global_lifecycle)},
      first_cut};
}
} // namespace

NormalIntroSceneHost NormalIntroSceneHostFactory::create(
    const MovieControlHostEvidence &evidence,
    NormalIntroSceneHostLifecycleServices lifecycle,
    NormalIntroSceneHostFirstCutConfig first_cut) {
  if (!lifecycle.reader_bracket || !lifecycle.outer_loader_tail ||
      !lifecycle.enter_global_lifecycle)
    throw std::runtime_error(
        "normal intro scene host factory requires complete lifecycle services");
  return construct(evidence, std::move(lifecycle), first_cut);
}

NormalIntroSceneHost NormalIntroSceneHostFactory::create_after_reader_bracket(
    const MovieControlHostEvidence &evidence,
    NormalIntroScenePostReaderHostLifecycleServices lifecycle,
    NormalIntroSceneHostFirstCutConfig first_cut) {
  if (!lifecycle.outer_loader_tail_ || !lifecycle.enter_global_lifecycle_)
    throw std::runtime_error("normal intro scene post-reader factory requires "
                             "only continuation lifecycle services");
  return construct(
      evidence,
      {.reader_bracket = {},
       .outer_loader_tail = std::move(lifecycle.outer_loader_tail_),
       .enter_global_lifecycle = std::move(lifecycle.enter_global_lifecycle_)},
      first_cut);
}
} // namespace off::graphics
