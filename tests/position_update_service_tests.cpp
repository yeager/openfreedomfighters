#include "off/graphics/position_update_service.hpp"
#include "off/graphics/center_picture_position.hpp"

#include <array>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using namespace off::graphics;
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class F> void rejects(F operation) {
    bool caught = false;
    try { operation(); } catch (const std::runtime_error&) { caught = true; }
    check(caught, "unsupported operation rejects");
}
constexpr PositionServiceMode deferred{false, true, 0};
struct Fixture {
    PositionUpdateService service;
    std::array<PositionResource, 51> resources{};
    std::vector<std::uint64_t> made, resolved, bounded, maintained, batch;
    int batches = 0;
    PositionServiceHooks hooks;
    Fixture() {
        for (std::size_t i = 0; i < resources.size(); ++i) resources[i] = {i, 0};
        hooks.make_handle = [&](PositionResource& r) {
            check((r.flags & 0x23000000U) == 0x23000000U, "flags set before handle creation");
            made.push_back(r.identity); return r.identity;
        };
        hooks.resolve = [&](std::uint64_t handle) -> PositionResource* {
            resolved.push_back(handle);
            return handle < resources.size() ? &resources[static_cast<std::size_t>(handle)] : nullptr;
        };
        hooks.bounds = [&](PositionResource& r) { bounded.push_back(r.identity); };
        hooks.maintenance = [&](PositionResource& r) { maintained.push_back(r.identity); };
        hooks.final_batch = [&](std::span<PositionResource* const> survivors) {
            ++batches;
            check(service.pending_count() == survivors.size(), "count committed before batch callback");
            batch.clear();
            for (auto* r : survivors) batch.push_back(r->identity);
        };
    }
};
}

