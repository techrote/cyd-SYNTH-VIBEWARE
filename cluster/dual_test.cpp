#include "dual.hpp"
#include <cstdlib>
#include <iostream>
#include <deque>
using namespace cluster;
using namespace cluster::dual;
unsigned checks=0;
void check(bool b){++checks;if(!b){std::cerr<<"FAIL check "<<checks<<'\n';std::exit(1);}}
struct Packet{Frame f;uint32_t due;bool to_leader;};
int main(){
    ClockMap c;
    check(c.sample(100,1110,1110,120));
    check(c.to_local(1300)==300);
    check(!c.sample(200,1200,1200,300));
    check(c.sample(1100,2111,2111,1120));
    check(c.ppm==1000);
    check(delta(c.to_local(3112),2110)==0);
    ClockMap wrap;
    check(wrap.sample(0xfffffff0u,90,90,4));
    check(wrap.to_local(100)==4);
    check(!wrap.fresh(3004));
    Peer leader(77,true), follower(77,false);
    leader.network(true);follower.network(true);
    std::deque<Packet> packets;
    uint32_t t=0;
    bool online=true;
    auto a=[&](const Frame& f){if(online)packets.push_back({f,t+8,false});return online;};
    auto b=[&](const Frame& f){if(online)packets.push_back({f,t+8,true});return online;};
    auto ft=[](uint32_t n){return 0xfffff000u+n+n/1000;};
    unsigned both=0,off=0;
    for(t=0;t<14000;++t){
        if(t==4500)online=false;
        if(t==8500)online=true;
        for(unsigned i=0;i<packets.size();) {
            if(packets[i].due>t){++i;continue;}
            auto p=packets[i];packets.erase(packets.begin()+i);
            if(p.to_leader)leader.receive(p.f,t,a);
            else {follower.receive(p.f,ft(t),b);if(t%3==0)follower.receive(p.f,ft(t),b);}
        }
        leader.tick(t,a);follower.tick(ft(t),b);
        if(t>1500&&t<4400&&leader.sound(t)&&follower.sound(ft(t)))++both;
        if(t>6000&&t<8400&&follower.sound(ft(t)))++off;
        check(follower.queued()<=4);check(leader.queued()<=1);
    }
    check(follower.stats.samples>=10);check(follower.stats.played>=20);
    check(follower.stats.local>=5);check(both>650);check(off>300);
    check(follower.stats.rejected>0);
    check(follower.stats.max_late_ms<=2);
    // Foreign epoch, impossible timestamps and wrong role must not enter the queue.
    auto wrong=frame(Kind::pulse_event,78,999);
    put32(wrong.bytes.data()+header,t+180);put32(wrong.bytes.data()+header+4,3);put32(wrong.bytes.data()+header+8,period);
    const auto rejected=follower.stats.rejected;
    follower.receive(wrong,ft(t),b);check(follower.stats.rejected==rejected+1);
    auto late=frame(Kind::pulse_event,77,999);
    put32(late.bytes.data()+header,t-100);put32(late.bytes.data()+header+4,3);put32(late.bytes.data()+header+8,period);
    follower.receive(late,ft(t),b);check(follower.stats.rejected==rejected+2);
    follower.network(false);check(follower.sound(ft(t))==0);check(follower.queued()==0);
    const auto probes=follower.stats.probes;
    for(unsigned i=0;i<1000;++i)follower.tick(ft(t+i),b);
    check(follower.stats.probes==probes);
    std::cout<<"dual checks="<<checks<<" samples="<<follower.stats.samples
      <<" remote_pulses="<<follower.stats.played<<" local_pulses="<<follower.stats.local
      <<" overlap_ms="<<both<<" outage_sound_ms="<<off<<" max_late_ms="<<follower.stats.max_late_ms<<'\n';
}
