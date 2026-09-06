#include "off/audio/intro_audio_stream.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <exception>
#include <future>
#include <stdexcept>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

namespace off::audio {
struct IntroAudioStream::Impl {
  data::AudioStreamRecord record;
  data::VfsFileReader reader;
  IntroAudioStreamState state{IntroAudioStreamState::idle};
  std::array<std::byte,2*window_bytes> encoded{};
  std::size_t available{}, cursor{}, requested{};
  std::atomic<std::size_t> bytes_read{};
  std::atomic<bool> cancelled{};
  std::future<void> work;
  std::string failure;
  std::exception_ptr callback_failure;
  OggVorbis_File decoder{};
  bool opened{}, eof{};
  std::uint64_t decoded_values{};
  IntroAudioPcm pcm;

  Impl(data::AudioStreamRecord r,data::VfsFileReader f):record(r),reader(std::move(f)) {
    if ((r.format_flags&~0x80000000U)!=0x1000 || r.bits_per_sample!=16 ||
        (r.channels!=1 && r.channels!=2) || r.block_align!=2*r.channels ||
        r.samples_per_block!=1 || !r.sample_rate || r.sample_rate>384000 ||
        !r.sample_value_count || r.sample_value_count%r.channels ||
        std::uint64_t(r.sample_value_count)*2!=r.decoded_byte_count ||
        r.sample_value_count>64U*1024U*1024U || !r.encoded_size ||
        r.encoded_size>64U*1024U*1024U || r.data_offset>reader.size() ||
        r.encoded_size>reader.size()-r.data_offset)
      throw std::runtime_error("Unsupported or out-of-range intro Vorbis stream");
  }
  ~Impl() {
    cancelled=true;
    if(work.valid()) work.wait();
    if(opened) ov_clear(&decoder);
  }
  void check_cancel() const {
    if(cancelled.load()) throw std::runtime_error("Intro audio stream cancelled");
  }
  void fill(std::size_t limit) {
    check_cancel();
    available=std::min<std::size_t>(limit,record.encoded_size-requested);
    cursor=0;
    reader.read_at(std::uint64_t(record.data_offset)+requested,
                   std::span<std::byte>(encoded).first(available));
    check_cancel();
    requested+=available;
    bytes_read.fetch_add(available);
  }
  static std::size_t read(void* destination,std::size_t size,std::size_t count,void* opaque) noexcept {
    auto& self=*static_cast<Impl*>(opaque);
    try {
      if(!size || !count) return 0;
      auto* out=static_cast<std::byte*>(destination);
      std::size_t elements=0;
      while(elements<count) {
        // libvorbisfile asks for byte elements. Reject unsupported element reads
        // instead of consuming a fractional final element.
        if(size!=1) throw std::runtime_error("Unsupported Vorbis callback element size");
        self.check_cancel();
        if(self.cursor==self.available) {
          if(self.requested==self.record.encoded_size) break;
          self.fill(window_bytes);
        }
        const auto n=std::min(count-elements,self.available-self.cursor);
        std::memcpy(out+elements,self.encoded.data()+self.cursor,n);
        self.cursor+=n; elements+=n;
      }
      return elements;
    } catch(...) {self.callback_failure=std::current_exception();return 0;}
  }
  void check_callback() {
    if(callback_failure) std::rethrow_exception(callback_failure);
    check_cancel();
  }
  void open() {
    ov_callbacks callbacks{read,nullptr,nullptr,nullptr};
    const int result=ov_open_callbacks(this,&decoder,nullptr,0,callbacks);
    if(result==0) opened=true;
    check_callback();
    if(result!=0) throw std::runtime_error("Invalid incremental Vorbis source");
    validate_info();
  }
  void validate_info() {
    const auto* info=ov_info(&decoder,-1);
    if(!info || info->channels!=static_cast<int>(record.channels) || info->rate<=0 ||
        static_cast<std::uint64_t>(info->rate)!=record.sample_rate)
      throw std::runtime_error("Incremental Vorbis metadata disagrees with WHD");
  }
  void decode(std::size_t frames) {
    check_cancel();
    if(!opened) open();
    pcm={};
    const auto limit=frames*record.channels;
    pcm.interleaved_samples.reserve(limit);
    std::array<char,8192> output{};
    while(pcm.interleaved_samples.size()<limit && !eof) {
      const auto remaining=limit-pcm.interleaved_samples.size();
      int stream=0;
      const long n=ov_read(&decoder,output.data(),static_cast<int>(std::min(remaining*2,output.size())),0,2,1,&stream);
      check_callback();
      if(n<0 || stream!=0 || n%static_cast<long>(record.channels*2))
        throw std::runtime_error("Corrupt or chained incremental Vorbis stream");
      validate_info();
      if(n==0) {
        eof=true;
        if(decoded_values!=record.sample_value_count)
          throw std::runtime_error("Incremental Vorbis EOF disagrees with WHD sample count");
        break;
      }
      const auto values=static_cast<std::size_t>(n)/2;
      if(values>record.sample_value_count-decoded_values)
        throw std::runtime_error("Incremental Vorbis exceeds WHD sample count");
      for(std::size_t i=0;i<static_cast<std::size_t>(n);i+=2) {
        const auto low=static_cast<unsigned char>(output[i]);
        const auto high=static_cast<unsigned char>(output[i+1]);
        pcm.interleaved_samples.push_back(std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(low|(high<<8))));
      }
      decoded_values+=values;
    }
    pcm.end_of_stream=eof;
  }
  template<class F> void launch(F operation,IntroAudioStreamState pending) {
    try {work=std::async(std::launch::async,std::move(operation));state=pending;}
    catch(const std::exception& e) {failure=e.what();state=IntroAudioStreamState::failed;throw;}
  }
};
IntroAudioStream::IntroAudioStream(data::AudioStreamRecord record,data::VfsFileReader reader)
  :impl_(std::make_unique<Impl>(record,std::move(reader))) {}
