#include "cluster.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace cluster;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n'; std::exit(1); } } while(false)
static void tests() {
    Worker w(7); Coordinator c(7); Job j{123,512,64,41000,23000};
    CHECK(c.negotiate(w.receive(c.hello())));
    auto req=c.start(j,100,20); CHECK(req.size==28); CHECK(!c.start(j,101,20).size);
    auto res=w.receive(req); CHECK(res.size==144); CHECK(w.receive(req).bytes==res.bytes);
    auto conflict=req; conflict.bytes[header]^=1; CHECK(!w.receive(conflict).size);
    CHECK(c.receive(res,119)==Completion::remote); CHECK(c.grid==compute(j));
    CHECK(c.receive(res,119)==Completion::none);
    auto next=c.start(j,120,20); auto result=w.receive(next); CHECK(!w.receive(req).size);
    CHECK(c.receive(res,121)==Completion::none); CHECK(c.pending());
    auto bad=result; bad.bytes[header]^=1; CHECK(c.receive(bad,122)==Completion::none);
    CHECK(c.receive(result,140)==Completion::local); CHECK(c.grid==compute(j));
    CHECK(c.receive(result,141)==Completion::none);
    next=c.start(j,150,20); result=w.receive(next); c.disconnect();
    CHECK(c.poll(151)==Completion::local); CHECK(c.grid==compute(j));
    CHECK(c.receive(result,152)==Completion::none);
    CHECK(!c.start(j,160,20).size); CHECK(c.poll(160)==Completion::local);
    CHECK(c.negotiate(w.receive(c.hello())));
    next=c.start(j,0xfffffff8u,16); result=w.receive(next);
    CHECK(c.poll(3)==Completion::none); CHECK(c.receive(result,7)==Completion::remote);
    Coordinator fresh(8); CHECK(!fresh.negotiate(w.receive(fresh.hello())));
    Worker reset(8); CHECK(fresh.negotiate(reset.receive(fresh.hello())));
    next=fresh.start(j,0,20); CHECK(fresh.receive(result,1)==Completion::none);
    CHECK(fresh.receive(reset.receive(next),2)==Completion::remote);
    View v; CHECK(parse(next,v));
    for(std::size_t n=0;n<=max_frame+1;++n) { auto truncated=next; truncated.size=n; CHECK(parse(truncated,v)==(n==next.size)); }
    for(unsigned offset: {0u,1u,2u,3u,12u,13u,14u,15u}) { auto corrupt=next; corrupt.bytes[offset]^=0xff; CHECK(!parse(corrupt,v)); }
    auto invalid=next; put16(invalid.bytes.data()+header+4,513); CHECK(!reset.receive(invalid).size);
    CHECK(!encode_job(8,1,Job{1,0,1,1,1}).size); CHECK(!frame(Kind::hello,0,0).size);
    CHECK(!frame(Kind::job,8,0).size); CHECK(!frame(Kind::ready,8,1).size);
    // Independent deterministic fault schedule: loss, duplication, delayed old
    // results, explicit disconnect and rejoin. Compare every completed grid.
    Coordinator a(19); Worker b(19); CHECK(a.negotiate(b.receive(a.hello())));
    Frame old; unsigned remote=0,local=0;
    for(uint32_t i=0;i<1000;++i) {
        j={i, uint16_t(1+i%512),uint16_t(1+i%64),uint16_t(i*313),uint16_t(i*107)};
        const uint32_t now=i*50;
        if(i%37==0) a.disconnect();
        if(i%37==1) CHECK(a.negotiate(b.receive(a.hello())));
        const auto q=a.start(j,now,20); auto r=b.receive(q);
        Completion done=Completion::none;
        if(old.size) { done=a.receive(old,now+1); CHECK(done!=Completion::remote); }
        if(done==Completion::none && i%5 && r.size) done=a.receive(r,now+(i%7==0?25:3));
        if(done==Completion::none) done=a.poll(now+20);
        CHECK(done!=Completion::none); CHECK(a.grid==compute(j));
        CHECK(a.receive(r,now+30)==Completion::none); CHECK(!a.pending());
        if(done==Completion::remote) ++remote; else ++local;
        old=r;
    }
    CHECK(remote>0 && local>0); std::cout<<"fault completions remote="<<remote<<" local="<<local<<'\n';
}
static uint64_t digest(const Grid& g) { uint64_t h=14695981039346656037ull; for(auto x:g) {h^=x; h*=1099511628211ull;} return h; }
static void bench() {
    using Clock=std::chrono::steady_clock; using Ns=std::chrono::nanoseconds;
    Worker w(42); Coordinator c(42); CHECK(c.negotiate(w.receive(c.hello())));
    std::vector<int64_t> local,loop; uint64_t witness=0;
    for(uint32_t i=0;i<300;++i) {
        Job j{i+1,512,64,uint16_t(i*101),uint16_t(i*251)};
        auto a=Clock::now(); auto g=compute(j); auto b=Clock::now();
        auto req=c.start(j,i*30,20); auto r=w.receive(req); auto status=c.receive(r,i*30+1); auto d=Clock::now();
        CHECK(status==Completion::remote && c.grid==g); witness^=digest(g);
        if(i>=20) {local.push_back(std::chrono::duration_cast<Ns>(b-a).count()); loop.push_back(std::chrono::duration_cast<Ns>(d-b).count());}
    }
    auto stats=[](const char* name,std::vector<int64_t>& v){std::sort(v.begin(),v.end()); auto pct=[&](std::size_t p){return v[(v.size()*p+99)/100-1];};
        std::cout<<name<<" n="<<v.size()<<" p50_ns="<<pct(50)<<" p95_ns="<<pct(95)<<" p99_ns="<<pct(99)<<" max_ns="<<v.back()<<'\n';};
    stats("host_local_compute",local); stats("host_inprocess_codec_worker_coordinator",loop);
    std::cout<<"witness="<<witness<<" request_bytes=28 result_bytes=144 (no radio, virtual time for deadlines)\n";
}
static void preview() {
    Job j{42,512,64,44000,22000}; const auto g=compute(j); auto peak=*std::max_element(g.begin(),g.end());
    const char* shades=" .:-=+*#%@";
    std::cout<<"vector-field density (touch x=44000 y=22000)\n";
    for(unsigned y=0;y<8;++y){for(unsigned x=0;x<8;++x) std::cout<<shades[unsigned(g[y*8+x])*9/peak]; std::cout<<'\n';}
}
int main(){tests();bench();preview();std::cout<<"PASS checks="<<checks<<'\n';}
