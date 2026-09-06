#include "off/audio/stereo_stream_playback.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace off::audio {
namespace {
struct Guard {
  bool& busy;
  explicit Guard(bool& value):busy(value) {
    if(busy) throw std::runtime_error("Stereo playback control reentry");
    busy=true;
  }
  ~Guard() {busy=false;}
};
void reserve_notification(std::vector<std::uint64_t>& queue) {
  if(queue.size()>=4096) throw std::runtime_error("Stereo playback receive queue is full");
  queue.reserve(queue.size()+1); // Allocation precedes the irreversible device operation.
}
}
struct StereoStreamPlayback::Channel {
  StereoSourceCommand command;
  data::AudioStreamRecord record;
  IntroAudioStream stream;
  std::unique_ptr<StereoPcmOutput> output;
  bool started{}, held{}, pcm_ready{}, input_ended{};
  std::uint64_t submitted_frames{};
  Channel(const StereoSourceCommand& c,IntroAudioStreamSource source,
          std::unique_ptr<StereoPcmOutput> device)
    :command(c),record(source.record),stream(source.record,std::move(source.reader)),
     output(std::move(device)) {}
  ~Channel() {
    stream.cancel();
    // Close/pause the output before joining pending I/O. No teardown ACK.
    output.reset();
  }
};
StereoStreamPlayback::StereoStreamPlayback(SourceResolver source,OutputFactory output,
    StereoStreamPlaybackLimits limits)
  :source_(std::move(source)),output_(std::move(output)),limits_(limits) {
  if(!source_ || !output_ || !limits.channels || limits.channels>1024 ||
     !limits.decode_frames || limits.decode_frames>IntroAudioStream::maximum_request_frames ||
     limits.queue_frames<limits.decode_frames || limits.queue_frames>262144)
    throw std::invalid_argument("Invalid stereo playback services or native limits");
  channels_.resize(limits.channels);
}
StereoStreamPlayback::~StereoStreamPlayback()=default;
std::unique_ptr<StereoStreamPlayback::Channel>* StereoStreamPlayback::find(std::uint64_t binding) {
  for(auto& channel:channels_) if(channel && channel->command.binding==binding) return &channel;
  return nullptr;
}
void StereoStreamPlayback::update(Channel& c,const StereoSourceCommand& command) {
  if(c.command.whd_offset!=command.whd_offset)
    throw std::runtime_error("Active stereo source replacement requires stop/new start");
  c.command=command;
  if(c.held) {
    c.held=false;
    if(c.started) c.output->start(); // Resume queued position, no new start ACK.
  }
  c.output->set_pan(std::clamp(command.pan,-10000,10000));
  // Group -1 has no source-effect update. Other groups are rejected at admission.
  const auto controls=stereo_output_controls(command.gain,command.pan,
      command.frequency_adjustment,c.record.sample_rate);
  c.command.gain=controls.retained_gain;
  c.output->set_volume_hundredths_db(controls.volume_hundredths_db);
  c.output->set_frequency(controls.frequency_hz);
}
void StereoStreamPlayback::submit(std::span<const StereoSourceCommand> commands) {
  Guard guard(busy_);
  for(const auto& command:commands) {
    if(!command.binding || !command.whd_offset || (command.whd_offset&1U) ||
       command.loop || command.environment_group!=-1)
      throw std::runtime_error("Unsupported intro stereo source command");
    // Resolve even existing bindings. The active channel keeps its original
    // reader and format; this candidate validates the incoming bank reference.
    auto source=source_(command.whd_offset);
    if(source.record.format_flags!=0x80001000U || source.record.channels!=2 ||
       source.record.bits_per_sample!=16 || !source.record.data_offset)
      throw std::runtime_error("Intro stereo command requires a real global stereo Vorbis source");
    if(auto* slot=find(command.binding)) {
      try {update(**slot,command);} catch(...) {slot->reset();throw;}
      continue;
    }
    if(!command.start_requested) continue;
    const auto slot=std::find_if(channels_.begin(),channels_.end(),[](const auto& c){return !c;});
    if(slot==channels_.end()) throw std::runtime_error("Stereo channel capacity exhausted; priority replacement unsupported");
    auto output=output_(source.record.sample_rate,limits_.queue_frames*4);
    if(!output) throw std::runtime_error("Stereo output factory returned no device");
    auto channel=std::make_unique<Channel>(command,std::move(source),std::move(output));
    update(*channel,command);
    channel->stream.issue_initial_read();
    *slot=std::move(channel);
  }
}
void StereoStreamPlayback::refill(Channel& c) {
  using State=IntroAudioStreamState;
  auto state=c.stream.poll();
  if(state==State::failed) throw std::runtime_error(c.stream.error());
  if(state==State::cancelled) throw std::runtime_error("Active stereo decoder was cancelled");
  if(state==State::initial_completed) {
    c.stream.observe_initial_completion();
    state=State::ready;
  }
  if(state==State::pcm_completed) {
    auto pcm=c.stream.take_pcm();
    if(!pcm.interleaved_samples.empty()) {
      c.output->submit(pcm.interleaved_samples);
      c.submitted_frames+=pcm.interleaved_samples.size()/2;
      c.pcm_ready=true;
    }
    if(pcm.end_of_stream) {
      c.output->flush();
      c.input_ended=true; // Keep queued/converter/hardware tail until ordered stop.
    }
    state=c.stream.state();
  }
  if(state==State::ready) {
    const auto queued=c.output->queued_input_bytes();
    if(queued>limits_.queue_frames*4)
      throw std::runtime_error("Stereo output exceeds its bounded input queue");
    if((limits_.queue_frames*4-queued)/4>=limits_.decode_frames)
      c.stream.request_pcm(limits_.decode_frames);
  }
}
void StereoStreamPlayback::pump() {
  Guard guard(busy_);
  for(auto& slot:channels_) {
    if(!slot) continue;
    try {
      auto& c=*slot;
      refill(c);
      if(!c.started && !c.held && c.pcm_ready) {
        reserve_notification(notifications_.started);
        c.output->start();
        c.started=true;
        notifications_.started.push_back(c.command.binding);
      }
    } catch(...) {slot.reset();throw;}
  }
}
void StereoStreamPlayback::stop(std::uint64_t binding) {
  Guard guard(busy_);
  auto* slot=find(binding);
  if(!slot) return;
  reserve_notification(notifications_.stopped);
  try {(*slot)->output->stop();} catch(...) {slot->reset();throw;}
  slot->reset(); // Cancels and joins the exact old decoder before slot reuse.
  notifications_.stopped.push_back(binding);
}
void StereoStreamPlayback::hold(std::uint64_t binding,bool held) {
  Guard guard(busy_);
  auto* slot=find(binding);
  if(!slot || (*slot)->held==held) return;
  auto& c=**slot;
  try {
    if(c.started) {
      if(held) c.output->pause();
      else c.output->start();
    }
    c.held=held;
  } catch(...) {slot->reset();throw;}
}
std::optional<StereoStreamStatus> StereoStreamPlayback::status(std::uint64_t binding) const {
  Guard guard(busy_);
  for(const auto& c:channels_) if(c && c->command.binding==binding)
    return StereoStreamStatus{c->started,c->held,c->input_ended,c->submitted_frames,
      c->output->queued_input_bytes(),c->stream.encoded_bytes_read()};
  return std::nullopt;
}
StereoPlaybackNotifications StereoStreamPlayback::take_notifications() {
  Guard guard(busy_);
  return std::exchange(notifications_,{});
}
} // namespace off::audio
