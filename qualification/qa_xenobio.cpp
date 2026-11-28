// Independent QA: use exact XENOBIO M2/M3 portable runtime, not a substitute DSP.
#include "runtime.hpp"
#include <array>
#include <cassert>
#include <cstdio>
using namespace xenobio;
using Block=std::array<int16_t,128>;
static bool silent(const Block& b){return std::all_of(b.begin(),b.end(),[](int16_t v){return v==0;});}
static void arm(Runtime& r){assert(r.input.down(75,10));r.input.up();}
static Block sounding(Runtime& r){
    arm(r); Block b{};
    for(int i=0;i<400;++i){r.render(b.data(),b.size());if(!silent(b))return b;}
    assert(false&&"dense scene must produce actual PCM");return b;
}
int main(){
    unsigned interior=0,boundary=0;
    // Full in-bounds panic rectangle plus one-pixel left/top perimeter.
    for(int y=219;y<240;++y)for(int x=279;x<320;++x){
        Runtime r;const auto prior=sounding(r);assert(!silent(prior));
        assert(r.input.down(100,100));
        const bool panic=x>=280&&y>=220;
        assert(r.input.move(x,y)==!panic);
        Block b{};r.render(b.data(),b.size());
        assert(r.snapshot().panic==panic);
        if(panic){
            ++interior;assert(silent(b));assert(!r.input.move(75,10));
            r.input.up();arm(r);r.render(b.data(),b.size());assert(!r.snapshot().panic);
        }else{++boundary;r.input.up();}
    }
    // Exercise every nonempty queue occupancy prior to emergency.
    unsigned occupancies=0;
    for(int n=1;n<=32;++n){
        Runtime r;sounding(r);assert(r.input.down(100,100));
        for(int i=1;i<n;++i)assert(r.input.move(100+i,100));
        assert(!r.input.move(319,239));Block b{};r.render(b.data(),b.size());
        assert(silent(b)&&r.snapshot().panic);assert(!r.input.move(75,10));
        r.input.up();arm(r);r.render(b.data(),b.size());assert(!r.snapshot().panic);++occupancies;
    }
    // Model only the exact M2 windows.cpp pre-loop pending test and render order.
    // A driver queue is represented by copies of real, previously generated PCM.
    // No Win32 driver, physical latency, or actual playback is inferred.
    Runtime r;const Block queued=sounding(r);Block next{};
    const bool reset_at_loop_entry=r.input.emergency_pending();assert(!reset_at_loop_entry);
    r.input.emergency(); // input arrives after the backend's once-per-loop check
    assert(r.input.emergency_pending());r.render(next.data(),next.size());
    assert(silent(next)&&r.snapshot().panic);
    const bool reset_at_next_loop=r.input.emergency_pending();assert(!reset_at_next_loop);
    assert(!silent(queued)); // old hardware-buffer payload was never reset in this ordering
    std::printf("PASS %u panic pixels, %u adjacent pixels, %u queue occupancies; reproduced pending/ack backend reset gap with real PCM (modeled scheduling only)\n",interior,boundary,occupancies);
}