IntroAudioStream::~IntroAudioStream()=default;
IntroAudioStream::IntroAudioStream(IntroAudioStream&&) noexcept=default;
IntroAudioStream& IntroAudioStream::operator=(IntroAudioStream&&) noexcept=default;
void IntroAudioStream::issue_initial_read() {
  if(!impl_ || impl_->state!=IntroAudioStreamState::idle) throw std::runtime_error("Initial audio read is not admissible");
  auto* p=impl_.get();
  p->launch([p]{p->fill(2*window_bytes);},IntroAudioStreamState::initial_pending);
}
IntroAudioStreamState IntroAudioStream::poll() {
  if(!impl_) return IntroAudioStreamState::cancelled;
  auto& p=*impl_;
  if(p.state!=IntroAudioStreamState::initial_pending && p.state!=IntroAudioStreamState::pcm_pending) return p.state;
  if(p.work.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return p.state;
  try {
    p.work.get();
    p.state=p.state==IntroAudioStreamState::initial_pending?IntroAudioStreamState::initial_completed:IntroAudioStreamState::pcm_completed;
  } catch(const std::exception& e) {p.failure=e.what();p.state=IntroAudioStreamState::failed;}
  catch(...) {p.failure="Unknown intro audio worker failure";p.state=IntroAudioStreamState::failed;}
  return p.state;
}
void IntroAudioStream::observe_initial_completion() {
  if(!impl_ || impl_->state!=IntroAudioStreamState::initial_completed) throw std::runtime_error("Initial audio completion has not been delivered");
  impl_->state=IntroAudioStreamState::ready;
}
void IntroAudioStream::request_pcm(std::size_t frames) {
  if(!impl_ || impl_->state!=IntroAudioStreamState::ready || !frames || frames>maximum_request_frames)
    throw std::runtime_error("Incremental PCM request is not admissible");
  auto* p=impl_.get();
  p->launch([p,frames]{p->decode(frames);},IntroAudioStreamState::pcm_pending);
}
IntroAudioPcm IntroAudioStream::take_pcm() {
  if(!impl_ || impl_->state!=IntroAudioStreamState::pcm_completed) throw std::runtime_error("PCM completion has not been delivered");
  auto result=std::move(impl_->pcm);
  impl_->state=result.end_of_stream?IntroAudioStreamState::ended:IntroAudioStreamState::ready;
  return result;
}
void IntroAudioStream::cancel() noexcept {
  if(impl_) {impl_->cancelled=true;impl_->state=IntroAudioStreamState::cancelled;}
}
IntroAudioStreamState IntroAudioStream::state() const noexcept {return impl_?impl_->state:IntroAudioStreamState::cancelled;}
std::string IntroAudioStream::error() const {return impl_?impl_->failure:std::string{};}
std::size_t IntroAudioStream::encoded_bytes_read() const noexcept {return impl_?impl_->bytes_read.load():0;}
} // namespace off::audio
