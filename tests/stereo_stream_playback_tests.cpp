#include "off/audio/stereo_stream_playback.hpp"
#include "off/audio/decode.hpp"
#include "off/data/archive_vfs.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vorbis/vorbisenc.h>

namespace {
using namespace off::audio;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F operation) {
  bool rejected=false; try {operation();} catch(const std::exception&) {rejected=true;}
  check(rejected,"expected explicit playback rejection");
}
// Independently generated noise fixture, using the same public encoder workflow
// as intro_audio_stream_tests. No retail audio or real playback device is used.
std::vector<std::byte> encode(int frames) {
  vorbis_info info{}; vorbis_info_init(&info);
  check(vorbis_encode_init_vbr(&info,2,44100,.8F)==0,"encoder initialization");
  vorbis_comment comment{}; vorbis_comment_init(&comment);
  vorbis_dsp_state dsp{}; vorbis_block block{}; ogg_stream_state stream{};
  check(vorbis_analysis_init(&dsp,&info)==0 && vorbis_block_init(&dsp,&block)==0 &&
        ogg_stream_init(&stream,117)==0,"encoder state");
  std::vector<std::byte> result;
  const auto append=[&](ogg_page& page) {
    const auto h=std::as_bytes(std::span(page.header,std::size_t(page.header_len)));
    const auto b=std::as_bytes(std::span(page.body,std::size_t(page.body_len)));
    result.insert(result.end(),h.begin(),h.end()); result.insert(result.end(),b.begin(),b.end());
  };
  ogg_packet a{},b{},c{}; vorbis_analysis_headerout(&dsp,&comment,&a,&b,&c);
  ogg_stream_packetin(&stream,&a); ogg_stream_packetin(&stream,&b); ogg_stream_packetin(&stream,&c);
  ogg_page page{}; while(ogg_stream_flush(&stream,&page)) append(page);
  std::uint32_t noise=0x12345678;
  for(int position=0;position<=frames;) {
    const int count=std::min(1024,frames-position);
    auto** pcm=vorbis_analysis_buffer(&dsp,count);
    for(int i=0;i<count;++i) for(int channel=0;channel<2;++channel) {
      noise=noise*1664525U+1013904223U;
      pcm[channel][i]=(float(noise>>8)/16777216.F-.5F)*.8F;
    }
    vorbis_analysis_wrote(&dsp,count);
    while(vorbis_analysis_blockout(&dsp,&block)==1) {
      vorbis_analysis(&block,nullptr); vorbis_bitrate_addblock(&block);
      ogg_packet packet{};
      while(vorbis_bitrate_flushpacket(&dsp,&packet)) {
        ogg_stream_packetin(&stream,&packet);
        while(ogg_stream_pageout(&stream,&page)) append(page);
      }
    }
    if(!count) break;
    position+=count;
  }
  ogg_stream_clear(&stream); vorbis_block_clear(&block); vorbis_dsp_clear(&dsp);
  vorbis_comment_clear(&comment); vorbis_info_clear(&info); return result;
}
struct Fixture {
  std::filesystem::path path=std::filesystem::current_path()/
      ("stereo-playback-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::vector<std::byte> encoded=encode(70001);
  off::data::AudioStreamRecord record{.format_flags=0x80001000U,.sample_rate=44100,
    .bits_per_sample=16,.decoded_byte_count=70001*4,.encoded_size=std::uint32_t(encoded.size()),
    .channels=2,.data_offset=13,.sample_value_count=70001*2,.block_align=4,.samples_per_block=1};
  off::data::ArchiveVfs vfs;
  Fixture() {
    check(std::filesystem::create_directory(path),"fixture directory");
    const auto save=[&](const char* name,bool corrupt) {
      std::ofstream file(path/name,std::ios::binary);
      const std::array<char,13> prefix{}; file.write(prefix.data(),prefix.size());
      if(corrupt) {const std::vector<char> bytes(encoded.size(),'x'); file.write(bytes.data(),std::streamsize(bytes.size()));}
      else file.write(reinterpret_cast<const char*>(encoded.data()),std::streamsize(encoded.size()));
      check(bool(file),"fixture file write");
    };
    save("good.wav",false); save("bad.wav",true); (void)vfs.mount_directory(path);
  }
  ~Fixture() {std::error_code error;std::filesystem::remove_all(path,error);}
  IntroAudioStreamSource source(bool corrupt=false) {
    return {record,vfs.open_stream(corrupt?"bad.wav":"good.wav").open_reader()};
  }
};
struct Capture {
  std::vector<std::int16_t> samples;
  std::vector<std::string> operations;
  std::size_t queued{},limit{},starts{},pauses{},stops{},flushes{};
  std::string failure;
  std::function<void(std::string_view)> callback;
  bool destroyed{};
  void operation(const char* name) {
    operations.emplace_back(name);
    if(callback) callback(name);
    if(failure==name) throw std::runtime_error("synthetic output failure");
  }
};
// Explicit synthetic recording double: queue progress occurs ONLY when the
// test changes queued. Its start result is not evidence of device playback.
class RecordingOutput final : public StereoPcmOutput {
  std::shared_ptr<Capture> c_;
public:
  explicit RecordingOutput(std::shared_ptr<Capture> c):c_(std::move(c)) {}
  ~RecordingOutput() override {c_->destroyed=true;}
  void submit(std::span<const std::int16_t> samples) override {
    c_->operation("submit"); check(c_->queued+samples.size_bytes()<=c_->limit,"queue overrun");
    c_->samples.insert(c_->samples.end(),samples.begin(),samples.end()); c_->queued+=samples.size_bytes();
  }
  std::size_t queued_input_bytes() const override {
    if(c_->callback) c_->callback("query");
    return c_->queued;
  }
  void start() override {c_->operation("start");check(!c_->samples.empty(),"start before PCM");++c_->starts;}
  void pause() override {c_->operation("pause");++c_->pauses;}
  void stop() override {c_->operation("stop");++c_->stops;c_->queued=0;}
  void flush() override {c_->operation("flush");++c_->flushes;}
  void set_volume_hundredths_db(std::int32_t) override {c_->operation("volume");}
  void set_frequency(std::uint32_t) override {c_->operation("frequency");}
  void set_pan(std::int32_t) override {c_->operation("pan");}
};
struct Harness {
  Fixture& fixture;
  std::vector<std::shared_ptr<Capture>> outputs;
  bool fail_open{},corrupt{};
  std::string output_failure;
  StereoStreamPlayback player;
  explicit Harness(Fixture& f,std::size_t capacity=1):fixture(f),player(
    [&](std::uint32_t link) {check(link==16 || link==32,"missing link");return fixture.source(corrupt);},
    [&](std::uint32_t rate,std::size_t bytes)->std::unique_ptr<StereoPcmOutput> {
      check(rate==44100 && bytes==2048*4,"factory format/queue contract");
      if(fail_open) throw std::runtime_error("synthetic open failure");
      auto c=std::make_shared<Capture>();c->limit=bytes;c->failure=output_failure;outputs.push_back(c);
      return std::make_unique<RecordingOutput>(c);
    },{capacity,2048,1024}) {}
  static StereoSourceCommand command(std::uint64_t binding=1) {
    return {binding,true,false,1000,16,-1,0,0,100};
  }
  void submit(StereoSourceCommand c=command()) {player.submit(std::span(&c,1));}
};
template<class F> void until(Harness& h,F predicate) {
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  while(!predicate()) {check(std::chrono::steady_clock::now()<end,"playback worker deadline");
    h.player.pump();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
void no_starts(Harness& h) {check(h.player.take_notifications().started.empty(),"unexpected start notification");}
}
int main() {
 try {
  Fixture fixture;
  const auto expected=decode_bank_stream(fixture.record,fixture.encoded).interleaved_samples;
  {
    Harness h(fixture);h.submit();
    check(h.outputs.size()==1 && h.outputs[0]->samples.empty(),"admission does not decode synchronously");
    no_starts(h);
    until(h,[&]{return h.outputs[0]->queued==8192;});
    auto c=h.outputs[0];check(c->starts==1,"initial start exactly once");
    check(h.player.take_notifications().started==std::vector<std::uint64_t>{1},"one initial start notification");
    for(int i=0;i<20;++i) h.player.pump();
    check(c->samples.size()==4096,"stalled output bounds decoder refill"); no_starts(h);
    auto update=Harness::command();h.submit(update);
    check(c->starts==1 && h.outputs.size()==1,"active start request neither reopens nor restarts");
    until(h,[&]{
      if(h.player.status(1)->input_ended) return true;
      c->queued=0; return false;
    });
    check(c->samples==expected,"bounded incremental decode equals independent offline PCM");
    check(c->flushes==1 && h.player.status(1)->submitted_frames==70001,"EOF flush/frame count");
    check(c->queued>0 && !c->destroyed,"EOF preserves queued tail");
    for(int i=0;i<10;++i) h.player.pump();
    no_starts(h);
    c->queued=0;h.player.pump();check(h.player.status(1).has_value(),"zero input queue does not fake hardware completion");
    h.player.stop(1);check(c->destroyed && c->stops==1 && !h.player.status(1),"explicit stop retires");
    check(h.player.take_notifications().stopped==std::vector<std::uint64_t>{1},"distinct stop notification");
  }
  {
    Harness h(fixture);h.submit();h.player.hold(1,true);h.player.hold(1,true);
    until(h,[&]{return h.outputs[0]->queued==8192;});
    auto c=h.outputs[0];check(c->starts==0 && c->pauses==0,"hold before initial PCM prevents native start");no_starts(h);
    h.submit();check(!h.player.status(1)->held && c->starts==0,"preinitial update clears hold without empty start");
    h.player.pump();check(c->starts==1,"PCM ready initial gate after hold release");
    h.player.hold(1,true);h.player.hold(1,true);check(c->pauses==1,"repeat hold idempotent");
    c->operations.clear();h.submit();
    check(c->operations==std::vector<std::string>{"start","pan","volume","frequency"},"resume before ordered controls");
    h.player.hold(1,false);check(c->starts==2,"repeat unhold does not resume again");
    h.player.stop(1);
    const auto notices=h.player.take_notifications();
    check(notices.started==std::vector<std::uint64_t>{1} && notices.stopped==std::vector<std::uint64_t>{1},
      "stop retains earlier pending start in separate notification sequence");
    auto c0=Harness::command();c0.start_requested=false;h.submit(c0);
    check(!h.player.status(1) && h.outputs.size()==1,"nonstart cannot resurrect stopped source");
    h.submit();until(h,[&]{return h.player.status(1)->started;});
    check(h.outputs.size()==2 && h.player.take_notifications().started==std::vector<std::uint64_t>{1},"new lifetime gets new start");
  }
  {
    Harness h(fixture);auto command=Harness::command();command.start_requested=false;h.submit(command);
    check(h.outputs.empty() && !h.player.status(1),"cold nonstart does not allocate");
    command.start_requested=true;h.submit(command);h.player.stop(1);
    check(!h.player.status(1) && h.outputs[0]->destroyed,"stop immediately cancels initial pending worker");
    const auto n=h.player.take_notifications();check(n.started.empty() && n.stopped.size()==1,"pending-read stop is not start");
    h.submit();rejects([&]{h.submit(Harness::command(2));});check(h.player.status(1).has_value(),"capacity rejection retains first source");
    no_starts(h);
  }
  for(const auto failure:{"pan","volume","frequency","start","submit"}) {
    Harness h(fixture);h.output_failure=failure;
    if(h.output_failure=="start" || h.output_failure=="submit") {
      h.submit();rejects([&]{until(h,[&]{return h.player.status(1)->started;});});
    } else rejects([&]{h.submit();});
    check(!h.player.status(1) && h.outputs[0]->destroyed,"failed output retires attempted source");no_starts(h);
  }
  {
    Harness h(fixture);h.fail_open=true;rejects([&]{h.submit();});
    check(h.outputs.empty() && !h.player.status(1),"open failure does not allocate");no_starts(h);
  }
  {
    Harness h(fixture);h.corrupt=true;h.submit();
    rejects([&]{until(h,[&]{return h.player.status(1)->started;});});
    check(!h.player.status(1),"decoder failure retires source");no_starts(h);
  }
  for(const auto failure:{"pause","start","stop","volume"}) {
    Harness h(fixture);h.submit();until(h,[&]{return h.player.status(1)->started;});
    (void)h.player.take_notifications();
    auto c=h.outputs[0];
    if(std::string_view(failure)=="start") h.player.hold(1,true);
    c->failure=failure;
    if(c->failure=="pause") rejects([&]{h.player.hold(1,true);});
    else if(c->failure=="start") rejects([&]{h.player.hold(1,false);});
    else if(c->failure=="stop") rejects([&]{h.player.stop(1);});
    else rejects([&]{h.submit();});
    check(!h.player.status(1) && c->destroyed,"started control failure retires channel");
    const auto notices=h.player.take_notifications();
    check(notices.started.empty() && notices.stopped.empty(),"failed operation invents no success notice");
  }
  {
    Harness h(fixture);h.submit();
    auto replacement=Harness::command();replacement.whd_offset=32;
    rejects([&]{h.submit(replacement);});
    check(h.outputs.size()==1 && !h.player.status(1),"unsupported replacement cannot reopen active decoder");
    no_starts(h);
    h.player.stop(123);h.player.hold(123,true);
    check(h.player.take_notifications().stopped.empty(),"missing stop/hold has no notification");
  }
  {
    Harness h(fixture,2);h.submit(Harness::command(20));h.submit(Harness::command(10));
    h.player.hold(20,true);h.player.hold(10,true);
    until(h,[&]{return h.outputs[0]->queued==8192 && h.outputs[1]->queued==8192;});
    h.player.hold(10,false);h.player.hold(20,false);h.player.pump();
    // Reverse release order must not override active channel slot order.
    check(h.player.take_notifications().started==std::vector<std::uint64_t>{20,10},
          "simultaneously ready channels start in slot order");
    h.player.stop(20);h.submit(Harness::command(30));
    until(h,[&]{return h.player.status(30)->started;});
    h.player.stop(10);h.player.stop(30);
    const auto n=h.player.take_notifications();
    check(n.started==std::vector<std::uint64_t>{30} && n.stopped==std::vector<std::uint64_t>{20,10,30},
          "slot reuse keeps one new start and ordered independent stops");
    const auto empty=h.player.take_notifications();
    check(empty.started.empty() && empty.stopped.empty(),"notification transfer clears both sequences");
  }
  {
    Harness h(fixture);h.submit();
    auto c=h.outputs[0];std::size_t callback_calls=0;
    c->callback=[&](std::string_view) {
      ++callback_calls;
      rejects([&]{h.player.stop(1);});
      rejects([&]{h.player.hold(1,true);});
      rejects([&]{h.player.pump();});
      rejects([&]{h.submit();});
      rejects([&]{(void)h.player.status(1);});
      rejects([&]{(void)h.player.take_notifications();});
    };
    h.submit(); // All control callbacks reject both recursive read and write.
    check(h.player.status(1).has_value(),"status query callback cannot invalidate active channel");
    until(h,[&]{return h.player.status(1)->started;});
    check(callback_calls>4 && c->starts==1,"guarded callbacks preserve initial start");
    c->callback={};
    check(h.player.take_notifications().started==std::vector<std::uint64_t>{1},"reentry does not duplicate notifications");
    c->callback=[&](std::string_view operation) {if(operation=="query") h.player.stop(1);};
    rejects([&]{(void)h.player.status(1);});
    c->callback={};
    check(h.player.status(1).has_value(),"query failure retains valid channel without reentrant destruction");
    c->callback=[&](std::string_view operation) {if(operation=="volume") (void)h.player.status(1);};
    rejects([&]{h.submit();});
    check(c->destroyed && !h.player.status(1),"uncaught control reentry retires safely");
    no_starts(h);
  }
  {
    Harness h(fixture);
    for(int mode=0;mode<5;++mode) {
      auto c=Harness::command();
      if(mode==0)c.whd_offset=0;
      if(mode==1)c.whd_offset=17;
      if(mode==2)c.whd_offset=48;
      if(mode==3)c.loop=true;
      if(mode==4)c.environment_group=0;
      rejects([&]{h.submit(c);});check(h.outputs.empty(),"invalid command cannot allocate output");
    }
    h.submit();until(h,[&]{return h.player.status(1)->started;});(void)h.player.take_notifications();
    h.outputs[0]->failure="submit";h.outputs[0]->queued=0;
    rejects([&]{until(h,[&]{return !h.player.status(1);});});
    check(!h.player.status(1),"refill failure retires started source");no_starts(h);
  }
  std::cout<<"Synthetic output sequencing and real generated Vorbis decoding verified; no device playback claim.\n";
 } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
