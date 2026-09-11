#include "off/graphics/intro_renderer_resource_relations.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
using Relations = off::graphics::IntroRendererResourceRelations;
using Bytes = std::vector<std::byte>;
using Members = std::vector<std::uint64_t>;

void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template<class Function> void rejects(Function function, const char* message) {
  try { function(); }
  catch (const std::runtime_error&) { return; }
  throw std::runtime_error(message);
}
void append_word(Bytes& bytes, std::uint32_t word) {
  for (unsigned shift = 0; shift < 32U; shift += 8U)
    bytes.push_back(static_cast<std::byte>((word >> shift) & 0xffU));
}

// Independent source-format fixtures: a four-word framing header, zero-head
// tagged groups, and optional opaque workspace words. No retail bytes are used.
Bytes payload(std::initializer_list<std::uint32_t> groups,
              std::uint32_t eight_words = 0U, std::uint32_t sixteen_words = 0U) {
  Bytes bytes;
  append_word(bytes, 4U + static_cast<std::uint32_t>(groups.size()));
  append_word(bytes, sixteen_words);
  append_word(bytes, eight_words);
  append_word(bytes, 0x12345678U);
  for (const auto word : groups) append_word(bytes, word);
  for (std::uint32_t word = 0; word < eight_words + sixteen_words; ++word)
    append_word(bytes, 0xa0U + word);
  return bytes;
}
constexpr auto live = [](std::uint64_t resource) { return resource != 0U; };

void check_unpublished(const Relations& relations) {
  check(!relations.loaded() && relations.group_count() == 0U &&
            relations.member_count() == 0U && !relations.has_selector(0U) &&
            !relations.has_selector(4U),
        "failed relation loading must not publish any selector, group or member");
  rejects([&] { static_cast<void>(relations.members(4U, live)); },
          "failed relation loading must not expose a partial group");
}

void test_owned_storage_and_queries() {
  auto relations = [] {
    auto input = payload({0U, 0x100U, 0x180U, 0x101U, 0U, 1U, 0U, 0x205U}, 2U, 5U);
    Relations result(input);
    std::ranges::fill(input, std::byte{0xee});
    return result;
  }();
  const auto expected_bytes = payload({0U, 0x100U, 0x180U, 0x101U, 0U, 1U, 0U, 0x205U}, 2U, 5U);
  check(std::ranges::equal(relations.workspace().bytes(), expected_bytes) &&
            relations.workspace().eight_byte_slot(0U).front() == std::byte{0xa0} &&
            relations.workspace().sixteen_byte_slot(0U).front() == std::byte{0xa2} &&
            relations.workspace().sixteen_byte_trailing_bytes().size() == 4U,
        "relation container owns both serialized groups and opaque workspace after input destruction");
  check_unpublished(relations);
  const std::map<std::uint32_t, std::uint64_t> resources{
      {0x40000160U, 0x100000007ULL}, {0x400001e0U, 42U}, {0x40000260U, 3U}};
  std::vector<std::uint32_t> lookups;
  const Relations::Resolve resolve = [&](std::uint32_t source) -> std::optional<std::uint64_t> {
    lookups.push_back(source);
    const auto found = resources.find(source);
    return found == resources.end() ? std::nullopt : std::optional{found->second};
  };
  relations.read(resolve);
  check(lookups == std::vector<std::uint32_t>{0x40000160U, 0x400001e0U, 0x40000160U, 0x40000260U} &&
            relations.loaded() && relations.group_count() == 3U && relations.member_count() == 4U,
        "source references preserve order, duplicates and source-domain lookups without resolving null");
  check(relations.has_selector(0U) && relations.has_selector(4U) &&
            relations.has_selector(8U) && relations.has_selector(10U),
        "selectors are exact group-head word offsets plus the distinct no-relation selector");
  for (const auto invalid : {1U, 3U, 5U, 7U, 9U, 11U, 12U, 16U, 32U,
                             std::numeric_limits<std::uint32_t>::max()}) {
    check(!relations.has_selector(invalid), "header, member, byte-offset and outside selectors are not group heads");
    rejects([&] { static_cast<void>(relations.members(invalid, live)); },
            "invalid relation selectors must reject rather than select a neighboring group");
  }
  unsigned no_member_calls{};
  const auto no_relation = relations.members(0U, [&](auto) { ++no_member_calls; return true; });
  const auto empty_group = relations.members(8U, [&](auto) { ++no_member_calls; return true; });
  check(!no_relation && empty_group && empty_group->empty() && no_member_calls == 0U,
        "selector zero is absent while a real sole-null group is present and empty");
  const Members expected{0x100000007ULL, 42U, 0x100000007ULL};
  auto first_alias = relations.members(4U, live);
  const auto second_alias = relations.members(4U, live);
  check(first_alias && *first_alias == expected && second_alias == first_alias,
        "resources using the same selector observe the same ordered members");
  first_alias->front() = 999U;
  check(second_alias && *second_alias == expected && relations.members(4U, live) == second_alias,
        "query results own independent copies without mutating aliased relation storage");
  check(relations.members(10U, live) == std::optional<Members>{{3U}},
        "tag bit two is inert and a nonzero source can resolve to the first native resource identity");
  rejects([&] { relations.read(resolve); }, "successful initial load cannot be repeated");
  rejects([&] { static_cast<void>(relations.members(4U, {})); }, "query requires a live-resource service");
  rejects([&] { static_cast<void>(relations.members(0U, {})); }, "even an absent selector requires query services");
  std::vector<std::uint64_t> checked;
  rejects([&] {
    static_cast<void>(relations.members(4U, [&](auto resource) {
      checked.push_back(resource); return resource != 42U;
    }));
  }, "stale member rejects the whole query instead of returning a filtered list");
  check(checked == Members{0x100000007ULL, 42U} && relations.member_count() == 4U &&
            relations.members(4U, live) == second_alias,
        "stale-member query failure neither mutates loaded groups nor returns a partial copy");
  std::optional<Members> nested;
  const auto outer = relations.members(4U, [&](auto) {
    nested = relations.members(10U, live); return true;
  });
  check(outer == second_alias && nested == std::optional<Members>{{3U}},
        "nested completed read-only queries safely return independent copies");
}

