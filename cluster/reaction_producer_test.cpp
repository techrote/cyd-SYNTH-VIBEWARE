#include "reaction_producer.hpp"
#include "clock_sink.hpp"
#include <cstdio>
#include <cstdlib>

using namespace cluster::spatial::sound;
using Sink=cyd_platform::ClockSink;
using P=Producer<Sink>;
using R=P::Result;
static unsigned checks=0;
static void check(bool ok,const char* why){
    ++checks;
    if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}
}
int main(){
    Sink sink;
    P producer(sink);
    Voice reference;
    Image image{};
    for(unsigned i=0;i<image.size();++i)image[i]=uint8_t((i*37u)%256u);
    auto word=colour_word(focus_word(controls(image,2,true),80,190),3);
    std::array<int16_t,128> pcm{};
    check(producer.pump(0)==R::idle&&sink.tick()==128,"muted boot");
    check(producer.arm(word,0)&&!producer.arm(word,0),"explicit arm only");
    check(producer.pump(0)==R::queued&&sink.queued()==128,"first block");
    check(sink.tick()==128&&sink.queued()==128,"prime before consumption");
    check(producer.pump(0)==R::started&&sink.queued()==256,"second block starts");
    check(producer.pump(0)==R::idle,"bounded queue");
    uint32_t nonmid=0;
    // Matched logical sample clock: compare each encoded sample to actual Voice.
    // At each block, update the panel gesture for the next produced block.
    uint32_t expected_word=word, next_word=word;
    for(unsigned k=0;k<400;++k){
        reference.render(expected_word,pcm.data(),pcm.size());
        for(auto sample:pcm){
            const auto code=sink.tick();
            check(code==Sink::encode(sample),"ordered actual DSP samples");
            nonmid+=code!=128?1u:0u;
        }
        const uint32_t now=(k+1u)*128u/24u;
        word=colour_word(focus_word(controls(image,k%4u,true),k%257u,256u-k%257u),k%4u);
        check(producer.refresh(word,now),"fresh gesture");
        check(producer.pump(now)==R::queued&&sink.queued()==256,"one refill block");
        expected_word=next_word;next_word=word;
    }
    check(nonmid>1000,"audible codes");
    producer.mute();
    check(!producer.armed()&&!producer.arm(word,2200),"rearm waits for ack");
    check(!producer.refresh(word,2200)&&producer.pump(2200)==R::idle,"no silent rearm");
    check(sink.tick()==128&&sink.acknowledged()&&sink.queued()==0,"mute flush");
    check(producer.arm(word,0xfffffff0u),"wrap arm");
    check(producer.pump(0xfffffff0u)==R::queued&&producer.pump(0xfffffff0u)==R::started,"wrap prime");
    check(producer.pump(103)==R::idle,"lease valid at 119 ms");
    check(producer.pump(104)==R::expired&&!producer.armed(),"lease expires at 120 ms");
    check(sink.tick()==128&&sink.queued()==0,"lease flush");
    check(producer.arm(word,3000),"explicit post-expiry rearm");
    check(producer.pump(3000)==R::queued&&producer.pump(3000)==R::started,"starvation prime");
    for(unsigned i=0;i<256;++i)sink.tick();
    check(sink.tick()==128&&sink.faulted(),"starvation latch");
    check(producer.pump(3001)==R::fault&&!producer.armed(),"producer observes fault");
    check(!producer.arm(word,3001),"fault ack required");
    check(sink.tick()==128&&!sink.faulted()&&sink.queued()==0,"fault flush");
    check(producer.pump(3002)==R::idle,"no automatic restart");
    check(producer.arm(word,3003),"explicit fault rearm");
    check(producer.pump(3003)==R::queued&&producer.pump(3003)==R::started,"reprime");
    check(!producer.refresh(word,3123)&&!producer.armed(),"late refresh cannot renew expired lease");
    check(sink.tick()==128&&sink.queued()==0,"late refresh flush");
    std::printf("PASS producer checks=%u ordered_samples=51200 nonmid=%u producer_bytes=%zu sink_bytes=%zu\n",checks,nonmid,sizeof(producer),sizeof(sink));
}
