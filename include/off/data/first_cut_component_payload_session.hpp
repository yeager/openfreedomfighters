#pragma once

#include "off/data/deferred_component_dispatcher.hpp"
#include "off/data/deferred_reader_session.hpp"
#include "off/data/first_cut_command_component_reader.hpp"
#include "off/data/first_cut_list_component_reader.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>

namespace off::data {

// One atomic, read-only parse of the reviewed first-cut component payload
// sequence. It neither owns the source block nor performs a component-reader
// mutation, event registration, lifecycle transition, audio operation, or draw.
struct FirstCutComponentPayloadSession final {
  FirstCutListComponentRecord list;
  std::array<FirstCutCommandComponentRecord,5> commands;

  [[nodiscard]] static FirstCutComponentPayloadSession read(
      const DeferredOwnerReaderResult& owner_reader_result) {
    return read(owner_reader_result.component_suffix,owner_reader_result.component_extent);
  }

  [[nodiscard]] static FirstCutComponentPayloadSession read(
      std::span<const std::byte> component_suffix,std::size_t component_extent) {
    if(component_extent!=157U || component_suffix.size()!=component_extent ||
        component_suffix.empty() || component_suffix.back()!=std::byte{0xff}) fail();
    FirstCutComponentPayloadSession result;
    std::array<DeferredComponentReader,6> readers{
        [&result](std::span<const std::byte>& payload) {
          result.list=FirstCutListComponentReader::read(payload);
        },
        [&result](std::span<const std::byte>& payload) {
          result.commands[0]=FirstCutCommandComponentReader::read(payload);
        },
        [&result](std::span<const std::byte>& payload) {
          result.commands[1]=FirstCutCommandComponentReader::read(payload);
        },
        [&result](std::span<const std::byte>& payload) {
          result.commands[2]=FirstCutCommandComponentReader::read(payload);
        },
        [&result](std::span<const std::byte>& payload) {
          result.commands[3]=FirstCutCommandComponentReader::read(payload);
        },
        [&result](std::span<const std::byte>& payload) {
          result.commands[4]=FirstCutCommandComponentReader::read(payload);
        }};
    const auto dispatch=DeferredComponentDispatcher::dispatch(
        component_suffix.first(component_extent),readers);
    if(dispatch.dispatched_components!=readers.size() || dispatch.continuation.size()!=1U ||
        dispatch.continuation.front()!=std::byte{0xff}) fail();
    return result;
  }

private:
  [[noreturn]] static void fail() {
    throw std::runtime_error("First-cut component payload session is unsupported or malformed");
  }
};

} // namespace off::data
