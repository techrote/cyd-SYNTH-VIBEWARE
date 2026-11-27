#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace cluster {
constexpr std::size_t header = 16, max_payload = 128, max_frame = header + max_payload;
constexpr uint8_t version = 1;
enum class Kind : uint8_t { hello = 1, ready, job, result, sync_request, sync_reply, pulse_event };
struct Frame {
    std::array<uint8_t, max_frame> bytes{};
    std::size_t size = 0;
};
inline void put16(uint8_t* p, uint16_t x) { p[0] = uint8_t(x); p[1] = uint8_t(x >> 8); }
inline void put32(uint8_t* p, uint32_t x) { put16(p, uint16_t(x)); put16(p+2, uint16_t(x >> 16)); }
inline uint16_t get16(const uint8_t* p) { return uint16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8)); }
inline uint32_t get32(const uint8_t* p) { return uint32_t(get16(p)) | (uint32_t(get16(p+2)) << 16); }
struct View { Kind kind{}; uint32_t epoch=0, seq=0; const uint8_t* payload=nullptr; };
inline std::size_t payload_size(Kind k) {
    switch(k) { case Kind::hello: return 0; case Kind::ready: return 4;
        case Kind::job: return 12; case Kind::result: return 128;
        case Kind::sync_request: return 4; case Kind::sync_reply: case Kind::pulse_event: return 12; }
    return max_payload+1;
}
inline Frame frame(Kind k, uint32_t epoch, uint32_t seq) {
    Frame f; const auto n=payload_size(k);
    if (!epoch || n>max_payload || ((k==Kind::hello || k==Kind::ready) != (seq==0))) return f;
    auto* p=f.bytes.data(); p[0]='C'; p[1]='V'; p[2]=version; p[3]=uint8_t(k);
    put32(p+4,epoch); put32(p+8,seq); put16(p+12,uint16_t(n)); // reserved bytes 14..15 zero
    f.size=header+n; return f;
}
inline bool parse(const Frame& f, View& v) {
    if (f.size<header || f.size>max_frame) return false;
    const auto* p=f.bytes.data(); const auto k=Kind(p[3]); const auto n=payload_size(k);
    if(p[0]!='C'||p[1]!='V'||p[2]!=version||n>max_payload||get16(p+12)!=n||f.size!=header+n||get16(p+14)!=0) return false;
    const auto e=get32(p+4), s=get32(p+8);
    if(!e || ((k==Kind::hello || k==Kind::ready)!=(s==0))) return false;
    v={k,e,s,p+header}; return true;
}
constexpr uint16_t max_particles=512, max_steps=64;
struct Job { uint32_t seed=1; uint16_t particles=128, steps=32, x=32768, y=32768; };
using Grid=std::array<uint16_t,64>;
inline bool valid(const Job& j) { return j.particles>0 && j.particles<=max_particles && j.steps>0 && j.steps<=max_steps; }
inline Frame encode_job(uint32_t e,uint32_t s,const Job& j) {
    if(!valid(j)) return {};
    auto f=frame(Kind::job,e,s); if(!f.size) return f;
    auto* p=f.bytes.data()+header; put32(p,j.seed); put16(p+4,j.particles); put16(p+6,j.steps); put16(p+8,j.x); put16(p+10,j.y); return f;
}
inline bool decode_job(const View& v,Job& j) {
    if(v.kind!=Kind::job || !v.payload) return false;
    const auto* p=v.payload; j={get32(p),get16(p+4),get16(p+6),get16(p+8),get16(p+10)}; return valid(j);
}
inline uint32_t random(uint32_t& s) { s ^= s<<13; s ^= s>>17; s ^= s<<5; return s; }
// Fixed-point, periodic 16-bit domain. Each job integrates an independent seeded
// particle cohort around a touch-directed vortex, then bins its final positions.
// Explicit signed division (toward zero), unsigned conversion/wrap, no FP or heap.
inline Grid compute(const Job& j) {
    Grid g{}; if(!valid(j)) return g;
    uint32_t rng=j.seed ? j.seed : 0x9e3779b9u;
    for(uint16_t i=0;i<j.particles;++i) {
        uint16_t x=uint16_t(random(rng)), y=uint16_t(random(rng));
        for(uint16_t t=0;t<j.steps;++t) {
            const int32_t dx=int32_t(j.x)-x, dy=int32_t(j.y)-y;
            const int32_t nx=dx/32-dy/16, ny=dy/32+dx/16;
            x=uint16_t(int32_t(x)+nx); y=uint16_t(int32_t(y)+ny);
        }
        ++g[std::size_t(y>>13)*8+(x>>13)];
    }
    return g;
}
inline Frame encode_grid(uint32_t e,uint32_t s,const Grid& g) {
    auto f=frame(Kind::result,e,s); if(!f.size) return f;
    for(std::size_t i=0;i<g.size();++i) put16(f.bytes.data()+header+2*i,g[i]);
    return f;
}
inline bool decode_grid(const View& v,uint16_t particles,Grid& g) {
    if(v.kind!=Kind::result || !v.payload) return false;
    Grid tmp{}; uint32_t sum=0;
    for(std::size_t i=0;i<tmp.size();++i) { tmp[i]=get16(v.payload+2*i); sum+=tmp[i]; }
    if(sum!=particles) return false;
    g=tmp; return true;
}
// One pre-authorized peer and out-of-band nonzero session epoch. Transport must
// filter peer identity before calling receive; this codec is NOT authentication.
// Stateless jobs permit safe retry without modifying remote simulation state.
class Worker {
    uint32_t epoch_, last_=0; Frame cached_request_{}, cached_result_{};
public:
    explicit Worker(uint32_t epoch):epoch_(epoch) {}
    Frame receive(const Frame& f) {
        View v; if(!parse(f,v)||v.epoch!=epoch_) return {};
        if(v.kind==Kind::hello) {
            auto r=frame(Kind::ready,epoch_,0); put16(r.bytes.data()+header,max_particles); put16(r.bytes.data()+header+2,max_steps); return r;
        }
        Job j; if(!decode_job(v,j)||v.seq<last_) return {};
        if(v.seq==last_) {
            for(std::size_t i=0;i<f.size;++i) if(f.bytes[i]!=cached_request_.bytes[i]) return {};
            return cached_result_;
        }
        cached_request_=f; cached_result_=encode_grid(epoch_,v.seq,compute(j)); last_=v.seq; return cached_result_;
    }
};
enum class Completion { none, remote, local };
class Coordinator {
    uint32_t epoch_, seq_=0, sent_=0, budget_=0; Job job_{}; bool ready_=false,pending_=false;
public:
    Grid grid{};
    explicit Coordinator(uint32_t epoch):epoch_(epoch) {}
    Frame hello() const { return frame(Kind::hello,epoch_,0); }
    bool negotiate(const Frame& f) {
        View v; if(pending_||!parse(f,v)||v.epoch!=epoch_||v.kind!=Kind::ready||get16(v.payload)!=max_particles||get16(v.payload+2)!=max_steps) return false;
        ready_=true; return true;
    }
    bool pending() const { return pending_; }
    bool ready() const { return ready_; }
    // Budget in coordinator monotonic milliseconds, limited to 60 seconds.
    // All calls on the coordinator loop, never an audio callback.
    Frame start(const Job& j,uint32_t now,uint32_t budget) {
        if(pending_||!valid(j)||!budget||budget>60000||seq_==std::numeric_limits<uint32_t>::max()) return {};
        job_=j; sent_=now; budget_=budget; pending_=true; ++seq_;
        return ready_ ? encode_job(epoch_,seq_,j) : Frame{};
    }
    Completion poll(uint32_t now) {
        if(!pending_ || (ready_ && uint32_t(now-sent_)<budget_)) return Completion::none;
        grid=compute(job_); pending_=false; return Completion::local;
    }
    Completion receive(const Frame& f,uint32_t now) {
        if(!pending_) return Completion::none;
        if(!ready_||uint32_t(now-sent_)>=budget_) return poll(now);
        View v; if(!parse(f,v)||v.epoch!=epoch_||v.seq!=seq_||!decode_grid(v,job_.particles,grid)) return Completion::none;
        pending_=false; return Completion::remote;
    }
    void disconnect() { ready_=false; }
};
} // namespace cluster
