#pragma once
#include "cluster.hpp"
#include <algorithm>

namespace cluster::dual {
// All times are monotonic uint32 milliseconds. Comparisons are valid only
// within half a wrap; every network acceptance window is < 4 seconds.
inline int64_t delta(uint32_t a,uint32_t b) {
    const uint32_t d=a-b;
    return d<=0x7fffffffu?int64_t(d):int64_t(d)-4294967296LL;
}
constexpr uint32_t period=250, lookahead=180, pulse=85, clock_lease=3000;
struct ClockMap {
    uint32_t local=0,remote=0,last=0;
    int32_t ppm=0;
    bool valid=false;
    bool fresh(uint32_t now) const {return valid&&uint32_t(now-last)<clock_lease;}
    bool sample(uint32_t t0,uint32_t t1,uint32_t t2,uint32_t t3) {
        const auto elapsed=delta(t3,t0), processing=delta(t2,t1);
        if(elapsed<0||elapsed>80||processing<0||processing>elapsed)return false;
        const uint32_t l=t0+uint32_t(elapsed/2),r=t1+uint32_t(processing/2);
        if(valid&&fresh(t3)) {
            const auto dl=delta(l,local),dr=delta(r,remote);
            if(dl<0||dr<0||dr>dl+20||dr<dl-20)return false;
            // Use a >=1 s baseline to limit integer timestamp quantization.
            // Shorter samples refresh the lease but don't replace the anchor.
            if(dl<1000){last=t3;return true;}
            const auto estimate=(dr-dl)*1000000/dl;
            ppm=int32_t(std::clamp(estimate,int64_t(-2000),int64_t(2000)));
        } else ppm=0;
        local=l;remote=r;last=t3;valid=true;return true;
    }
    uint32_t to_local(uint32_t remote_time) const {
        const auto d=delta(remote_time,remote);
        return local+uint32_t(d*1000000/(1000000+int64_t(ppm)));
    }
};
struct Stats {
    uint32_t probes=0,samples=0,events_sent=0,events_queued=0,played=0,local=0,rejected=0,dropped=0;
    uint32_t max_late_ms=0;
};
struct Event {uint32_t due=0,seq=0;uint8_t note=0;};
inline uint32_t voice_word(uint8_t note,bool follower) {
    // Complementary bass/upper-register patterns from the same beat number.
    constexpr std::array<uint8_t,8> a{{0,1,0,2,0,1,3,2}},b{{2,3,1,3,2,1,3,1}};
    return uint32_t(190)<<(8*(follower?b[note%8]:a[note%8]));
}
// One control-loop owner, no allocation, no network/audio callback coupling.
// A leader emits future pulses; follower translates them into its own clock.
// Both voices run locally. Touch/arm/mute remains the panel's responsibility.
class Peer {
    uint32_t epoch_,seq_=0,probe_=0,probe_t_=0,last_probe_=0,next_=0,last_event_=0;
    uint32_t active_due_=0,last_remote_=0;
    uint8_t active_note_=0,local_note_=0;
    bool leader_,network_=false,started_=false,probe_pending_=false,active_=false,remote_seen_=false;
    std::array<Event,4> queue_{};
    unsigned count_=0;
    void play(uint32_t due,uint8_t note,uint32_t now,bool remote) {
        active_due_=due;active_note_=note;active_=true;
        const auto late=delta(now,due);
        if(late>=0)stats.max_late_ms=std::max(stats.max_late_ms,uint32_t(late));
        if(remote)++stats.played;else ++stats.local;
    }
public:
    ClockMap clock{};
    Stats stats{};
    explicit Peer(uint32_t epoch=1,bool leader=true):epoch_(epoch),leader_(leader) {}
    bool leader() const{return leader_;}
    bool synchronized(uint32_t now) const{return !leader_&&network_&&clock.fresh(now)&&remote_seen_&&uint32_t(now-last_remote_)<600;}
    unsigned queued() const{return count_;}
    void network(bool on) {
        if(network_==on)return;
        network_=on;count_=0;clock={};probe_pending_=false;remote_seen_=false;active_=false;
        // Preserve monotonic sequence across local/remote mode changes.
        last_probe_=0;
    }
    uint32_t sound(uint32_t now) const {
        return active_&&delta(now,active_due_)>=0&&uint32_t(now-active_due_)<pulse?voice_word(active_note_,!leader_):0;
    }
    template<class Send> void receive(const Frame& f,uint32_t now,Send& send) {
        View v;
        if(!network_||!parse(f,v)||v.epoch!=epoch_){++stats.rejected;return;}
        if(leader_&&v.kind==Kind::sync_request) {
            auto reply=frame(Kind::sync_reply,epoch_,v.seq);
            auto* p=reply.bytes.data()+header;put32(p,get32(v.payload));put32(p+4,now);put32(p+8,now);
            if(!send(reply))++stats.dropped;
            return;
        }
        if(!leader_&&v.kind==Kind::sync_reply) {
            if(!probe_pending_||v.seq!=probe_||get32(v.payload)!=probe_t_){++stats.rejected;return;}
            probe_pending_=false;
            if(clock.sample(probe_t_,get32(v.payload+4),get32(v.payload+8),now))++stats.samples;
            else ++stats.rejected;
            return;
        }
        if(!leader_&&v.kind==Kind::pulse_event&&clock.fresh(now)) {
            const uint32_t note=get32(v.payload+4);
            if(v.seq<=last_event_||note>=8||get32(v.payload+8)!=period){++stats.rejected;return;}
            const auto due=clock.to_local(get32(v.payload));
            const auto ahead=delta(due,now);
            if(ahead<0||ahead>400||count_==queue_.size()||
               (count_&&delta(due,queue_[count_-1].due)<=0)){++stats.rejected;return;}
            queue_[count_++]={due,v.seq,uint8_t(note)};last_event_=v.seq;
            remote_seen_=true;last_remote_=now;++stats.events_queued;
            return;
        }
        ++stats.rejected;
    }
    template<class Send> void tick(uint32_t now,Send& send) {
        if(!started_){started_=true;next_=now+lookahead;last_probe_=now-500;}
        if(network_&&!leader_&&uint32_t(now-last_probe_)>=500&&probe_!=0xffffffffu) {
            last_probe_=now;probe_t_=now;++probe_;
            auto f=frame(Kind::sync_request,epoch_,probe_);
            put32(f.bytes.data()+header,now);probe_pending_=send(f);
            ++stats.probes;if(!probe_pending_)++stats.dropped;
        }
        if(!leader_&&!clock.fresh(now)){count_=0;remote_seen_=false;}
        if(leader_) {
            // At most one future event per loop; skip stale periods after stalls.
            if(delta(next_,now)<0&&uint32_t(now-next_)>=period)next_=now+lookahead;
            if(delta(next_,now)<=int64_t(lookahead)&&
               (!count_||queue_[0].due!=next_)) {
                const uint8_t note=uint8_t(local_note_++%8);
                if(count_==0) {
                    queue_[count_++]={next_,0,note};
                    if(network_&&seq_!=0xffffffffu) {
                        auto f=frame(Kind::pulse_event,epoch_,++seq_);auto* p=f.bytes.data()+header;
                        put32(p,next_);put32(p+4,note);put32(p+8,period);
                        if(send(f))++stats.events_sent;else ++stats.dropped;
                    }
                    next_+=period;
                }
            }
        }
        // Bounded drain: obsolete events never produce a late stuck note.
        while(count_&&delta(now,queue_[0].due)>=0) {
            const auto e=queue_[0];for(unsigned i=1;i<count_;++i)queue_[i-1]=queue_[i];--count_;
            if(uint32_t(now-e.due)<pulse)play(e.due,e.note,now,!leader_);
            else ++stats.rejected;
        }
        if(!leader_&&delta(now,next_)>=0) {
            if(!synchronized(now))play(now,uint8_t(local_note_++%8),now,false);
            next_=now+period;
        }
    }
};
} // namespace cluster::dual
