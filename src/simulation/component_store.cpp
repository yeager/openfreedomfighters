#include "off/simulation/component_store.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace off::simulation {
namespace {
constexpr std::array<std::uint8_t, 8> kMagic{'O', 'F', 'F', 'C', 'M', 'P', 0, 0};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kEndianMarker = 0x01020304U;
constexpr std::uint32_t kHeaderBytes = 60;

class Writer final {
public:
  template <class Integer> void integer(Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto encoded = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(encoded); ++index) {
      bytes_.push_back(static_cast<std::byte>(encoded & 0xffU));
      if constexpr (sizeof(Unsigned) > 1U) encoded >>= 8U;
    }
  }
  void bytes(std::span<const std::byte> values) {
    bytes_.insert(bytes_.end(), values.begin(), values.end());
  }
  [[nodiscard]] std::vector<std::byte> take() && { return std::move(bytes_); }
private:
  std::vector<std::byte> bytes_;
};

class Reader final {
public:
  explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}
  template <class Integer> Integer integer() {
    using Unsigned = std::make_unsigned_t<Integer>;
    if (position_ > bytes_.size() || bytes_.size() - position_ < sizeof(Unsigned))
      throw std::invalid_argument("truncated component snapshot");
    Unsigned result{};
    for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
      const auto byte = static_cast<Unsigned>(
          std::to_integer<std::uint8_t>(bytes_[position_++]));
      result = static_cast<Unsigned>(
          result | static_cast<Unsigned>(byte << (index * 8U)));
    }
    return static_cast<Integer>(result);
  }
  [[nodiscard]] std::span<const std::byte> bytes(std::size_t count) {
    if (position_ > bytes_.size() || count > bytes_.size() - position_)
      throw std::invalid_argument("truncated component snapshot payload");
    const auto result = bytes_.subspan(position_, count);
    position_ += count;
    return result;
  }
  [[nodiscard]] bool exhausted() const noexcept { return position_ == bytes_.size(); }
private:
  std::span<const std::byte> bytes_;
  std::size_t position_{};
};

void validate_limits(const ComponentStoreLimits &limits) {
  if (limits.maximum_records == 0U || limits.maximum_types == 0U ||
      limits.maximum_payload_bytes == 0U || limits.maximum_record_bytes == 0U ||
      limits.maximum_record_bytes > limits.maximum_payload_bytes ||
      limits.maximum_payload_bytes > std::numeric_limits<std::uint32_t>::max() ||
      limits.maximum_record_bytes > std::numeric_limits<std::uint32_t>::max())
    throw std::invalid_argument("component store limits are invalid");
}

bool fits(const ComponentStoreLimits &actual, const ComponentStoreLimits &maximum) {
  return actual.maximum_records <= maximum.maximum_records &&
         actual.maximum_types <= maximum.maximum_types &&
         actual.maximum_payload_bytes <= maximum.maximum_payload_bytes &&
         actual.maximum_record_bytes <= maximum.maximum_record_bytes;
}
} // namespace

ComponentStore::ComponentStore(ComponentStoreLimits limits) : limits_(limits) {
  validate_limits(limits_);
}

bool ComponentStore::valid_key(std::uint32_t type, EntityId entity) noexcept {
  return type != 0U && (entity.index != 0U || entity.generation != 0U);
}
bool ComponentStore::key_less(const ComponentRecord &left, const ComponentRecord &right) noexcept {
  if (left.type != right.type) return left.type < right.type;
  if (left.entity.index != right.entity.index) return left.entity.index < right.entity.index;
  return left.entity.generation < right.entity.generation;
}
std::vector<ComponentRecord>::iterator ComponentStore::lower_bound(std::uint32_t type, EntityId entity) noexcept {
  return std::lower_bound(records_.begin(), records_.end(),
                          ComponentRecord{.type=type, .entity=entity, .payload={}}, key_less);
}
std::vector<ComponentRecord>::const_iterator ComponentStore::lower_bound(std::uint32_t type, EntityId entity) const noexcept {
  return std::lower_bound(records_.begin(), records_.end(),
                          ComponentRecord{.type=type, .entity=entity, .payload={}}, key_less);
}
void ComponentStore::validate_payload_size(std::size_t bytes) const {
  if (bytes > limits_.maximum_record_bytes || bytes > limits_.maximum_payload_bytes)
    throw std::length_error("component payload exceeds store limits");
}

