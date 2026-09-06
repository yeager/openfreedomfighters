#include "off/graphics/intro_prepared_resources.hpp"

#include "off/data/zip_archive.hpp"
#include "off/data/archive_vfs.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace off::graphics {
namespace {

const data::ZipEntry &unique_member(const data::ZipArchive &archive,
                                    std::string_view extension) {
  const data::ZipEntry *found = nullptr;
  for (const auto &entry : archive.entries()) {
    const auto dot = entry.name.find_last_of('.');
    if (dot == std::string::npos)
      continue;
    auto suffix = entry.name.substr(dot);
    std::transform(
        suffix.begin(), suffix.end(), suffix.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (suffix != extension)
      continue;
    if (found)
      throw std::runtime_error("intro archive has duplicate " +
                               std::string(extension) + " members");
    found = &entry;
  }
  if (!found)
    throw std::runtime_error("intro archive has no " + std::string(extension) +
                             " member");
  return *found;
}

std::size_t required_source(const data::GmsImage &sources,
                            std::uint32_t reference) {
  const auto index = sources.local_source_for_authored_reference(reference);
  if (!index)
    throw std::runtime_error("intro required source reference is null");
  return *index;
}

} // namespace

IntroPreparedAudio::IntroPreparedAudio(std::span<const IntroPreparedSound> sounds,
    data::AudioBankHeader header,data::VfsFileView local,data::VfsFileView global)
    : header_(std::move(header)),local_(std::move(local)),global_(std::move(global)) {
  header_.validate_payload_ranges(local_.size(),global_.size());
  records_.reserve(sounds.size());
  for(const auto& sound:sounds)
    records_.push_back(header_.record_index_for_sound_link(sound.definition.resource_link));
}

std::vector<std::byte> IntroPreparedAudio::read_encoded(std::size_t sound_index,std::size_t byte_budget) const {
  if(sound_index>=records_.size()) throw std::runtime_error("Intro sound index is out of range");
  const auto index=records_[sound_index];
  if(!index) throw std::runtime_error("Intro sound definition has no stream request");
  const auto& record=header_.records()[*index];
  if(record.encoded_size>byte_budget) throw std::runtime_error("Intro encoded sound exceeds native byte budget");
  return (record.uses_global_bank()?global_:local_).read(record.data_offset,record.encoded_size);
}

audio::DecodedAudio IntroPreparedAudio::decode(std::size_t sound_index) const {
  const auto encoded=read_encoded(sound_index);
  const auto record=*records_[sound_index];
  return audio::decode_bank_stream(header_.records()[record],encoded);
}

audio::IntroAudioStream IntroPreparedAudio::open_stream(std::size_t sound_index) const {
  if (sound_index>=records_.size()) throw std::runtime_error("Intro sound index is out of range");
  const auto index=records_[sound_index];
  if (!index) throw std::runtime_error("Intro sound definition has no stream request");
  const auto& record=header_.records()[*index];
  return audio::IntroAudioStream(record,
      (record.uses_global_bank()?global_:local_).open_reader());
}

audio::IntroAudioStreamSource IntroPreparedAudio::open_source_for_sound_link(std::uint32_t link) const {
  const auto index=header_.record_index_for_sound_link(link);
  if(!index) throw std::runtime_error("Intro stereo command has no WHD source");
  const auto record=header_.records()[*index];
  if(!record.data_offset) throw std::runtime_error("Intro stereo command has no encoded-data offset");
  return {record,(record.uses_global_bank()?global_:local_).open_reader()};
}

IntroPreparedResources build_intro_prepared_resources(
    data::GmsImage sources, std::span<const std::byte> names,
    std::span<const std::byte> primitives, const data::TextureCatalog &textures,
    std::size_t decoded_byte_budget, std::span<const std::byte> sound_definitions) {
  sources.validate_buf(names);
  std::optional<std::size_t> controller_index;
  for (std::size_t index = 0; index < sources.directory().size(); ++index) {
    const auto &entry = sources.directory()[index];
    for (std::size_t attachment = 0; attachment < entry.attachments.size();
         ++attachment) {
      if (sources.attachment_identifier(index, attachment) !=
          "ZGEOM_MovieControl")
        continue;
      if (controller_index)
        throw std::runtime_error(
            "intro has multiple movie-controller attachments");
      controller_index = index;
    }
  }
  if (!controller_index)
    throw std::runtime_error("intro has no movie-controller attachment");

  IntroPreparedResources result(std::move(sources));
  const auto &gms = result.sources_;
  result.names_.assign(names.begin(), names.end());
  if(!sound_definitions.empty())
    result.sound_bank_.emplace(data::SoundDefinitionBank::parse(sound_definitions,1024U*1024U));
  for(std::size_t index=0;index<gms.directory().size();++index) {
    if(gms.directory()[index].source_type!=0x00200012U) continue;
    const auto source=gms.intro_sound_owner_prefix(index);
    if(!result.sound_bank_) throw std::runtime_error("Intro sound owner has no SND definition bank");
    auto definition=result.sound_bank_->simple_definition(source.sound_definition_reference);
    if(!definition) throw std::runtime_error("Intro sound owner has a null sound definition");
    IntroPreparedSound sound{index,source,std::move(*definition),gms.intro_sound_attachments(index)};
    for(std::size_t group=0;group<sound.segment_times.size();++group) {
      const auto& time=sound.attachments.segment.times[group];
      const std::uint32_t whole=((time[0]*std::uint32_t{60}+time[1])*std::uint32_t{60}+time[2]);
      // Explicit binary32 boundaries prevent fusion and preserve unsigned
      // conversion after integer wrap, including high-bit-set source values.
      volatile float seconds=static_cast<float>(whole);
      volatile float fraction=static_cast<float>(time[3]);
      volatile float scaled=fraction*0.04F;
      volatile float total=seconds+scaled;
      sound.segment_times[group]=total;
    }
    result.sounds_.push_back(std::move(sound));
  }
  result.controller_index_ = *controller_index;
  // Matching identity has been selected uniquely. Payload errors must
  // propagate.
  result.controller_ = gms.intro_movie_controller_source(*controller_index);
  const auto &controller = result.controller_;
  result.cuts_ = gms.intro_source_reference_list(
      required_source(gms, controller.sequence_reference));
  result.groups_ = gms.intro_source_reference_list(
      required_source(gms, controller.group_reference));
  const auto validate_reference = [&](std::uint32_t reference) {
    static_cast<void>(gms.local_source_for_authored_reference(reference));
  };
  for (const auto reference : result.cuts_)
    validate_reference(reference);
  for (const auto reference : result.groups_)
    validate_reference(reference);
  validate_reference(controller.additional_reference);
  if (controller.first_optional_reference)
    validate_reference(*controller.first_optional_reference);
  if (controller.second_optional_reference)
    validate_reference(*controller.second_optional_reference);
  if (result.cuts_.empty())
    throw std::runtime_error("intro cut list is empty");

  result.first_cut_index_ = required_source(gms, result.cuts_.front());
  result.first_cut_ = gms.intro_first_cut_source(result.first_cut_index_);
  validate_reference(result.first_cut_.settings_words[3]);
  result.member_index_ =
      required_source(gms, result.first_cut_.sequence_reference);
  result.member_ = gms.intro_cut_sequence_source(result.member_index_);
  for (const auto reference : result.member_.references)
    validate_reference(reference);
  result.camera_index_ = required_source(gms, result.member_.references[0]);
  result.camera_ = gms.intro_camera_source(result.camera_index_);

  // Supported first-cut shape: the camera is an authored child of its window.
  // Other scene windows have different grammars and remain unprepared sources.
  const auto selected_window = gms.hierarchy().at(result.camera_index_).parent_directory_index;
  if (!selected_window)
    throw std::runtime_error("intro first-cut camera has no authored window parent");
  result.window_index_ = *selected_window;
  result.window_ = gms.intro_window_source(*selected_window);
  for (const auto reference : result.window_.opaque_references)
    validate_reference(reference);
  if (required_source(gms, result.window_.selected_camera_reference) != result.camera_index_)
    throw std::runtime_error("intro window does not select its first-cut camera child");

  const auto add_picture = [&](std::size_t index, bool legal) {
    // Check the requested grammar even on aliases: a fade target cannot
    // silently reuse a legal picture merely because it was already prepared.
    auto source = legal ? gms.intro_legal_picture_source(index)
                        : gms.intro_fade_picture_source(index);
    if (std::any_of(result.pictures_.begin(), result.pictures_.end(),
                    [&](const auto &picture) {
                      return picture.directory_index == index;
                    }))
      return;
    auto picture = data::PictureResource::parse(primitives,
                                                source.picture_asset_reference);
    auto bindings = data::PictureTextureBindings::build(
        picture.texture_resources(), textures);
    result.pictures_.push_back(
        {index, std::move(source), std::move(picture), std::move(bindings)});
  };
  add_picture(required_source(gms, result.member_.references[1]), true);
  for (std::size_t i = 0; i < result.first_cut_.commands.size(); ++i) {
    const auto &command = result.first_cut_.commands[i];
    result.events_[i] = gms.authored_event_identifier(command.event_reference);
    add_picture(required_source(gms, command.target_reference), false);
  }

  std::vector<std::size_t> indexes;
  std::size_t total = 0;
  for (const auto &picture : result.pictures_) {
    for (const auto &binding : picture.bindings.entries()) {
      const auto index = binding.image_index;
      if (std::find(indexes.begin(), indexes.end(), index) != indexes.end())
        continue;
      if (index >= textures.images().size())
        throw std::runtime_error("intro texture index is out of range");
      const auto &image = textures.images()[index];
      // Division avoids overflow even for malformed maximum-u32 dimensions.
      const auto pixels =
          static_cast<std::uint64_t>(image.width) * image.height;
      if (pixels == 0 || pixels > (decoded_byte_budget - total) / 4U)
        throw std::runtime_error(
            "intro decoded images exceed the CPU preparation budget");
      total += static_cast<std::size_t>(pixels * 4U);
      indexes.push_back(index);
    }
  }
  // Validate the complete aggregate budget before decoding any image.
  for (const auto index : indexes) {
    const auto &image = textures.images()[index];
    auto decoded = decode_texture_mip(image, 0);
    if (decoded.width != image.width || decoded.height != image.height ||
        decoded.pixels.size() !=
            static_cast<std::uint64_t>(image.width) * image.height * 4U)
      throw std::runtime_error("intro image decoded to an invalid extent");
    result.images_.push_back({index, image.id, std::move(decoded)});
  }
  return result;
}

IntroPreparedResources
load_intro_prepared_resources(const std::filesystem::path &intro_archive) {
  const auto archive = data::ZipArchive::open(intro_archive);
  const auto &gms = unique_member(archive, ".gms");
  const auto &buf = unique_member(archive, ".buf");
  const auto &prm = unique_member(archive, ".prm");
  const auto &tex = unique_member(archive, ".tex");
  const auto &snd = unique_member(archive, ".snd");
  auto sources =
      data::GmsImage::parse(data::PackedResource::parse(archive.read(gms)));
  const auto names = archive.read(buf);
  const auto primitives = archive.read(prm);
  const auto textures = data::TextureCatalog::parse(archive.read(tex));
  const auto sound_definitions=archive.read(snd);
  auto result=build_intro_prepared_resources(std::move(sources), names, primitives,
                                            textures,intro_decoded_byte_budget,sound_definitions);
  if(!result.sounds_.empty()) {
    // Supported installation layout: scene archive and paired WHD/WAV live in
    // Scenes, global streams.wav in its parent. Logical SND names are not paths.
    data::ArchiveVfs vfs;
    constexpr std::array<std::string_view,5> excluded{
        "Freedom_Fighters_OST","Launcher.exe","eax.dll","steam_api.dll","steam_appid.txt"};
    const auto root=intro_archive.parent_path().parent_path();
    static_cast<void>(vfs.mount_directory(root,excluded));
    const auto base=(intro_archive.parent_path().filename()/intro_archive.stem()).generic_string();
    auto header=data::AudioBankHeader::parse(vfs.read(base+".WHD"));
    result.audio_.emplace(result.sounds_,std::move(header),
                          vfs.open_stream(base+".WAV"),vfs.open_stream("streams.wav"));
  }
  return result;
}

} // namespace off::graphics
