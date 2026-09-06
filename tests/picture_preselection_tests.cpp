#include "off/graphics/picture_preselection.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool value, const char* message) {
  if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation, const char* message) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, message);
}
struct Fixture {
  PictureSelectionContext context{1, true, 10, 0, 7, 100};
  std::uint8_t override_byte = 0;
  bool eligible = true;
  std::vector<std::string> trace;
  std::vector<std::uint64_t> selected;
  void event(const char* name) { trace.emplace_back(name); }
  PicturePreselectionHooks hooks() {
    return {
      [&](auto backend) { check(backend == 10, "backend identity"); event("owner"); return 20; },
      [&](auto) { event("override"); return override_byte; },
      [&](auto) { event("interface"); return 30; },
      [&](auto) { event("identifier"); return 40; },
      [&](auto id) { check(id == 40, "selection identifier preserved"); event("resolve"); return 50; },
      [&](auto view) { check(view == 7 || view == 8, "live view queried"); event("camera"); return 60; },
      [&](auto camera) { check(camera == 60, "camera identity preserved"); event("prepare"); },
      [&](const auto&, auto) { event("point"); return PicturePreselectionHooks::Point{1,2,3}; },
      [&](auto) -> PicturePreselectionHooks::Optional { event("extension"); return 70; },
      [&](auto query, const auto& point, auto extension, auto selection) {
        check(query == 30 && point == PicturePreselectionHooks::Point{1,2,3} && extension == 70 && selection == 50,
          "predicate argument identities/order preserved"); event("predicate"); return eligible;
      },
      [&](auto) -> PicturePreselectionHooks::Optional { event("next_record"); return {}; },
      [&](auto, auto output) { check(output.size() == 8, "related query capacity eight"); event("related"); return std::size_t{0}; },
      [](auto) -> PicturePreselectionHooks::Optional { return {}; },
      [](auto) { return std::uint32_t{0}; },
      [](auto) -> PicturePreselectionHooks::Optional { return {}; },
      [](auto) { return std::uint64_t{0}; },
      [](auto) -> PictureSelectionContext* { return nullptr; },
      [](auto, auto) -> PicturePreselectionHooks::Optional { return {}; }
    };
  }
  void append(std::uint64_t record) { event("append"); selected.push_back(record); }
};
}
int main() {
  {
    Fixture f;
    PictureSelectionContext dead{2, false, 0, 255, {}, {}};
    std::array<PictureSelectionContext*,3> registry{&dead, &f.context, &dead};
    PicturePreselection preselection(registry);
    preselection.run(8, f.hooks(), [&](auto r) { f.append(r); });
    check(preselection.registry_size() == 1 && f.selected == std::vector<std::uint64_t>{100},
      "inactive removal neither advances past successor nor dereferences inactive backend");
    check(f.trace == std::vector<std::string>{"owner","override","interface","identifier","resolve",
      "camera","prepare","point","extension","predicate","append","next_record","related"},
      "selection query, camera, geometry, predicate and append order");
  }
  for (std::uint8_t participation : {0,1,2}) {
    Fixture f; f.context.participation = participation;
    std::array registry{&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    hooks.selection_interface = [&](auto) { f.override_byte = 1; return 30; };
    selection.run(8, hooks, [&](auto r) { f.append(r); });
    if (participation == 2)
      check(f.trace == std::vector<std::string>{"owner","override"}, "early gate stops after unconditional override read");
    else check(f.selected.size() == 1 && std::find(f.trace.begin(), f.trace.end(), "predicate") != f.trace.end(),
      "captured zero override is reused despite later mutation");
  }
  {
    Fixture f; f.override_byte = 1;
    std::array registry{&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    hooks.resolve_selection = [&](auto) { f.override_byte = 0; f.context.associated_view = 8; return 50; };
    hooks.view_camera = [](auto view) { check(view == 8, "camera uses association changed by selection query"); return 60; };
    selection.run(8, hooks, [&](auto r) { f.append(r); });
    check(f.selected.size() == 1 && std::find(f.trace.begin(),f.trace.end(),"point") != f.trace.end() &&
      std::find(f.trace.begin(),f.trace.end(),"predicate") == f.trace.end() &&
      std::find(f.trace.begin(),f.trace.end(),"extension") == f.trace.end(),
      "captured override skips only extension/predicate, not geometry or live camera preparation");
  }
  {
    Fixture f; f.eligible = false;
    std::array registry{&f.context}; PicturePreselection selection(registry);
    selection.run(8, f.hooks(), [&](auto r) { f.append(r); });
    check(f.selected.empty() && f.trace.back() == "predicate", "false predicate does not collect direct or related records");
  }
  {
    Fixture f; f.context.associated_view.reset();
    std::array registry{&f.context}; PicturePreselection selection(registry);
    selection.run(8, f.hooks(), [&](auto r) { f.append(r); });
    check(f.selected.size() == 1 && std::find(f.trace.begin(),f.trace.end(),"camera") == f.trace.end(),
      "initial null view bypasses camera preparation but still invokes geometry");
  }
  {
    Fixture f; auto second = f.context; second.identity = 2; second.associated_view.reset();
    std::array registry{&f.context,&second}; PicturePreselection selection(registry);
    rejects([&] { selection.run(8, f.hooks(), [&](auto r) { f.append(r); }); }, "nonnull to null view rejects");
    check(selection.poisoned() && f.selected == std::vector<std::uint64_t>{100}, "null view failure preserves first context prefix");
  }
  {
    Fixture f; PictureSelectionContext related_context{2,true,11,0,{},100};
    std::array registry{&f.context,&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    hooks.related_resources = [](auto, auto out) { out[0]=200; out[1]=200; out[2]=201; return std::size_t{3}; };
    hooks.resource_owner = [](auto resource) -> PicturePreselectionHooks::Optional {
      if (resource == 200) return 300;
      if (resource == 400 || resource == 401) return 301;
      return {};
    };
    hooks.owner_capabilities = [](auto owner) { return owner == 300 ? 0x100000U : 0x200000U; };
    hooks.current_resource = [](auto owner) -> PicturePreselectionHooks::Optional {
      check(owner == 300, "initial related owner current resource receiver"); return 400;
    };
    hooks.resource_registry_identifier = [](auto resource) { return resource == 400 ? 500U : 0U; };
    hooks.state_context = [&](auto id) { check(id == 500, "registry lookup identifier"); return &related_context; };
    unsigned enumerations = 0;
    hooks.next_resource = [&](auto owner, auto resource) -> PicturePreselectionHooks::Optional {
      check(owner == 300, "enumeration receiver is not each resource live owner"); ++enumerations;
      return resource == 400 ? PicturePreselectionHooks::Optional{401} : std::nullopt;
    };
    selection.run(8, hooks, [&](auto r) { f.append(r); });
    check(f.selected == std::vector<std::uint64_t>(6,100) && enumerations == 8,
      "duplicate registry/related paths append every occurrence without deduplication");
    check(std::count(f.trace.begin(),f.trace.end(),"prepare") == 1, "consecutive same view prepares once per invocation");
  }
  for (unsigned failure = 0; failure != 7; ++failure) {
    Fixture f; PictureSelectionContext dead{2,false,0,0,{},{}};
    std::array registry{&dead,&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    if (failure == 0) hooks.related_resources = [](auto, auto) { return std::size_t{9}; };
    if (failure == 1) hooks.next_record = [](auto record) { return PicturePreselectionHooks::Optional{record}; };
    if (failure == 2) hooks.relative_point = [](const auto&, auto) { return PicturePreselectionHooks::Point{0,0,std::numeric_limits<float>::infinity()}; };
    if (failure == 3) hooks.backend_owner = [](auto) { return std::uint64_t{0}; };
    if (failure >= 4) {
      hooks.related_resources = [](auto, auto out) { out[0]=200; return std::size_t{1}; };
      hooks.resource_owner = [](auto) { return PicturePreselectionHooks::Optional{300}; };
      hooks.owner_capabilities = [](auto) { return 0x300000U; };
      hooks.current_resource = [](auto) { return PicturePreselectionHooks::Optional{400}; };
      hooks.resource_registry_identifier = [failure](auto) { return failure == 4 ? 1U : 0U; };
      hooks.next_resource = [failure](auto, auto resource) { return PicturePreselectionHooks::Optional{resource + (failure == 6)}; };
    }
    rejects([&] { selection.run(2, hooks, [&](auto r) { f.append(r); }); }, "invalid producer, cycle, or chain bound rejects");
    check(selection.poisoned() && selection.registry_size() == 1, "failed selection retains inactive removal and poisons");
    rejects([&] { selection.run(8, f.hooks(), [](auto) {}); }, "poisoned selection cannot retry");
  }
  {
    Fixture f; std::array registry{&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    hooks.next_record = [](auto record) -> PicturePreselectionHooks::Optional { return record + 1; };
    rejects([&] { selection.run(9000, hooks, [&](auto r) { f.append(r); }); }, "8193rd append rejected");
    check(f.selected.size() == 8192, "append capacity rejects before extra callback");
  }
  {
    Fixture f; std::array registry{&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    hooks.next_record = [](auto record) -> PicturePreselectionHooks::Optional {
      return record == 8291 ? std::nullopt : PicturePreselectionHooks::Optional{record + 1};
    };
    selection.run(8192, hooks, [&](auto r) { f.append(r); });
    check(f.selected.size() == 8192 && !selection.poisoned(), "exact 8192 record and append boundary succeeds");
  }
  for (unsigned mode = 0; mode != 3; ++mode) {
    Fixture f; std::array registry{&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    hooks.related_resources = [](auto, auto output) { output[0] = 200; return std::size_t{1}; };
    hooks.resource_owner = [mode](auto resource) -> PicturePreselectionHooks::Optional {
      if (resource == 200) return 300;
      return mode == 1 ? std::nullopt : PicturePreselectionHooks::Optional{301};
    };
    hooks.owner_capabilities = [](auto owner) { return owner == 300 ? 0x100000U : 0U; };
    hooks.current_resource = [mode](auto) -> PicturePreselectionHooks::Optional {
      return mode == 0 ? std::nullopt : PicturePreselectionHooks::Optional{400};
    };
    hooks.resource_registry_identifier = [](auto) { return 500U; };
    unsigned enumerations = 0, context_queries = 0;
    hooks.next_resource = [&](auto, auto) -> PicturePreselectionHooks::Optional { ++enumerations; return {}; };
    hooks.state_context = [&](auto) -> PictureSelectionContext* { ++context_queries; return nullptr; };
    selection.run(8, hooks, [&](auto r) { f.append(r); });
    check(enumerations == (mode != 0) && context_queries == 0 && f.selected.size() == 1,
      "null initial resource skips enumeration; missing or incapable live owner skips context lookup");
  }
  {
    Fixture f; std::array registry{&f.context}; PicturePreselection selection(registry);
    auto hooks = f.hooks();
    rejects([&] { selection.run(0, hooks, [](auto) {}); }, "zero chain bound preflight rejects");
    hooks.predicate = {};
    rejects([&] { selection.run(8, hooks, [](auto) {}); }, "missing service preflight rejects");
    check(!selection.poisoned() && f.trace.empty(), "preflight failures have no effects");
    hooks = f.hooks();
    hooks.relative_point = [&](const auto&, auto) {
      selection.run(8, f.hooks(), [](auto) {}); return PicturePreselectionHooks::Point{};
    };
    rejects([&] { selection.run(8, hooks, [](auto) {}); }, "selection reentry rejects");
    check(selection.poisoned(), "propagated callback failure poisons entered selection");
  }
  {
    Fixture f; PicturePreselection selection({});
    selection.run(1, f.hooks(), [&](auto r) { f.append(r); });
    check(f.trace.empty() && !selection.poisoned(), "explicit empty registry produces no callbacks, not a lifecycle inference");
  }
  {
    Fixture f; PicturePreselection selection({});
    selection.run(8, f.hooks(), [](auto) {});
    selection.register_context(f.context); selection.register_context(f.context);
    auto hooks = f.hooks();
    hooks.relative_point = [&](const auto&, auto) {
      rejects([&] { selection.register_context(f.context); }, "callback insertion rejects before mutation");
      return PicturePreselectionHooks::Point{1,2,3};
    };
    selection.run(8, hooks, [&](auto r) { f.append(r); });
    check(selection.registry_size() == 2 && f.selected == std::vector<std::uint64_t>{100,100},
      "between-frame registration preserves duplicates, caught insertion rejection has no effects");
    hooks.relative_point = [](const auto&, auto) -> PicturePreselectionHooks::Point {
      throw std::runtime_error("producer failure");
    };
    rejects([&] { selection.run(8, hooks, [](auto) {}); }, "entered callback failure poisons registry");
    rejects([&] { selection.register_context(f.context); }, "poisoned registry rejects registration");
    check(selection.registry_size() == 2, "rejected registration never changes membership");
  }
  return failures ? 1 : 0;
}
