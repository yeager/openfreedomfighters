#pragma once
#include "off/data/postload_integer_profile.hpp"
#include "off/runtime/sequence_timing.hpp"
#include <memory>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace off::runtime {
// Project-owned data-exchange service for the bounded integer profile. An
// exchange stores typed values; it does not execute script or dispatch events.
class PostloadScriptService final {
public:
  struct Exchange { std::vector<data::PostloadIntegerEntry> entries; };
  [[nodiscard]] std::unique_ptr<Exchange> create() const {return std::make_unique<Exchange>();}
  void insert_integer(Exchange& exchange,std::string name,std::uint32_t bits) const {
    exchange.entries.push_back({std::move(name),bits});
  }
  void release(std::unique_ptr<Exchange>& exchange) const noexcept {exchange.reset();}
  using Hook = std::function<void(std::uint32_t,std::uint16_t,std::optional<SequenceTiming>&)>;
  void bind_hook(Hook hook) {hook_=std::move(hook);}
  void invoke(std::uint32_t key,std::uint16_t event,std::optional<SequenceTiming>& output) const {
    if(key==0U) return;
    const auto hook=hook_;
    if(!hook) throw std::runtime_error("postload script hook service is unavailable");
    hook(key,event,output);
  }
private:
  Hook hook_;
};
// Owned by the list factory. The same auxiliary container must be observed
// by later event hooks and post-cleanup; it never retains a source pointer.
struct PostloadScriptAux final {
  std::shared_ptr<PostloadScriptService> service;
  std::uint64_t service_generation{};
  std::unique_ptr<PostloadScriptService::Exchange> exchange;
  std::string source_text;
  std::uint32_t script_key{};
  std::vector<std::uint64_t> pending_work;
  ~PostloadScriptAux() {if(service) service->release(exchange);}
};
}
