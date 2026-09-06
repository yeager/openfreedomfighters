#include "off/runtime/component_lifecycle.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace off::runtime;
void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F&& operation) {
  bool caught = false;
  try { operation(); } catch (const std::runtime_error&) { caught = true; }
  check(caught, "expected lifecycle rejection");
}
ComponentSource source(std::uint64_t owner, std::size_t attachment = 0) {
  return {owner, static_cast<std::size_t>(owner), attachment,
          "Synthetic_Component", 12, 24, 1.0F, false};
}
ConstructedComponent instance(std::uint64_t owner, std::uint32_t mask = 3) {
  return {{314, 0, mask, 0, 0, 0x20, 0, owner}, {}, {}};
}
struct Fixture {
  SceneComponentSequence sequence;
  ComponentLifecycle lifecycle{sequence};
  std::map<std::uint64_t, std::uint32_t> flags;
  std::vector<std::string> log;
  ComponentLifecycleServices services{
    [](bool, ComponentRecord&, std::size_t) {},
    [this](std::uint64_t owner) -> std::optional<std::uint32_t> {
      log.push_back("flags:" + std::to_string(owner));
      const auto found = flags.find(owner);
      return found == flags.end() ? std::nullopt : std::optional(found->second);
    },
    [this](std::uint64_t owner) { log.push_back("post:" + std::to_string(owner)); },
    [this](ComponentRecord& record) {
      log.push_back("retire:" + std::to_string(record.source().owner));
      record.state().attached_owner = 0;
    }
  };
  std::size_t add(std::uint64_t owner, std::uint32_t mask = 3) {
    flags[owner] = 0;
    const auto index = lifecycle.append(source(owner));
    lifecycle.construct(index, [this, owner, mask](ComponentRecord&) {
      auto value = instance(owner, mask);
      value.phase_one = [this, owner](ComponentRecord&) {
        log.push_back("one:" + std::to_string(owner));
      };
      value.phase_two = [this, owner](ComponentRecord&) {
        log.push_back("two:" + std::to_string(owner));
      };
      return value;
    });
    return index;
  }
};
}