void test_ranges() {
  // Native resources deliberately have no arithmetic relationship. Expansion
  // must look up each source address at stride 112, including the endpoint.
  const std::map<std::uint32_t, std::uint64_t> resources{
      {0x40000160U, 901U}, {0x400001d0U, 77U}, {0x40000240U, 50000U}, {0x400002b0U, 3U}};
  for (const auto opaque_bit : {0U, 4U}) {
    Relations relations(payload({0U, 0x102U | opaque_bit, 0x251U | opaque_bit}));
    std::vector<std::uint32_t> lookups;
    relations.read([&](auto source) -> std::optional<std::uint64_t> {
      lookups.push_back(source);
      const auto found = resources.find(source);
      return found == resources.end() ? std::nullopt : std::optional{found->second};
    }, 4U);
    check(lookups == std::vector<std::uint32_t>{0x40000160U, 0x400001d0U, 0x40000240U, 0x400002b0U} &&
              relations.members(4U, live) == std::optional<Members>{{901U, 77U, 50000U, 3U}} &&
              relations.member_count() == 4U,
          "range resolves every stride-112 source slot and endpoint independently with inert bit two");
  }
  Relations adjacent(payload({0U, 0x102U, 0x172U, 0x1e1U}));
  adjacent.read([&](auto source) -> std::optional<std::uint64_t> { return resources.at(source); });
  check(adjacent.members(4U, live) == std::optional<Members>{{901U, 77U, 50000U}},
        "adjacent encoded ranges retain each shared endpoint only as the next range start");

  Relations insufficient(payload({0U, 0x102U, 0x251U}));
  std::vector<std::uint32_t> looked_up;
  const Relations::Resolve resolve = [&](auto source) -> std::optional<std::uint64_t> {
    looked_up.push_back(source); return resources.at(source);
  };
  rejects([&] { insufficient.read(resolve, 3U); }, "range endpoint also counts against the member limit");
  check_unpublished(insufficient);
  check(looked_up.size() <= 3U, "range limit failure must not resolve beyond the allowed member count");
  looked_up.clear();
  insufficient.read(resolve, 4U);
  check(insufficient.member_count() == 4U && looked_up.size() == 4U,
        "a failed atomic load can be retried without stale partial groups");

  Relations unresolved_middle(payload({0U, 0x102U, 0x251U}));
  rejects([&] {
    unresolved_middle.read([&](auto source) -> std::optional<std::uint64_t> {
      return source == 0x400001d0U ? std::nullopt : std::optional{resources.at(source)};
    });
  }, "range intermediate slots must resolve even when both authored endpoints exist");
  check_unpublished(unresolved_middle);
}

