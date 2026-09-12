#include "off/simulation/component_store.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char *message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
std::vector<std::byte> bytes(std::initializer_list<unsigned char> values) { std::vector<std::byte> result; for (const auto value : values) result.push_back(static_cast<std::byte>(value)); return result; }
}

int main() {
  using namespace off::simulation;
  ComponentStore store({.maximum_records=4, .maximum_types=2, .maximum_payload_bytes=12, .maximum_record_bytes=8});
  const auto a=bytes({1,2,3}); const auto b=bytes({4,5}); const auto replacement=bytes({7});
  store.upsert(2, {4,1}, a); store.upsert(1, {9,2}, b); store.upsert(1, {3,1}, a);
  check(store.records().size()==3 && store.records()[0].type==1 && store.records()[0].entity.index==3 && store.records()[1].entity.index==9 && store.records()[2].type==2 && store.payload_bytes()==8, "canonical record order is independent of insertion order");
  store.upsert(1, {9,2}, replacement);
  check(store.records().size()==3 && std::ranges::equal(store.find(1,{9,2}), replacement) && store.payload_bytes()==7, "exact key upsert replaces payload deterministically");
  const auto snapshot=store.export_snapshot(); ComponentStore restored; restored.import_snapshot(snapshot);
  check(std::ranges::equal(restored.records(), store.records()) && restored.state_hash()==store.state_hash() && restored.export_snapshot()==snapshot, "component snapshot round trips canonical state");
  const auto before=restored.state_hash(); auto corrupt=snapshot; corrupt.back()^=std::byte{1}; bool rejected=false; try { restored.import_snapshot(corrupt); } catch(const std::invalid_argument &) { rejected=true; }
  check(rejected && restored.state_hash()==before, "invalid snapshot leaves destination unchanged");
  bool all_truncated=true;
  for(std::size_t length=0; length<snapshot.size(); ++length) {
    try { ComponentStore target; target.import_snapshot({snapshot.data(), length}); all_truncated=false; }
    catch(const std::invalid_argument &) {}
  }
  check(all_truncated, "every truncated component snapshot prefix is rejected");
  rejected=false; try { store.upsert(0,{1,1},a); } catch(const std::invalid_argument &) { rejected=true; }
  check(rejected, "reserved component key is rejected");
  rejected=false; try { store.upsert(3,{1,1},a); } catch(const std::length_error &) { rejected=true; }
  check(rejected, "type capacity is bounded");
  check(store.erase(2,{4,1}) && !store.erase(2,{4,1}), "erase reports exact membership");
  store.erase_entity({3,1}); check(store.records().size()==1 && store.records()[0].entity.index==9, "entity erase removes every component owned by exact generation");
  return 0;
}