void ComponentStore::upsert(std::uint32_t type, EntityId entity,
                            std::span<const std::byte> payload) {
  if (!valid_key(type, entity)) throw std::invalid_argument("component record key is invalid");
  validate_payload_size(payload.size());
  auto found = lower_bound(type, entity);
  const bool present = found != records_.end() && found->type == type && found->entity == entity;
  const auto old_size = present ? found->payload.size() : 0U;
  if (payload.size() > limits_.maximum_payload_bytes - (payload_bytes_ - old_size))
    throw std::length_error("component payload capacity exceeded");
  if (!present) {
    if (records_.size() >= limits_.maximum_records) throw std::length_error("component record capacity exceeded");
    const auto type_begin = std::lower_bound(records_.begin(), records_.end(), type,
                                             [](const ComponentRecord &record, std::uint32_t value) { return record.type < value; });
    const auto new_type = type_begin == records_.end() || type_begin->type != type;
    if (new_type) {
      const auto types = [&] { std::size_t count{}; std::uint32_t previous{}; for (const auto &record : records_) { if (record.type != previous) { ++count; previous = record.type; } } return count; }();
      if (types >= limits_.maximum_types) throw std::length_error("component type capacity exceeded");
    }
    found = records_.insert(found, ComponentRecord{.type=type, .entity=entity, .payload={}});
  }
  payload_bytes_ = payload_bytes_ - old_size + payload.size();
  found->payload.assign(payload.begin(), payload.end());
}

bool ComponentStore::erase(std::uint32_t type, EntityId entity) noexcept {
  if (!valid_key(type, entity)) return false;
  const auto found = lower_bound(type, entity);
  if (found == records_.end() || found->type != type || found->entity != entity) return false;
  payload_bytes_ -= found->payload.size(); records_.erase(found); return true;
}
void ComponentStore::erase_entity(EntityId entity) noexcept {
  if (entity.index == 0U && entity.generation == 0U) return;
  for (auto found = records_.begin(); found != records_.end();) {
    if (found->entity == entity) { payload_bytes_ -= found->payload.size(); found = records_.erase(found); }
    else ++found;
  }
}
void ComponentStore::clear() noexcept { records_.clear(); payload_bytes_ = 0U; }
std::span<const std::byte> ComponentStore::find(std::uint32_t type, EntityId entity) const noexcept {
  if (!valid_key(type, entity)) return {};
  const auto found = lower_bound(type, entity);
  return found != records_.end() && found->type == type && found->entity == entity
             ? std::span<const std::byte>(found->payload) : std::span<const std::byte>{};
}

crypto::Sha256Digest ComponentStore::state_hash() const {
  Writer writer;
  for (const auto value : std::array<std::uint8_t, 8>{'O','F','F','C','M','P',1,0}) writer.integer(value);
  writer.integer(limits_.maximum_records); writer.integer(limits_.maximum_types);
  writer.integer(static_cast<std::uint64_t>(limits_.maximum_payload_bytes)); writer.integer(static_cast<std::uint64_t>(limits_.maximum_record_bytes));
  writer.integer(static_cast<std::uint64_t>(records_.size()));
  for (const auto &record : records_) { writer.integer(record.type); writer.integer(record.entity.index); writer.integer(record.entity.generation); writer.integer(static_cast<std::uint64_t>(record.payload.size())); writer.bytes(record.payload); }
  crypto::Sha256 hash; const auto bytes = std::move(writer).take(); hash.update(bytes); return hash.finish();
}

