#include "off/data/picture_resource.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <stdexcept>

namespace off::data {
namespace {

constexpr std::size_t descriptor_size = 40;
constexpr std::size_t frame_value_size = 4;
constexpr std::size_t frame_record_size = 8;
constexpr std::size_t frame_texture_resource_size = 32;
constexpr std::uint32_t frame_texture_resource_marker = 0x00020100U;
constexpr std::size_t prm_header_size = 16;
constexpr std::size_t maximum_descriptor_count = 4'096;
constexpr std::size_t maximum_frame_count = 4'096;

[[nodiscard]] std::size_t checked_array_end(
    std::size_t offset,
    std::size_t count,
    std::size_t stride,
    std::size_t allocation_size,
    const char* message
) {
    if (offset > allocation_size || count > (allocation_size - offset) / stride) {
        throw std::runtime_error(message);
    }
    return offset + count * stride;
}

}  // namespace

PictureResource PictureResource::parse(
    std::span<const std::byte> allocation,
    std::uint32_t relocation_key
) {
    const auto resource_offset = static_cast<std::size_t>(relocation_key);
    if (allocation.size() < prm_header_size) {
        throw std::runtime_error("picture resource is missing its PRM envelope");
    }
    const ByteReader reader(allocation);
    const auto base_data_end = static_cast<std::size_t>(reader.u32(0));
    const auto primitive_index_offset = static_cast<std::size_t>(reader.u32(4));
    const auto repeated_index_offset = static_cast<std::size_t>(reader.u32(8));
    const auto primitive_count = static_cast<std::size_t>(reader.u32(12));
    if (base_data_end < prm_header_size ||
        base_data_end > primitive_index_offset ||
        primitive_index_offset != repeated_index_offset ||
        (primitive_index_offset & 1U) != 0U ||
        primitive_index_offset > allocation.size() ||
        primitive_count > (allocation.size() - primitive_index_offset) / 4U ||
        primitive_index_offset + primitive_count * 4U != allocation.size()) {
        throw std::runtime_error("picture resource has an invalid PRM envelope");
    }
    if ((resource_offset & 1U) != 0U || resource_offset < prm_header_size ||
        resource_offset > primitive_index_offset ||
        sizeof(std::uint32_t) > primitive_index_offset - resource_offset) {
        throw std::runtime_error("picture resource relocation key is out of range");
    }
    const auto descriptor_count = static_cast<std::size_t>(
        reader.u32(resource_offset)
    );
    if (descriptor_count > maximum_descriptor_count) {
        throw std::runtime_error("picture resource descriptor count exceeds its limit");
    }
    const auto descriptor_start = resource_offset + sizeof(std::uint32_t);
    const auto descriptor_end = checked_array_end(
        descriptor_start,
        descriptor_count,
        descriptor_size,
        primitive_index_offset,
        "picture resource descriptor array exceeds its allocation"
    );
    if (descriptor_end > primitive_index_offset ||
        sizeof(std::uint32_t) > primitive_index_offset - descriptor_end) {
        throw std::runtime_error("picture resource is missing its frame count");
    }
    const auto frame_count = static_cast<std::size_t>(reader.u32(descriptor_end));
    if (frame_count > maximum_frame_count) {
        throw std::runtime_error("picture resource frame count exceeds its limit");
    }
    const auto frame_value_start = descriptor_end + sizeof(std::uint32_t);
    const auto frame_value_end = checked_array_end(
        frame_value_start,
        frame_count,
        frame_value_size,
        primitive_index_offset,
        "picture resource frame-value array exceeds its allocation"
    );
    const auto frame_end = checked_array_end(
        frame_value_end,
        frame_count,
        frame_record_size,
        primitive_index_offset,
        "picture resource frame array exceeds its allocation"
    );

    for (std::size_t index = 0; index < frame_count; ++index) {
        const auto texture_resource_offset = static_cast<std::size_t>(
            reader.u32(frame_value_start + index * frame_value_size)
        );
        if ((texture_resource_offset & 1U) != 0U ||
            texture_resource_offset < prm_header_size ||
            texture_resource_offset > primitive_index_offset ||
            frame_texture_resource_size >
                primitive_index_offset - texture_resource_offset) {
            throw std::runtime_error(
                "picture frame texture-resource reference is out of range"
            );
        }
        if (reader.u32(texture_resource_offset) !=
            frame_texture_resource_marker) {
            throw std::runtime_error(
                "picture frame texture-resource marker is invalid"
            );
        }
        const auto frame_offset = frame_value_end + index * frame_record_size;
        const auto descriptor_index = static_cast<std::size_t>(
            reader.u32(frame_offset + sizeof(std::uint32_t))
        );
        if (descriptor_index >= descriptor_count) {
            throw std::runtime_error(
                "picture resource frame descriptor index is out of range"
            );
        }
    }

    PictureResource result;
    result.descriptors_.reserve(descriptor_count);
    for (std::size_t index = 0; index < descriptor_count; ++index) {
        PictureResourceDescriptor descriptor;
        const auto source = reader.slice(
            descriptor_start + index * descriptor_size, descriptor_size
        );
        std::ranges::copy(source, descriptor.encoded.begin());
        result.descriptors_.push_back(descriptor);
    }
    result.frame_texture_references_.reserve(frame_count);
    result.frame_texture_resources_.reserve(frame_count);
    result.frames_.reserve(frame_count);
    for (std::size_t index = 0; index < frame_count; ++index) {
        const auto texture_resource_reference =
            reader.u32(frame_value_start + index * frame_value_size);
        result.frame_texture_references_.push_back(texture_resource_reference);
        PictureFrameTextureResource texture_resource{
            .prm_offset = texture_resource_reference,
            .manager_key = reader.u16(
                static_cast<std::size_t>(texture_resource_reference) + 4U),
        };
        const auto texture_source = reader.slice(
            static_cast<std::size_t>(texture_resource_reference),
            frame_texture_resource_size
        );
        std::ranges::copy(
            texture_source, texture_resource.encoded.begin()
        );
        result.frame_texture_resources_.push_back(texture_resource);
        const auto frame_offset = frame_value_end + index * frame_record_size;
        const auto descriptor_index = static_cast<std::size_t>(
            reader.u32(frame_offset + sizeof(std::uint32_t))
        );
        result.frames_.push_back({
            .opaque_value = reader.u32(frame_offset),
            .descriptor_index = descriptor_index,
        });
    }
    result.encoded_size_ = frame_end - resource_offset;
    return result;
}

}  // namespace off::data
