#include "off/audio/intro_audio_stream.hpp"
#include "off/audio/decode.hpp"
#include "off/data/archive_vfs.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <vorbis/vorbisenc.h>

namespace {
using namespace off::audio;
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F f) {bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}check(caught,"expected rejection");}
std::vector<std::byte> encode(int channels,int rate,int frames,int serial) {
  vorbis_info info{}; vorbis_info_init(&info);
  check(vorbis_encode_init_vbr(&info,channels,rate,.8F)==0,"encoder initialization");
  vorbis_comment comment{};vorbis_comment_init(&comment);
  vorbis_dsp_state dsp{};vorbis_block block{};ogg_stream_state stream{};
  check(vorbis_analysis_init(&dsp,&info)==0 && vorbis_block_init(&dsp,&block)==0 &&
        ogg_stream_init(&stream,serial)==0,"encoder state");
  std::vector<std::byte> result;
  const auto append=[&](ogg_page& page) {
    const auto h=std::as_bytes(std::span(page.header,std::size_t(page.header_len)));
    const auto b=std::as_bytes(std::span(page.body,std::size_t(page.body_len)));
    result.insert(result.end(),h.begin(),h.end());result.insert(result.end(),b.begin(),b.end());
  };
  ogg_packet a{},b{},c{};vorbis_analysis_headerout(&dsp,&comment,&a,&b,&c);
  ogg_stream_packetin(&stream,&a);ogg_stream_packetin(&stream,&b);ogg_stream_packetin(&stream,&c);
  ogg_page page{};while(ogg_stream_flush(&stream,&page)) append(page);
  std::uint32_t noise=0x12345678;
  for(int position=0;position<=frames;) {
    const int count=std::min(1024,frames-position);
    auto** pcm=vorbis_analysis_buffer(&dsp,count);
    for(int i=0;i<count;++i) for(int channel=0;channel<channels;++channel) {
      noise=noise*1664525U+1013904223U;
      pcm[channel][i]=(float(noise>>8)/16777216.F-.5F)*.8F;
    }
    vorbis_analysis_wrote(&dsp,count);
    while(vorbis_analysis_blockout(&dsp,&block)==1) {
      vorbis_analysis(&block,nullptr);vorbis_bitrate_addblock(&block);
      ogg_packet packet{};
      while(vorbis_bitrate_flushpacket(&dsp,&packet)) {
        ogg_stream_packetin(&stream,&packet);
        while(ogg_stream_pageout(&stream,&page)) append(page);
      }
    }
    if(!count) break;
    position+=count;
  }
  ogg_stream_clear(&stream);vorbis_block_clear(&block);vorbis_dsp_clear(&dsp);
  vorbis_comment_clear(&comment);vorbis_info_clear(&info);return result;
}
IntroAudioStreamState wait(IntroAudioStream& stream) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
  for(;;) {
    const auto state=stream.poll();
    if(state!=IntroAudioStreamState::initial_pending && state!=IntroAudioStreamState::pcm_pending) return state;
    check(std::chrono::steady_clock::now()<deadline,"worker deadline");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
struct Directory {
  std::filesystem::path path=std::filesystem::current_path()/
      ("intro-stream-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Directory(){check(std::filesystem::create_directory(path),"unique test directory");}
  ~Directory(){std::error_code e;std::filesystem::remove_all(path,e);}
};
void save(const std::filesystem::path& path,std::span<const std::byte> bytes) {
  std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
  check(bool(file),"fixture write");
}
off::data::AudioStreamRecord metadata(std::size_t bytes,int channels,int rate,int frames) {
  return {.format_flags=0x80001000U,.sample_rate=std::uint32_t(rate),.bits_per_sample=16,
    .decoded_byte_count=std::uint32_t(frames*channels*2),.encoded_size=std::uint32_t(bytes),
    .channels=std::uint32_t(channels),.data_offset=13,.sample_value_count=std::uint32_t(frames*channels),
    .block_align=std::uint32_t(channels*2),.samples_per_block=1};
}
}
int main() {
 try {
  Directory directory;
  for(int channels:{1,2}) {
    const int frames=channels==1?257:200000,rate=channels==1?22050:48000;
    auto encoded=encode(channels,rate,frames,50+channels);
    if(channels==2) check(encoded.size()>65536,"fixture crosses initial windows");
    auto record=metadata(encoded.size(),channels,rate,frames);
    std::vector<std::byte> file(13,std::byte{0x77});file.insert(file.end(),encoded.begin(),encoded.end());
    file.insert(file.end(),17,std::byte{0xee});save(directory.path/"bank.wav",file);
    off::data::ArchiveVfs vfs;(void)vfs.mount_directory(directory.path);
    const auto open=[&](off::data::AudioStreamRecord r){return IntroAudioStream(r,vfs.open_stream("bank.wav").open_reader());};
    auto stream=open(record);
    rejects([&]{stream.observe_initial_completion();});rejects([&]{stream.request_pcm(1);});
    stream.issue_initial_read();
    check(stream.state()==IntroAudioStreamState::initial_pending,"issue never publishes completion");
    IntroAudioStream moved=std::move(stream);
    check(stream.state()==IntroAudioStreamState::cancelled,"moved-from stream inert");
    check(wait(moved)==IntroAudioStreamState::initial_completed,"real initial read completion");
    check(moved.encoded_bytes_read()==std::min<std::size_t>(65536,encoded.size()),"bounded exact initial bytes");
    rejects([&]{moved.request_pcm(1);});moved.observe_initial_completion();
    rejects([&]{moved.request_pcm(0);});rejects([&]{moved.request_pcm(65537);});
    std::vector<std::int16_t> decoded;
    while(moved.state()!=IntroAudioStreamState::ended) {
      moved.request_pcm(channels==1?17:4093);
      check(wait(moved)==IntroAudioStreamState::pcm_completed,moved.error().c_str());
      auto chunk=moved.take_pcm();decoded.insert(decoded.end(),chunk.interleaved_samples.begin(),chunk.interleaved_samples.end());
    }
    check(decoded==decode_bank_stream(record,encoded).interleaved_samples,"incremental PCM matches independent whole-stream decode");
    check(moved.encoded_bytes_read()==encoded.size(),"sequential refills consume exact selected range");
    auto cancelled=open(record);cancelled.issue_initial_read();cancelled.cancel();
    check(cancelled.poll()==IntroAudioStreamState::cancelled,"cancel does not publish stale completion");
    auto wrong=record;wrong.sample_value_count+=channels;wrong.decoded_byte_count+=channels*2;
    auto bad=open(wrong);bad.issue_initial_read();check(wait(bad)==IntroAudioStreamState::initial_completed,"bad count still permits real encoded fill");
    bad.observe_initial_completion();
    while(bad.state()==IntroAudioStreamState::ready) {
      bad.request_pcm(65536);if(wait(bad)==IntroAudioStreamState::failed) break;(void)bad.take_pcm();
    }
    check(bad.state()==IntroAudioStreamState::failed && !bad.error().empty(),"EOF requires exact meaningful count");
    wrong=record;wrong.sample_rate=0;rejects([&]{(void)open(wrong);});
    wrong=record;wrong.encoded_size=std::uint32_t(file.size());rejects([&]{(void)open(wrong);});
    if(channels==1) {
      const auto fails_decode=[&](std::vector<std::byte> payload) {
        std::vector<std::byte> contents(13);
        contents.insert(contents.end(),payload.begin(),payload.end());
        save(directory.path/"invalid.wav",contents);
        off::data::ArchiveVfs invalid_vfs;(void)invalid_vfs.mount_directory(directory.path);
        auto metadata_record=record;metadata_record.encoded_size=std::uint32_t(payload.size());
        IntroAudioStream invalid(metadata_record,invalid_vfs.open_stream("invalid.wav").open_reader());
        invalid.issue_initial_read();check(wait(invalid)==IntroAudioStreamState::initial_completed,"invalid codec does not fabricate I/O failure");
        invalid.observe_initial_completion();
        while(invalid.state()==IntroAudioStreamState::ready) {
          invalid.request_pcm(65536);if(wait(invalid)==IntroAudioStreamState::failed) break;(void)invalid.take_pcm();
        }
        check(invalid.state()==IntroAudioStreamState::failed && !invalid.error().empty(),"corrupt/chained stream is terminal failure");
      };
      auto chained=encoded;auto next=encode(1,rate,257,99);
      chained.insert(chained.end(),next.begin(),next.end());fails_decode(std::move(chained));
      auto empty_chain=encoded;auto empty_link=encode(1,rate,0,100);
      empty_chain.insert(empty_chain.end(),empty_link.begin(),empty_link.end());
      fails_decode(std::move(empty_chain));
      fails_decode(std::vector<std::byte>(encoded.size(),std::byte{0x71}));
      fails_decode(std::vector<std::byte>(encoded.begin(),encoded.begin()+encoded.size()/2));
      save(directory.path/"short.wav",file);
      off::data::ArchiveVfs short_vfs;(void)short_vfs.mount_directory(directory.path);
      IntroAudioStream short_read(record,short_vfs.open_stream("short.wav").open_reader());
      std::filesystem::resize_file(directory.path/"short.wav",15);
      short_read.issue_initial_read();
      check(wait(short_read)==IntroAudioStreamState::failed,"short real I/O cannot publish completion");
      rejects([&]{short_read.observe_initial_completion();});
    }
  }
  std::cout<<"Owned two-window reads, incremental varied Vorbis PCM, refills, completion observation, moves and cancellation verified.\n";
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
