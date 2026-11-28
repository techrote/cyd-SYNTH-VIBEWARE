#include "runtime.hpp"
#include "labeled.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>
using namespace xenobio;
using PCM = std::array<int16_t,128>;
static bool silent(const PCM& p) { return std::all_of(p.begin(),p.end(),[](int v){return v==0;}); }
static void tap(Runtime& r,int x,int y) { assert(r.input.down(x,y)); r.input.up(); }
static void ppm(const char* path,const Engine& e) {
    std::ofstream f(path,std::ios::binary); f<<"P6\n320 240\n255\n";
    for(int y=0;y<240;++y){uint16_t row[322]{};row[0]=row[321]=0xdead;labeled_row(e,y,row+1);
        assert(row[0]==0xdead&&row[321]==0xdead);
        for(int x=1;x<=320;++x){auto c=row[x];f.put(char((c>>11)*255/31));f.put(char(((c>>5)&63)*255/63));f.put(char((c&31)*255/31));}}
    assert(f.good());
}
int main(int argc,char** argv) {
    assert(argc==2);Runtime r;PCM p{},q{};Engine direct;direct.reset(2);direct.stop();
    r.render(p.data(),p.size());assert(silent(p)&&r.snapshot().panic);
    tap(r,120,10);direct.touch(120,10,true);direct.release();
    uint64_t compared=0;bool sounded=false;
    for(int b=0;b<4500;++b){
        if(b==200){tap(r,100,230);direct.touch(100,230,true);direct.release();assert(r.input.down(120,100));direct.touch(120,100,true);}
        if(b>=201&&b<260&&b%4==0){assert(r.input.move(200,140));direct.touch(200,140,false);}
        if(b==260){r.input.up();direct.release();}
        r.render(p.data(),p.size());for(auto& s:q)s=direct.sample();assert(p==q);compared+=p.size();sounded|=!silent(p);
    }
    assert(sounded);auto e=r.snapshot();assert(e.food==direct.food&&e.ticks==direct.ticks&&e.notes==direct.notes);
    ppm(argv[1],e);
    // Drag panic bypasses queued reset/movement; release+ack and fresh down needed.
    assert(r.input.down(120,100));assert(r.input.move(180,100));assert(!r.input.move(319,239));
    assert(!r.input.move(120,10));r.render(p.data(),p.size());assert(silent(p)&&r.snapshot().panic);
    assert(!r.input.down(120,10));r.input.up();tap(r,120,10);r.render(p.data(),p.size());assert(!r.snapshot().panic);
    // Full queue cannot swallow safety state, including a dropped Up.
    assert(r.input.down(100,100));for(int i=0;i<31;++i)assert(r.input.move(100+i,100));r.input.up();
    assert(r.input.emergency_pending());assert(!r.input.down(120,10));r.input.up();
    r.render(p.data(),p.size());assert(silent(p)&&r.snapshot().panic);tap(r,120,10);r.render(p.data(),p.size());assert(!r.snapshot().panic);
    assert(r.input.down(100,100));r.input.cancel();assert(!r.input.move(120,120));r.render(p.data(),p.size());assert(silent(p)&&r.snapshot().panic);
    tap(r,120,10);r.render(p.data(),p.size());assert(!r.snapshot().panic);
    assert(r.input.down(100,100));for(int b=0;b<40;++b)r.render(p.data(),p.size());assert(!r.snapshot().held);r.input.up();
    // Actual-core render labels leave the acoustic field bit-exact.
    uint16_t a[320],b[320];unsigned changed=0;
    for(int y=0;y<240;++y){e.render(y,a);labeled_row(e,y,b);for(int x=0;x<320;++x){if(y>=20&&y<220)assert(a[x]==b[x]);changed+=a[x]!=b[x];}}
    assert(changed>100);
    // SPSC FIFO concurrent test with no overflow: one in-flight contact at a time.
    Controls c;std::atomic<unsigned> seen{0};std::thread producer([&]{for(unsigned i=0;i<50000;++i){while(seen.load(std::memory_order_acquire)!=2*i)std::this_thread::yield();assert(c.down(int(i%280),100));c.up();}});
    for(unsigned i=0;i<100000;++i){Input in{};while(!c.pop(in))std::this_thread::yield();assert(in.kind==(i%2?Contact::Up:Contact::Down));if(!(i%2))assert(in.x==int((i/2)%280));seen.store(i+1,std::memory_order_release);}producer.join();
    // Triple-buffer test: every read is a coherent complete actual Engine value.
    Engine seed;seed.reset(2);Snapshots snaps(seed);std::atomic<bool> done{false};
    std::thread writer([&]{for(unsigned i=1;i<=100000;++i){seed.ticks=i;seed.notes=i*3;seed.food.fill(uint8_t(i%101));snaps.publish(seed);}done.store(true,std::memory_order_release);});
    unsigned reads=0,last=0;do{auto s=snaps.read();if(s.ticks){assert(s.ticks>=last&&s.notes==s.ticks*3);for(auto f:s.food)assert(f==s.ticks%101);last=s.ticks;++reads;}}while(!done.load(std::memory_order_acquire)||last!=100000);writer.join();assert(reads>0);
    // Matched worst-case dense-scene host block costs; not ESP32 measurements.
    Runtime dense;tap(dense,75,10);std::vector<long long> times;times.reserve(12000);
    for(int i=0;i<12000;++i){auto t=std::chrono::steady_clock::now();dense.render(p.data(),p.size());auto end=std::chrono::steady_clock::now();times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end-t).count());}
    std::sort(times.begin(),times.end());
    std::printf("PASS %llu exact PCM; 100000 FIFO events; 100000 publications (%u reads); %u label pixels changed; Runtime=%zu B\n",(unsigned long long)compared,reads,changed,sizeof(Runtime));
    std::printf("Dense host 12000 x 128-sample blocks: p50=%lld ns p95=%lld ns p99=%lld ns max=%lld ns\n",times[5999],times[11399],times[11879],times.back());
}
