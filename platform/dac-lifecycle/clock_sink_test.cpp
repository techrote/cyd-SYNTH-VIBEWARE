#include "clock_sink.hpp"
#include "grimoire/panel.hpp"
#include "phosphor/board_session.hpp"
#include <cassert>
#include <cstdio>
#include <deque>
#include <chrono>
#include <vector>
using cyd_platform::ClockSink;
using Block=std::array<int16_t,128>;
using Clock=std::chrono::steady_clock;
struct Stats {
    uint64_t samples=0, nonmid=0, pixels=0, hash=0;
    std::vector<long long> ns;
};
// Host-only logical sample clock; expected queue holds independent PCM codes.
struct Witness {
    ClockSink sink;
    std::deque<uint8_t> expected;
    Stats stats;
    void enqueue(const Block& pcm) {
        assert(sink.push(pcm.data()));
        for(auto v:pcm)expected.push_back(uint8_t((32768+int(v)/2)>>8));
    }
    void consume(unsigned n) {
        assert(expected.size()>=n);
        while(n--){const auto v=sink.tick();assert(v==expected.front());expected.pop_front();++stats.samples;stats.nonmid+=v!=128;}
    }
    void flush() {
        assert(!expected.empty());
        const auto residual=std::count_if(expected.begin(),expected.end(),[](uint8_t v){return v!=128;});
        assert(residual>0);std::printf("pre-flush queued nonmidpoint codes=%zu\n",size_t(residual));
        sink.invalidate();expected.clear();
        assert(!sink.acknowledged());assert(!sink.start());
        Block stale{};stale.fill(16000);assert(!sink.push(stale.data()));
        assert(sink.tick()==128&&sink.acknowledged()&&sink.queued()==0);
        for(int i=0;i<128;++i)assert(sink.tick()==128);
    }
    void raster(const uint16_t* p) {
        for(int i=0;i<320*4;++i)stats.hash=stats.hash*131u+p[i];
        stats.pixels+=320*4;
    }
    void report(const char* label) {
        assert(stats.nonmid>1000&&stats.hash!=0);
        std::sort(stats.ns.begin(),stats.ns.end());
        const auto n=stats.ns.size();
        std::printf("%s samples=%llu nonmid=%llu pixels=%llu hash=%llu host raster+hash batch ns p50/p95/p99/max=%lld/%lld/%lld/%lld\n",label,(unsigned long long)stats.samples,(unsigned long long)stats.nonmid,(unsigned long long)stats.pixels,(unsigned long long)stats.hash,stats.ns[n/2],stats.ns[n*95/100],stats.ns[n*99/100],stats.ns.back());
    }
};
void grimoire_test(){
    grimoire::BoardSession session;grimoire::PanelRaster panel;Witness w;
    Block pcm{};std::array<uint16_t,1280> pixels{};
    assert(session.arm());session.poll(true,false,0,0,0);
    session.poll(true,true,80,100,0);
    int scan=240;
    for(unsigned k=0;k<1500;++k){
        const unsigned ms=k*128/24;
        if(k%3==0)assert(!session.poll(true,true,80+int(k%200),100+int(k%70),ms));
        assert(!session.audio(ms,pcm.data(),pcm.size()));w.enqueue(pcm);
        if(k==1)assert(w.sink.start());
        if(k>=1)w.consume(128);
        const auto t=Clock::now();
        for(int j=0;j<6;++j){if(scan==240){panel.begin(session.frame());scan=0;}assert(panel.render(scan,4,pixels.data()));w.raster(pixels.data());scan+=4;}
        w.stats.ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count());
    }
    // Real drag into Panic while a previous musical block is still queued.
    assert(session.poll(true,true,300,10,8000));w.flush();
    assert(!session.audio(8000,pcm.data(),pcm.size()));for(auto v:pcm)assert(v==0);
    session.poll(true,false,0,0,8001);session.poll(true,true,120,100,8002);
    assert(!session.audio(8002,pcm.data(),pcm.size()));w.enqueue(pcm);
    assert(!session.audio(8007,pcm.data(),pcm.size()));w.enqueue(pcm);
    assert(w.sink.start());w.consume(256);
    w.report("Grimoire");
}
void phosphor_test(){
    phosphor::BoardSession session;Witness w;Block pcm{};std::array<uint16_t,1280> pixels{};
    session.poll(true,false,0,0,0);session.poll(true,true,20,10,0);
    session.poll(true,false,0,0,1);session.poll(true,true,100,100,2);
    int scan=240;
    for(unsigned k=0;k<1500;++k){
        const uint64_t us=2+uint64_t(k)*16000/3;
        if(k%3==0)session.poll(true,true,80+int(k%200),100+int(k%70),us);
        session.advance(us);assert(session.digital(us,pcm.data()));w.enqueue(pcm);
        if(k==1)assert(w.sink.start());
        if(k>=1)w.consume(128);
        const auto t=Clock::now();
        for(int j=0;j<6;++j){if(scan==240){session.capture();scan=0;}assert(session.strip(scan,4,pixels.data()));w.raster(pixels.data());scan+=4;}
        w.stats.ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count());
    }
    // Reset generation, not just muted bit: an immediate scene selection can
    // clear muted before the next audio block. Observe after every UI mutation.
    auto epoch=session.visual().audio_control().reset;
    session.poll(true,true,300,230,8000002);
    assert(session.visual().muted()&&session.visual().audio_control().reset!=epoch);
    w.flush();session.poll(true,false,0,0,8000003);
    session.poll(true,true,120,10,8000004);assert(!session.visual().muted());
    assert(session.visual().audio_control().reset!=epoch);assert(w.sink.tick()==128);
    session.poll(true,false,0,0,8000005);session.poll(true,true,160,110,8000006);
    for(unsigned k=1500;k<1502;++k){const uint64_t us=6+uint64_t(k)*16000/3;session.advance(us);assert(session.digital(us,pcm.data()));w.enqueue(pcm);}
    assert(w.sink.start());w.consume(256);
    w.report("PHOSPHOR");
}
void starvation(){
    ClockSink s;Block pcm{};pcm.fill(16384);assert(!s.start());assert(s.push(pcm.data()));assert(!s.start());assert(s.push(pcm.data()));assert(s.start());
    for(unsigned i=0;i<256;++i)assert(s.tick()==160);
    assert(s.tick()==128&&s.faulted());assert(!s.push(pcm.data())&&!s.start());
    for(unsigned i=0;i<100;++i)assert(s.tick()==128);
    s.invalidate();assert(!s.push(pcm.data()));assert(s.tick()==128&&!s.faulted());
    assert(s.push(pcm.data()));assert(s.push(pcm.data()));assert(s.push(pcm.data()));assert(s.push(pcm.data()));assert(!s.push(pcm.data()));
    assert(s.start());assert(s.tick()==160);s.invalidate();s.invalidate();assert(s.tick()==128&&s.queued()==0);
    assert(!s.start());for(unsigned i=0;i<100;++i)assert(s.tick()==128);
    for(int i=-32768;i<32768;++i)assert(ClockSink::encode(int16_t(i))==uint8_t((32768+i/2)>>8));
    std::printf("PASS starvation, full queue, repeated reset, 65536 DAC encoding values, sink bytes=%zu\n",sizeof(s));
}
int main(){starvation();grimoire_test();phosphor_test();}