void test_rejected_loads() {
  for (const auto& input : {
      payload({0U, 0U, 0x101U}), payload({0U, 0x100U, 1U}),
      payload({0U, 3U}), payload({0U, 2U, 0x101U}),
      payload({0U, 0x103U}), payload({0U, 0x102U, 0x101U}),
      payload({0U, 0x102U, 0xf1U}), payload({0U, 0x102U, 0x179U}),
      payload({0U, 0x102U, 1U}), payload({0U, 0x3fffffa1U}),
      payload({0U, 0x40000001U}), payload({0U, 0xfffffff9U}),
      payload({0U, 0x3fffff32U, 0x3fffffa1U})}) {
    Relations relations(input);
    rejects([&] { relations.read([](auto) -> std::optional<std::uint64_t> { return 17U; }); },
            "unsupported null, terminal range, reversed/unaligned range or overflowing source domain must reject");
    check_unpublished(relations);
  }
  Relations upper_bound(payload({0U, 0x3fffff99U}));
  upper_bound.read([](auto source) -> std::optional<std::uint64_t> {
    check(source == 0x7ffffff8U, "highest supported aligned address receives bias before domain marking");
    return 17U;
  });
  check(upper_bound.member_count() == 1U, "highest valid biased source address is not rejected as overflow");

  Relations no_members(payload({0U, 1U}));
  rejects([&] { no_members.read({}); }, "initial loading requires a resolver even for empty groups");
  check_unpublished(no_members);
  unsigned called{};
  no_members.read([&](auto) -> std::optional<std::uint64_t> { ++called; return 17U; }, 0U);
  check(called == 0U && no_members.member_count() == 0U && no_members.group_count() == 1U,
        "zero limit admits a sole-null group without resolving a fabricated resource zero");
  Relations no_groups(payload({}));
  no_groups.read([](auto) -> std::optional<std::uint64_t> { throw std::runtime_error("unexpected empty lookup"); }, 0U);
  check(no_groups.loaded() && no_groups.group_count() == 0U && !no_groups.members(0U, live),
        "empty relation prefix still retains the distinct no-relation query");

  for (const auto result : {std::optional<std::uint64_t>{}, std::optional<std::uint64_t>{0U}}) {
    Relations relations(payload({0U, 0x101U}));
    rejects([&] { relations.read([&](auto) { return result; }); },
            "nonzero authored source requires a nonzero live native identity, not slot-zero-as-null");
    check_unpublished(relations);
  }
  Relations zero_limit(payload({0U, 0x101U}));
  rejects([&] { zero_limit.read([](auto) -> std::optional<std::uint64_t> { return 17U; }, 0U); },
          "zero member limit rejects nonempty groups");
  check_unpublished(zero_limit);
  Relations late_failure(payload({0U, 0x101U, 0U, 0x181U}));
  unsigned resolves{};
  rejects([&] {
    late_failure.read([&](auto) -> std::optional<std::uint64_t> {
      if (++resolves == 2U) throw std::runtime_error("independent resolver failure");
      return 17U;
    });
  }, "failure in a later group cannot publish an earlier successful group");
  check(resolves == 2U, "late-failure fixture reached a completed lookup prefix");
  check_unpublished(late_failure);
}

void test_malformed_storage_and_reentry() {
  const auto complete = payload({0U, 0x101U});
  for (std::size_t size = 0; size < complete.size(); ++size) {
    rejects([&] { Relations bad(std::span<const std::byte>{complete}.first(size)); },
            "every truncated serialized relation container must reject");
  }
  for (const auto& input : {payload({2U, 0x101U}), payload({0U}), payload({0U, 0x100U}),
                            payload({0U, 0x101U}, 1U)})
    rejects([&] { Relations bad(input); },
            "nonzero relation-chain head, missing terminator and malformed workspace must reject");
  auto oversized = complete;
  oversized[0] = std::byte{0xff};
  rejects([&] { Relations bad(oversized); }, "declared prefix cannot escape its owned payload");
  auto overlong = complete;
  overlong.push_back(std::byte{0});
  rejects([&] { Relations bad(overlong); }, "unframed bytes cannot trail the declared workspaces");

  Relations relations(complete);
  unsigned callbacks{};
  relations.read([&](auto) -> std::optional<std::uint64_t> {
    ++callbacks;
    rejects([&] { relations.read([](auto) -> std::optional<std::uint64_t> { return 9U; }); },
            "initial relation read cannot recursively load its own staged container");
    rejects([&] { static_cast<void>(relations.members(0U, live)); },
            "initial read cannot query an unpublished no-relation selector");
    rejects([&] { static_cast<void>(relations.members(4U, live)); },
            "initial read cannot query unpublished partial members");
    return 17U;
  });
  check(callbacks == 1U && relations.members(4U, live) == std::optional<Members>{{17U}},
        "handled reentry rejection leaves the outer transaction intact");
  Relations escaping(complete);
  rejects([&] {
    escaping.read([&](auto) -> std::optional<std::uint64_t> {
      escaping.read([](auto) -> std::optional<std::uint64_t> { return 9U; });
      return 17U;
    });
  }, "unhandled recursive read failure aborts the outer load atomically");
  check_unpublished(escaping);
}
} // namespace

int main() {
  try {
    test_owned_storage_and_queries();
    test_ranges();
    test_rejected_loads();
    test_malformed_storage_and_reentry();
    std::cout << "intro renderer resource relations tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