int main() {
  try {
    {
      Fixture f;
      const auto index = f.lifecycle.append(source(1));
      check(!f.lifecycle.at(index).constructed() && !f.lifecycle.at(index).identity() &&
            f.sequence.next_identity() == 0 && f.lifecycle.construction_order().empty(),
            "catalog is not construction, identity or admission");
      rejects([&] { (void)f.lifecycle.at(index).state(); });
      rejects([&] { f.lifecycle.run_global_phases(f.services); });
      check(!f.lifecycle.failed() && f.log.empty(), "unconstructed catalog rejects before callbacks");
      rejects([&] { f.lifecycle.construct(index, {}); });
      f.lifecycle.construct(index, [&](ComponentRecord& record) {
        check(record.identity() == 0 && f.sequence.next_identity() == 1 &&
              f.lifecycle.construction_order().size() == 1 && !record.constructed(),
              "base identity and registration precede concrete factory");
        rejects([&] { f.lifecycle.append(source(2)); });
        rejects([&] { f.lifecycle.run_global_phases(f.services); });
        return instance(1, 0);
      });
      rejects([&] { f.lifecycle.construct(index, [](auto&) { return instance(1); }); });
      ComponentLifecycle next(f.sequence);
      const auto next_index = next.append(source(5));
      next.construct(next_index, [](auto&) { return instance(5, 0); });
      check(next.at(next_index).identity() == 1 && f.sequence.next_identity() == 2,
            "another registry shares scene-manager serial without resetting it");
      rejects([&] { next.run_global_phases(f.services); });
      check(!next.failed(), "foreign live registry prevents partial global pass before effects");
    }
    {
      Fixture f;
      const auto a = f.lifecycle.append(source(1));
      const auto b = f.lifecycle.append(source(2));
      const auto c = f.lifecycle.append(source(3));
      f.flags = {{1, 0}, {2, 0}, {3, 0}};
      for (const auto index : {c, a, b}) {
        f.lifecycle.construct(index, [&](ComponentRecord& record) {
          const auto owner = record.source().owner;
          auto value = instance(owner);
          value.phase_one = [&, owner](auto&) { f.log.push_back("one:" + std::to_string(owner)); };
          value.phase_two = [&, owner](auto&) { f.log.push_back("two:" + std::to_string(owner)); };
          return value;
        });
      }
      f.lifecycle.run_global_phases(f.services);
      const std::vector<std::string> expected{
        "flags:2", "one:2", "post:2", "flags:1", "one:1", "post:1",
        "flags:3", "one:3", "post:3", "flags:2", "two:2", "flags:1", "two:1", "flags:3", "two:3"};
      check(f.log == expected && f.lifecycle.phases_completed(), "both complete reverse construction passes, not catalog order");
      for (auto index : {a, b, c}) check(f.lifecycle.at(index).state().status == 0x2c, "phase status committed after callbacks");
      f.log.clear();
      f.lifecycle.run_global_phases(f.services);
      check(f.log == expected, "global phase repetition is not gated by completion status bits");
    }
    {
      Fixture f;
      const auto hidden = f.add(1);
      const auto bypass = f.add(2, 0x203);
      f.add(3, 0);
      const auto absent = f.add(4);
      const auto detached = f.add(5);
      f.flags[1] = f.flags[2] = 0x400;
      f.flags.erase(4);
      f.lifecycle.at(detached).state().attached_owner = 0;
      f.lifecycle.run_global_phases(f.services);
      check(f.log == std::vector<std::string>{"flags:4", "flags:2", "one:2", "post:2", "flags:1", "post:1",
            "flags:4", "flags:2", "two:2", "flags:1"}, "hidden owner still notified; bypass, mask, missing and detached gates");
      check(f.lifecycle.at(hidden).state().status == 0x20 && f.lifecycle.at(absent).state().status == 0x20 &&
            f.lifecycle.at(bypass).state().status == 0x2c, "skipped components receive no phase completion bits");
    }
    {
      Fixture f;
      f.add(1);
      const auto newer = f.lifecycle.append(source(2)); f.flags[2] = 0;
      f.lifecycle.construct(newer, [&](auto&) {
        auto value = instance(2);
        value.phase_one = [&](auto&) { f.log.push_back("hide older"); f.flags[1] = 0x400; };
        value.phase_two = [&](auto&) { f.log.push_back("unhide older"); f.flags[1] = 0; };
        return value;
      });
      f.lifecycle.run_global_phases(f.services);
      check(f.lifecycle.at(0).state().status == 0x28 &&
            f.log == std::vector<std::string>{"flags:2", "hide older", "post:2", "flags:1", "post:1",
              "flags:2", "unhide older", "flags:1", "two:1"}, "each phase reads live hide state; phase two has no invented phase-one gate");
    }
    {
      Fixture f;
      const auto before = f.add(1);
      f.lifecycle.at(before).state().status |= 1;
      const auto after = f.lifecycle.append(source(2)); f.flags[2] = 0;
      f.lifecycle.construct(after, [&](auto&) {
        auto value = instance(2);
        value.phase_one = [&](ComponentRecord& record) { f.log.push_back("one:2"); record.state().status |= 1; };
        return value;
      });
      f.lifecycle.run_global_phases(f.services);
      check(f.log == std::vector<std::string>{"flags:2", "one:2", "retire:2", "post:2", "flags:1", "retire:1", "post:1"},
            "retirement before/after dispatch preserves saved-owner notification order");
      check(f.lifecycle.at(after).state().status == 0x25 && f.lifecycle.at(before).state().status == 0x21,
            "only returned phase callback receives completion bit before retirement");
      check(f.lifecycle.at(after).removed() && f.lifecycle.at(before).removed() && f.sequence.live_count() == 0,
            "completed concrete retirement removes both instances from live registry");
      f.log.clear(); f.lifecycle.run_global_phases(f.services);
      check(f.log.empty(), "removed entries never receive later phase callbacks or notifications");
    }
    {
      Fixture f;
      const auto index = f.lifecycle.append(source(1));
      rejects([&] { f.lifecycle.construct(index, [](auto&) -> ConstructedComponent { throw std::runtime_error("factory failed"); }); });
      check(f.lifecycle.failed() && !f.lifecycle.at(index).constructed() && f.lifecycle.at(index).identity() == 0 &&
            f.sequence.next_identity() == 1 && f.lifecycle.construction_order().size() == 1, "factory failure preserves base-registration prefix");
      rejects([&] { f.lifecycle.construct(index, [](auto&) { return instance(1); }); });
    }
    {
      Fixture f;
      const auto unsupported = f.lifecycle.append(source(1)); f.flags[1] = 0;
      f.lifecycle.construct(unsupported, [](auto&) { return instance(1); });
      const auto newer = f.add(2);
      rejects([&] { f.lifecycle.run_global_phases(f.services); });
      check(f.lifecycle.failed() && !f.lifecycle.phases_completed() && f.lifecycle.at(newer).state().status == 0x24 &&
            f.lifecycle.at(unsupported).state().status == 0x20, "missing reached callback preserves earlier completed prefix");
      const auto log = f.log;
      rejects([&] { f.lifecycle.run_global_phases(f.services); });
      check(f.log == log, "failed pass cannot retry callbacks");
    }
    {
      Fixture f; f.add(1);
      auto missing = f.services; missing.retire = {};
      rejects([&] { f.lifecycle.run_global_phases(missing); });
      check(!f.lifecycle.failed() && f.log.empty(), "missing lifecycle service preflight is effect-free");
      f.services.post_phase_one = [&](auto) {
        rejects([&] { f.lifecycle.run_global_phases(f.services); });
        rejects([&] { f.lifecycle.construct(0, [](auto&) { return instance(1); }); });
        rejects([&] { f.lifecycle.append(source(2)); });
      };
      f.lifecycle.run_global_phases(f.services);
      check(f.lifecycle.phases_completed(), "caught forbidden reentry leaves admitted outer pass intact");
    }
    {
      SceneComponentSequence sequence;
      sequence.set_construction_mode(true);
      {
        ComponentLifecycle first(sequence);
        const auto index = first.append(source(1));
        first.construct(index, [&](ComponentRecord& record) {
          const auto base = record.state();
          check(base.status == 0x10 && base.requested == 0 && base.admitted == 0 &&
                base.registered_cache == 0 && base.script_reference == 0 && base.attached_owner == 0 &&
                sequence.live_count() == 1, "factory observes registered zero base with distinct construction-mode status bit");
          auto value = instance(1, 0); value.state.status |= base.status; return value;
        });
        check(first.at(index).state().status == 0x30 && first.at(index).state().admitted == 0,
              "concrete factory preserves construction status without fabricating ordinary admission");
      }
      check(sequence.live_count() == 0 && sequence.next_identity() == 1, "native registry teardown decrements live count, not serial");
      ComponentLifecycle next(sequence);
      const auto index = next.append(source(2));
      next.construct(index, [](auto&) { return instance(2, 0); });
      check(next.at(index).identity() == 1, "serial survives prior registry lifetime");
    }
    {
      Fixture f;
      f.add(1, 3); f.add(2, 0); f.add(3, 3);
      std::vector<std::string> progress;
      f.services.progress = [&](bool second, ComponentRecord& record, std::size_t visited) {
        progress.push_back(std::to_string(second) + ":" + std::to_string(record.source().owner) + ":" + std::to_string(visited));
        if (!second && record.source().owner == 3) {
          // The already-tested phase bit must not be retested. Owner and hide
          // bypass must be read only after this callback returns.
          record.state().requested = 0x200;
          record.state().attached_owner = 7; f.flags[7] = 0x400;
        }
      };
      f.lifecycle.run_global_phases(f.services);
      check(progress == std::vector<std::string>{"0:3:1", "0:1:3", "1:1:6"},
            "progress counts all surviving visited nodes across phases, including absent phase bits");
      check(f.log == std::vector<std::string>{"flags:7", "one:3", "post:7", "flags:1", "one:1", "post:1", "flags:1", "two:1"},
            "captured phase bit and post-progress live owner/bypass drive dispatch");
    }
    {
      Fixture f;
      auto payload = std::make_shared<int>(42);
      std::weak_ptr<int> weak = payload;
      const auto index = f.lifecycle.append(source(1)); f.flags[1] = 0;
      f.lifecycle.construct(index, [payload](auto&) {
        auto value = instance(1);
        value.phase_one = [payload](ComponentRecord& record) { record.state().status |= 1; };
        value.phase_two = [payload](auto&) {};
        return value;
      });
      payload.reset();
      f.services.retire = [&](ComponentRecord& record) {
        check(!record.removed() && !weak.expired() && f.sequence.live_count() == 1,
              "concrete retirement owns live payload before native removal");
        record.state().attached_owner = 0;
      };
      f.services.post_phase_one = [&](auto owner) {
        check(owner == 1 && weak.expired() && f.lifecycle.at(index).removed(),
              "callback captures release after concrete retirement, before saved-owner notification");
      };
      f.lifecycle.run_global_phases(f.services);
    }
    {
      Fixture f; const auto index = f.add(1); f.lifecycle.at(index).state().status |= 1;
      f.services.retire = [](ComponentRecord& record) { record.state().priority = 99; throw std::runtime_error("retirement failed"); };
      rejects([&] { f.lifecycle.run_global_phases(f.services); });
      check(f.lifecycle.failed() && !f.lifecycle.at(index).removed() && f.sequence.live_count() == 1 &&
            f.lifecycle.at(index).state().priority == 99, "failed concrete retirement retains live metadata and completed prefix");
    }
    {
      Fixture f;
      const auto index = f.lifecycle.append(source(1)); f.flags[1] = 0;
      f.lifecycle.construct(index, [&](auto&) {
        auto value = instance(1);
        value.phase_one = [](auto&) {};
        value.phase_two = [](ComponentRecord& record) { record.state().priority = 77; throw std::runtime_error("phase two failed"); };
        return value;
      });
      rejects([&] { f.lifecycle.run_global_phases(f.services); });
      check(f.lifecycle.at(index).state().status == 0x24 && f.lifecycle.at(index).state().priority == 77 &&
            f.lifecycle.failed(), "throwing callback retains mutation without granting its completion bit");
    }
    {
      Fixture f;
      ComponentLifecycle foreign(f.sequence);
      const auto foreign_index=foreign.append(source(2));
      const auto index=f.lifecycle.append(source(1)); f.flags[1]=0;
      f.lifecycle.construct(index,[&](auto&) {
        rejects([&] { foreign.construct(foreign_index,[](auto&) { return instance(2); }); });
        auto value=instance(1,1);
        value.phase_one=[&](auto&) {
          rejects([&] { foreign.construct(foreign_index,[](auto&) { return instance(2); }); });
          rejects([&] { foreign.run_global_phases(f.services); });
          check(f.sequence.live_count()==1 && f.sequence.next_identity()==1 &&
                !foreign.at(foreign_index).identity(),
                "cross-registry reentry is rejected before base registration");
        };
        return value;
      });
      f.lifecycle.run_global_phases(f.services);
      check(f.lifecycle.phases_completed(),"caught cross-registry mutation leaves outer pass valid");
      foreign.construct(foreign_index,[](auto&) { return instance(2,0); });
      check(f.sequence.next_identity()==2,"scene-wide guard releases after complete pass");
    }
    std::cout << "Retained component identity, reverse global lifecycle, live admission and failure prefixes verified.\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
