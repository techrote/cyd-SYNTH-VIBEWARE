#include "multi.hpp"
#include "udp_transport.hpp"
#include <memory>

static unsigned number(const char* s,unsigned low,unsigned high) {
    char* end=nullptr; const auto v=std::strtoul(s,&end,10);
    if(!*s||*end||v<low||v>high) fail("invalid numeric argument");
    return unsigned(v);
}
static void print_latency(const char* label,std::vector<int64_t> v) {
    if(v.empty()) return;
    std::sort(v.begin(),v.end());
    auto q=[&](std::size_t p){return v[(v.size()*p+99)/100-1];};
    std::cout<<label<<" n="<<v.size()<<" p50_us="<<q(50)<<" p95_us="<<q(95)<<" p99_us="<<q(99)<<" max_us="<<v.back()<<'\n';
}
static void run(unsigned nodes,char** argv) {
    std::array<std::unique_ptr<Udp>,2> sockets{};
    for(unsigned i=0;i<nodes;++i) sockets[i]=std::make_unique<Udp>(number(argv[3+2*i],1024,65535),number(argv[4+2*i],1024,65535));
    Multi multi(nodes,91,91); // existing cluster_udp worker uses epoch 91
    auto send=[&](unsigned slot,const Frame& f){return sockets[slot]->send(f);};
    auto pump=[&](){
        for(unsigned slot=0;slot<nodes;++slot) {
            for(unsigned n=0;n<8;++n) {
                Frame f; if(!sockets[slot]->receive(f)) break;
                multi.receive(slot,f,ms());
            }
        }
        multi.tick(ms(),send);
    };
    const unsigned count=number(argv[2],1,1000);
    std::vector<int64_t> batches,reference,setup;
    for(unsigned i=1;i<=count;++i) {
        const auto setup_start=Clock::now();
        // Re-negotiate available workers before each trial. A missing peer
        // must not prevent local computation; give discovery a finite window.
        const auto ready_until=setup_start+150ms;
        while(nodes&&Clock::now()<ready_until) {
            pump(); bool all=true;
            for(unsigned s=0;s<nodes;++s) all=all&&multi.ready(s);
            if(all) break;
            std::this_thread::sleep_for(1ms);
        }
        setup.push_back(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-setup_start).count());
        const auto jobs=cohorts(uint32_t(i),uint16_t(i*301u),uint16_t(i*601u));
        const auto ref_start=Clock::now(); const auto expected=local_cohorts(jobs);
        reference.push_back(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-ref_start).count());
        const auto before=Clock::now();
        if(!multi.begin(jobs,100)) fail("multi start failed");
        while(multi.active()&&Clock::now()-before<2s) {
            pump();
            if(multi.active()&&nodes) std::this_thread::sleep_for(1ms);
        }
        batches.push_back(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-before).count());
        if(multi.active()||multi.grid!=expected) fail("cohort mismatch or incomplete batch");
    }
    if(multi.stats.batches!=count||multi.stats.remote+multi.stats.local!=2*count) fail("completion accounting");
    print_latency("host_batch",batches); print_latency("host_reference_compute",reference); print_latency("host_discovery",setup);
    for(unsigned slot=0;slot<nodes;++slot) {
        const auto& s=multi.link_stats(slot);
        std::cout<<"slot="<<slot<<" remote="<<s.remote<<" local="<<s.local<<" retries="<<s.retries<<" tx="<<s.sent<<" rejected="<<sockets[slot]->rejected<<'\n';
    }
    std::cout<<"PASS nodes="<<nodes<<" matched="<<count<<" remote="<<multi.stats.remote<<" local="<<multi.stats.local<<'\n';
}
int main(int argc,char** argv) {
    if(argc<3) fail("usage: cluster_multi_udp nodes batches [own-port peer-port]...");
    const unsigned nodes=number(argv[1],0,2);
    if(argc!=int(3+2*nodes)) fail("wrong number of ports");
#ifdef _WIN32
    WSADATA data{}; if(WSAStartup(MAKEWORD(2,2),&data)!=0) fail("WSAStartup failed");
#endif
    run(nodes,argv);
#ifdef _WIN32
    WSACleanup();
#endif
}