int main() {
    {
        Fixture f;
        f.service.notify(f.resources[0],deferred,f.hooks);
        const auto flags=f.resources[0].flags;
        for(const auto suppression:{0,1,-1,std::numeric_limits<std::int32_t>::min()})
            f.service.notify_with_collection_disabled({false,false,suppression});
        check(f.service.pending_count()==1 && f.resources[0].flags==flags && f.made.size()==1 &&
              f.resolved.empty() && f.bounded.empty() && f.maintained.empty() && f.batches==0,
              "disabled notification preserves retained queue and ignores suppression without resource callbacks");
        rejects([&]{f.service.notify_with_collection_disabled({true,false,0});});
        rejects([&]{f.service.notify_with_collection_disabled({false,true,0});});
        check(!f.service.failed() && f.service.pending_count()==1,
              "non-disabled modes reject before side effects rather than fabricate early return");
        f.service.flush(deferred,f.hooks);
        check(f.resolved==std::vector<std::uint64_t>{0} && f.service.pending_count()==0,
              "disabled notifications do not replace or invalidate prior queued handles");
    }
    static_assert(!std::is_copy_constructible_v<PositionUpdateService> &&
                  !std::is_move_constructible_v<PositionUpdateService> &&
                  !std::is_copy_assignable_v<PositionUpdateService> &&
                  !std::is_move_assignable_v<PositionUpdateService>);
    {
        std::array<PositionResource, 3> resources{{{17, 0x100000U}, {23, 0xC00U}, {0, 0}}};
        std::array<PositionResource*, 3> batch{&resources[0], &resources[1], &resources[2]};
        std::vector<std::uint64_t> queried;
        const auto owner_class = [&](const PositionResource& resource) -> std::optional<std::uint32_t> {
            queried.push_back(resource.identity); return 0x00200046U;
        };
        complete_plain_picture_position_batch(batch, owner_class);
        check(queried == std::vector<std::uint64_t>{17, 23, 0} &&
              resources[0].flags == 0x100000U && resources[1].flags == 0xC00U && resources[2].flags == 0,
              "plain batch validates all owner metadata without interpreting runtime flags as class bits");
        const std::array<PositionResource*, 0> empty{};
        queried.clear();
        complete_plain_picture_position_batch(empty, owner_class);
        check(queried.empty(), "empty concrete batch has no owner queries or scene effects");
        rejects([&] { complete_plain_picture_position_batch(empty, {}); });
        auto invalid = batch; invalid[2] = nullptr;
        rejects([&] { complete_plain_picture_position_batch(invalid, owner_class); });
        check(queried.empty(), "validate every pointer before invoking any lookup");
        rejects([&] { complete_plain_picture_position_batch(batch,
            [](const PositionResource&) -> std::optional<std::uint32_t> { return std::nullopt; }); });
        rejects([&] { complete_plain_picture_position_batch(batch,
            [](const PositionResource& r) -> std::optional<std::uint32_t> {
                return r.identity == 23 ? 0x00100030U : 0x00200046U;
            }); });
        rejects([&] { complete_plain_picture_position_batch(batch,
            [](const PositionResource&) -> std::optional<std::uint32_t> { return 0x00200047U; }); });
        check(resources[0].flags == 0x100000U && resources[1].flags == 0xC00U,
              "unknown or mixed owner family cannot silently use the no-effect handler");
        for (bool mixed : {false, true}) {
            Fixture f;
            f.service.notify(f.resources[0], deferred, f.hooks);
            f.service.notify(f.resources[1], deferred, f.hooks);
            f.hooks.final_batch = [&](std::span<PositionResource* const> survivors) {
                check(f.service.pending_count() == 2, "concrete final handler enters before count clear");
                complete_plain_picture_position_batch(survivors,
                    [&](const PositionResource& r) -> std::optional<std::uint32_t> {
                        check(f.service.pending_count() == 2, "owner validation retains survivor count");
                        return mixed && r.identity == 1 ? 0x00100030U : 0x00200046U;
                    });
                check(f.service.pending_count() == 2, "concrete final handler returns before count clear");
            };
            if (mixed) rejects([&] { f.service.flush(deferred, f.hooks); });
            else f.service.flush(deferred, f.hooks);
            check(f.service.failed() == mixed && f.service.pending_count() == (mixed ? 2U : 0U) &&
                  f.bounded == std::vector<std::uint64_t>{0, 1},
                  "mixed final batch poisons without rolling back preceding bounds or clearing count");
        }
    }
    {
        Fixture f;
        f.service.notify(f.resources[0], {false, false, 0}, {});
        for (auto gate : {0x20000000U, 0x200000U}) {
            f.resources[0].flags = gate;
            f.service.notify(f.resources[0], deferred, {});
            check(f.resources[0].flags == gate, "gate preserves flags without required hooks");
        }
        f.resources[0].flags = 0x100000U;
        f.service.notify(f.resources[0], deferred, f.hooks);
        f.service.notify(f.resources[0], deferred, {});
        check(f.service.pending_count() == 1 && f.made == std::vector<std::uint64_t>{0},
              "zero handle is enqueued and queue bit deduplicates notifications");
        f.service.flush(deferred, f.hooks);
        check(f.resolved == std::vector<std::uint64_t>{0} && f.batch == f.resolved &&
              f.bounded == f.resolved && f.resources[0].flags == 0x03100000U &&
              f.service.pending_count() == 0, "clear only queue bit and retain other masks");
        f.service.flush(deferred, f.hooks);
        check(f.batches == 2 && f.batch.empty(), "enabled empty flush still calls final batch");
    }
    {
        Fixture f;
        for (std::size_t i = 0; i < 4; ++i) f.service.notify(f.resources[i], deferred, f.hooks);
        f.resources[1].flags |= 0x200000U;
        f.resources[2].flags |= 0x40000U;
        f.hooks.bounds = [&](PositionResource& r) {
            f.bounded.push_back(r.identity);
            f.resources[3].flags |= 0x40000U;
        };
        f.service.flush(deferred, f.hooks);
        check(f.bounded == std::vector<std::uint64_t>{0} &&
              f.maintained == std::vector<std::uint64_t>{2, 3} &&
              f.batch == std::vector<std::uint64_t>{0, 1, 2, 3},
              "later predicates read current flags; batch includes skipped survivors");
        for (auto suppression : {-1, 1}) {
            Fixture g;
            g.service.notify(g.resources[0], deferred, g.hooks);
            g.resources[1].flags = 0x40000U;
            g.service.notify(g.resources[1], deferred, g.hooks);
            g.service.flush({false, true, suppression}, g.hooks);
            check(g.bounded.size() == (suppression < 0 ? 1U : 0U) && g.maintained.size() == 1,
                  "signed suppression gates only ordinary bounds, not maintenance");
        }
    }
    {
        Fixture f;
        for (std::size_t i = 0; i < 3; ++i) f.service.notify(f.resources[i], deferred, f.hooks);
        f.hooks.resolve = [&](std::uint64_t handle) -> PositionResource* {
            check(f.service.pending_count() == 3, "first pass retains original count");
            f.resolved.push_back(handle);
            if (handle == 1) {
                check((f.resources[0].flags & 0x20000000U) == 0, "previous survivor cleared before next resolution");
                return nullptr;
            }
            return &f.resources[0]; // Two handles may resolve to one live identity.
        };
        f.service.flush(deferred, f.hooks);
        check(f.resolved == std::vector<std::uint64_t>{0, 1, 2} &&
              f.batch == std::vector<std::uint64_t>{0, 0} && f.bounded == f.batch,
              "resolve once, compact missing entries, preserve duplicate live identities");
        Fixture disabled;
        disabled.service.notify(disabled.resources[0], deferred, disabled.hooks);
        PositionServiceHooks only_resolve;
        only_resolve.resolve = disabled.hooks.resolve;
        disabled.service.flush({false, false, 0}, only_resolve);
        check(disabled.resources[0].flags == 0x03000000U && disabled.service.pending_count() == 0 &&
              disabled.batches == 0, "disabled flush still resolves and clears before dropping queue");
        Fixture missing;
        missing.service.notify(missing.resources[0], deferred, missing.hooks);
        missing.hooks.resolve = [](std::uint64_t) -> PositionResource* { return nullptr; };
        missing.service.flush(deferred, missing.hooks);
        check(missing.batches == 1 && missing.batch.empty(), "all missing still invokes empty final batch");
    }
    {
        Fixture f;
        for (std::size_t i = 0; i < 50; ++i) f.service.notify(f.resources[i], deferred, f.hooks);
        check(f.service.pending_count() == 50 && f.batches == 0, "capacity is exactly fifty");
        auto incomplete = f.hooks; incomplete.final_batch = {};
        rejects([&] { f.service.notify(f.resources[50], deferred, incomplete); });
        check(!f.service.failed() && f.service.pending_count() == 50 && f.resources[50].flags == 0,
              "capacity flush validation occurs before effects");
        f.hooks.final_batch = [&](std::span<PositionResource* const> survivors) {
            check(survivors.size() == 50 && f.service.pending_count() == 50,
                  "capacity batch sees complete prior queue");
            f.resources[50].flags |= 0x200000U;
            ++f.batches;
        };
        f.service.notify(f.resources[50], deferred, f.hooks);
        check(f.service.pending_count() == 1 && f.made.size() == 51 && f.batches == 1 &&
              f.resources[50].flags == 0x23200000U, "enqueue after capacity flush does not repeat flag gate");
    }
    {
        Fixture f;
        f.service.notify(f.resources[0], deferred, f.hooks);
        std::vector<int> order;
        PositionServiceHooks immediate;
        immediate.spatial_change = [&](PositionResource&) { order.push_back(1); };
        immediate.bounds = [&](PositionResource&) { order.push_back(2); };
        f.service.notify(f.resources[0], {true, false, 99}, immediate);
        check(order == std::vector<int>{1, 2} && f.service.pending_count() == 1 && f.resolved.empty(),
              "immediate mode bypasses collection, suppression and queue gates without flushing");
        immediate.spatial_change = {};
        f.service.notify(f.resources[0], {true, true, 0}, immediate);
        check(order == std::vector<int>{1, 2, 2}, "spatial service is optional");
        rejects([&] { f.service.notify(f.resources[0], {true, true, 0}, {}); });
        rejects([&] { f.service.flush(deferred, {}); });
        check(!f.service.failed(), "missing hooks do not poison service");
    }
    {
        Fixture f;
        f.hooks.make_handle = [&](PositionResource&) -> std::uint64_t { throw std::runtime_error("handle"); };
        rejects([&] { f.service.notify(f.resources[0], deferred, f.hooks); });
        check(f.service.failed() && f.service.pending_count() == 0 && f.resources[0].flags == 0x23000000U,
              "handle exception retains flags without append and poisons");
        rejects([&] { f.service.notify(f.resources[1], {false, false, 0}, {}); });
        rejects([&] { f.service.flush({false, false, 0}, f.hooks); });
        Fixture g;
        g.service.notify(g.resources[0], deferred, g.hooks);
        g.hooks.final_batch = [](std::span<PositionResource* const>) { throw std::runtime_error("batch"); };
        rejects([&] { g.service.flush(deferred, g.hooks); });
        check(g.service.failed() && g.service.pending_count() == 1 &&
              g.resources[0].flags == 0x03000000U && g.bounded.size() == 1,
              "final batch exception retains survivor count and completed effects");
        Fixture h;
        h.hooks.make_handle = [&](PositionResource& r) {
            rejects([&] { h.service.flush(deferred, h.hooks); });
            rejects([&] { h.service.notify(r, {false, false, 0}, {}); });
            return r.identity;
        };
        h.service.notify(h.resources[0], deferred, h.hooks);
        check(!h.service.failed() && h.service.pending_count() == 1,
              "caught reentry rejects even no-op branch without poisoning successful outer call");
    }
    for (int stage = 0; stage < 4; ++stage) {
        Fixture f;
        const auto fail = [](PositionResource&) { throw std::runtime_error("operation"); };
        if (stage == 0) {
            f.hooks.spatial_change = fail;
            rejects([&] { f.service.notify(f.resources[0], {true, false, 0}, f.hooks); });
            check(f.bounded.empty() && f.service.pending_count() == 0,
                  "spatial exception prevents immediate bounds and queue effects");
        } else {
            f.service.notify(f.resources[0], deferred, f.hooks);
            if (stage == 1)
                f.hooks.resolve = [](std::uint64_t) -> PositionResource* { throw std::runtime_error("resolve"); };
            else if (stage == 2) f.hooks.bounds = fail;
            else { f.resources[0].flags |= 0x40000U; f.hooks.maintenance = fail; }
            rejects([&] { f.service.flush(deferred, f.hooks); });
            check(f.service.pending_count() == 1 && f.batches == 0 &&
                  ((f.resources[0].flags & 0x20000000U) != 0U) == (stage == 1),
                  "failed resolve or second-pass hook retains precisely completed clear prefix");
        }
        check(f.service.failed(), "every unexpected synchronous hook failure poisons service");
    }
    {
        const int saved = std::fegetround();
        check(std::fesetround(FE_TONEAREST) == 0, "nearest rounding available");
        Fixture f;
        CenterPicturePosition center;
        PictureSubmissionCache cache;
        std::array<float, 3> position{7, 8, 9};
        std::uint32_t status = 0;
        center.initialize(position, f.resources[0].flags, status, 101, 81, cache,
                          [&] { f.service.notify(f.resources[0], deferred, f.hooks); });
        check(f.resources[0].flags == 0x23100000U && f.service.pending_count() == 1 && status == 1,
              "Center and shared service update the same live flag word");
        f.service.flush(deferred, f.hooks);
        check(f.resources[0].flags == 0x03100000U && f.bounded == std::vector<std::uint64_t>{0},
              "deferred Center notification reaches explicit bounds hook on flush");
        check(std::fesetround(saved) == 0, "restore caller rounding");
    }
    return failures == 0 ? 0 : 1;
}
