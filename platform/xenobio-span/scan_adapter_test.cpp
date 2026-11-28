#include "runtime.hpp"
#include "scan_adapter.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
using namespace xenobio;
using namespace cyd::platform;
struct Pixel { uint16_t v=0; Pixel& operator=(uint16_t x) {v=x;return *this;} };
static_assert(sizeof(Pixel)==2);
struct Sink {
    std::array<uint16_t,320*240> frame{};
    unsigned calls=0;
    bool fail=false;
    bool operator()(int y,const Pixel* data) {
        assert(y>=0&&y<=236&&y%4==0); ++calls;
        for(int i=0;i<1280;++i) frame[y*320+i]=data[i].v;
        return !fail;
    }
    void check(const Engine& e) const {
        std::array<uint16_t,320> row{};
        for(int y=0;y<240;++y) {
            labeled_row(e,y,row.data());
            assert(std::equal(row.begin(),row.end(),frame.begin()+y*320));
        }
    }
};
template<XenobioScanMode M> void run() {
    XenobioScanAdapter<M,Pixel> adapter;
    Runtime a,b; Sink sink;
    std::array<int16_t,1200> pcm{},reference{};
    uint64_t samples=0,ns=0; unsigned frames=0,nonzero=0;
    auto gesture=[&](int x,int y) {assert(a.input.down(x,y));assert(b.input.down(x,y));a.input.up();b.input.up();};
    for(int scene=0;scene<6;++scene) {
        gesture(scene*320/6+2,5);
        for(int t=0;t<240;++t) {
            if(t%31==0) gesture((t*13)%320,20+(t*7)%200);
            a.render(pcm.data(),pcm.size()); b.render(reference.data(),reference.size());
            assert(pcm==reference); samples+=pcm.size();
            nonzero+=std::count_if(pcm.begin(),pcm.end(),[](int16_t x){return x!=0;});
            const auto expected=a.snapshot(); unsigned reads=0;
            const uint32_t now=50*(1+scene*240+t);
            auto snapshot=[&](){++reads;return expected;};
            const auto start=std::chrono::steady_clock::now();
            auto r=adapter.step(now,snapshot,sink);
            while(r==ScanStep::Submitted) r=adapter.step(now,snapshot,sink);
            ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
            assert(r==ScanStep::Complete&&reads==1); sink.check(expected); ++frames;
            assert(adapter.step(now+49,snapshot,sink)==ScanStep::Idle&&reads==1);
        }
    }
    const auto calls=sink.calls;
    // Force recovery from a failed (possibly partially applied) transfer.
    auto snapshot=[&](){return a.snapshot();};
    adapter.invalidate();sink.fail=true;
    assert(adapter.step(72100,snapshot,sink)==ScanStep::Failed&&!adapter.pending());
    sink.fail=false;unsigned before=sink.calls;
    assert(adapter.step(72149,snapshot,sink)==ScanStep::Idle);
    auto r=adapter.step(72150,snapshot,sink);
    // New engine state cannot overwrite the in-progress scan.
    auto expected=a.snapshot();a.input.emergency();a.render(pcm.data(),pcm.size());
    while(r==ScanStep::Submitted) r=adapter.step(72150,snapshot,sink);
    assert(r==ScanStep::Complete&&sink.calls-before==60);sink.check(expected);
    // Millisecond counter wrap does not break the cadence.
    r=adapter.step(UINT32_MAX-20,snapshot,sink);
    while(r==ScanStep::Submitted) r=adapter.step(UINT32_MAX-20,snapshot,sink);
    assert(r==ScanStep::Complete);sink.check(a.snapshot());
    assert(adapter.step(28,snapshot,sink)==ScanStep::Idle);
    r=adapter.step(29,snapshot,sink);
    while(r==ScanStep::Submitted) r=adapter.step(29,snapshot,sink);
    assert(r==ScanStep::Complete);sink.check(a.snapshot());
    assert(nonzero>0);
    std::printf("mode=%d frames=%u pixels=%llu samples=%llu nonzero=%u strips=%u state=%zu scan_ms=%.3f PASS\n",int(M),frames,static_cast<unsigned long long>(frames)*76800,static_cast<unsigned long long>(samples),nonzero,calls,sizeof(adapter),ns/1e6);
}
int main(){run<XenobioScanMode::Original>();run<XenobioScanMode::Span>();run<XenobioScanMode::Selective>();}