std::vector<std::byte> ComponentStore::export_snapshot() const {
  Writer payload;
  payload.integer(limits_.maximum_records); payload.integer(limits_.maximum_types);
  payload.integer(static_cast<std::uint64_t>(limits_.maximum_payload_bytes)); payload.integer(static_cast<std::uint64_t>(limits_.maximum_record_bytes));
  payload.integer(static_cast<std::uint32_t>(records_.size()));
  for (const auto &record : records_) { payload.integer(record.type); payload.integer(record.entity.index); payload.integer(record.entity.generation); payload.integer(static_cast<std::uint32_t>(record.payload.size())); payload.bytes(record.payload); }
  auto body = std::move(payload).take(); crypto::Sha256 hash; hash.update(body); const auto digest = hash.finish();
  Writer result; for (const auto value : kMagic) result.integer(value); result.integer(kVersion); result.integer(kEndianMarker); result.integer(kHeaderBytes); result.integer(static_cast<std::uint64_t>(body.size())); for (const auto value : digest) result.integer(value); auto bytes = std::move(result).take(); bytes.insert(bytes.end(), body.begin(), body.end()); return bytes;
}

void ComponentStore::import_snapshot(std::span<const std::byte> bytes, ComponentSnapshotReadLimits policy) {
  validate_limits(policy.maximum_store);
  if (bytes.size() < kHeaderBytes || bytes.size() > policy.maximum_bytes) throw std::invalid_argument("component snapshot size is invalid");
  Reader header(bytes.first(kHeaderBytes));
  for (const auto expected : kMagic) if (header.integer<std::uint8_t>() != expected) throw std::invalid_argument("component snapshot magic is invalid");
  if (header.integer<std::uint32_t>() != kVersion || header.integer<std::uint32_t>() != kEndianMarker || header.integer<std::uint32_t>() != kHeaderBytes) throw std::invalid_argument("component snapshot version is unsupported");
  const auto size = header.integer<std::uint64_t>(); std::array<std::uint8_t, 32> expected{}; for (auto &value : expected) value=header.integer<std::uint8_t>();
  if (!header.exhausted() || size != bytes.size() - kHeaderBytes) throw std::invalid_argument("component snapshot length is invalid");
  const auto body = bytes.subspan(kHeaderBytes); crypto::Sha256 hash; hash.update(body); if (hash.finish() != expected) throw std::invalid_argument("component snapshot checksum is invalid");
  Reader reader(body); ComponentStoreLimits stored{.maximum_records=reader.integer<std::uint32_t>(), .maximum_types=reader.integer<std::uint32_t>(), .maximum_payload_bytes=static_cast<std::size_t>(reader.integer<std::uint64_t>()), .maximum_record_bytes=static_cast<std::size_t>(reader.integer<std::uint64_t>())};
  validate_limits(stored); if (!fits(stored, policy.maximum_store)) throw std::invalid_argument("component snapshot limits exceed policy");
  ComponentStore staged(stored); const auto count=reader.integer<std::uint32_t>(); if (count > stored.maximum_records) throw std::invalid_argument("component snapshot record count is invalid");
  for (std::uint32_t index=0; index<count; ++index) {
    const auto type=reader.integer<std::uint32_t>(); const EntityId entity{.index=reader.integer<std::uint32_t>(), .generation=reader.integer<std::uint32_t>()}; const auto size=reader.integer<std::uint32_t>();
    if (!valid_key(type, entity) || size > stored.maximum_record_bytes) throw std::invalid_argument("component snapshot record is invalid");
    const auto payload=reader.bytes(size);
    if (!staged.records_.empty() && !key_less(staged.records_.back(), ComponentRecord{.type=type,.entity=entity,.payload={}})) throw std::invalid_argument("component snapshot order is invalid");
    staged.upsert(type, entity, payload);
  }
  if (!reader.exhausted()) throw std::invalid_argument("component snapshot has trailing payload bytes");
  *this=std::move(staged);
}
} // namespace off::simulation
