#include "udp_transport.hpp"
static void worker(Udp& udp,bool faults,bool serve) {
    Worker w(91); uint32_t last=0;
    std::cout<<"READY"<<std::endl;
    const auto until=Clock::now()+(serve?1800s:20s);
    while(Clock::now()<until) {
        Frame f;
        if(!udp.receive(f)) { std::this_thread::sleep_for(1ms); continue; }
        View v; if(!parse(f,v)) continue;
        auto r=w.receive(f); if(!r.size) continue;
        if(faults&&v.kind==Kind::job) {
            const bool first=v.seq!=last; last=v.seq;
            if(v.seq%11==0||(first&&v.seq%7==0)) continue;
            if(v.seq%5==0) udp.send(r);
        }
        udp.send(r);
    }
}
static void controller(Udp& udp,bool faults) {
    Link link(91); auto send=[&](const Frame& f){return udp.send(f);};
    auto pump=[&](){
        auto done=link.tick(ms(),send);
        for(unsigned i=0;i<8;++i) { Frame f; if(!udp.receive(f)) break; auto c=link.receive(f,ms()); if(c!=Completion::none) done=c; }
        return done;
    };
    auto negotiate=[&](){const auto until=Clock::now()+2s; while(!link.ready()&&Clock::now()<until) {pump();std::this_thread::sleep_for(1ms);} if(!link.ready()) fail("handshake timeout");};
    std::vector<int64_t> remote,local;
    for(uint32_t i=1;i<=100;++i) {
        negotiate(); Job job{i,512,64,uint16_t(i*301),uint16_t(i*601)};
        const auto before=Clock::now(); if(!link.start(job,ms(),60,send)) fail("start failed");
        Completion done=Completion::none;
        const auto until=before+2s;
        while(done==Completion::none&&Clock::now()<until) {done=pump(); if(done==Completion::none) std::this_thread::sleep_for(1ms);}
        const auto us=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-before).count();
        if(done==Completion::none||link.grid()!=compute(job)) fail("wrong or missing grid");
        if(faults&&i%11==0&&done!=Completion::local) fail("forced loss did not fall back");
        (done==Completion::remote?remote:local).push_back(us);
    }
    auto print=[](const char* name,std::vector<int64_t>& v){if(v.empty()) return;std::sort(v.begin(),v.end());auto q=[&](std::size_t p){return v[(v.size()*p+99)/100-1];};std::cout<<name<<" n="<<v.size()<<" p50_us="<<q(50)<<" p95_us="<<q(95)<<" p99_us="<<q(99)<<" max_us="<<v.back()<<'\n';};
    print("host_udp_remote",remote); print("host_udp_local_fallback",local);
    if(remote.size()<75||(faults&&(local.size()<9||link.stats.retries<9))) fail("insufficient remote/fault witness");
    std::cout<<"PASS matched=100 remote="<<link.stats.remote<<" local="<<link.stats.local<<" retries="<<link.stats.retries<<" hello="<<link.stats.hello<<" accepted_tx="<<link.stats.sent<<" busy="<<link.stats.busy<<" rejected="<<udp.rejected<<'\n';
}
int main(int argc,char** argv) {
    if(argc!=5) fail("usage: cluster_udp worker|serve|controller own-port peer-port clean|faults");
#ifdef _WIN32
    WSADATA data{}; if(WSAStartup(MAKEWORD(2,2),&data)!=0) fail("WSAStartup failed");
#endif
    auto port=[](const char* x){char* end=nullptr;auto p=std::strtoul(x,&end,10);if(!*x||*end||p<1024||p>65535) fail("invalid port");return unsigned(p);};
    const bool faults=std::strcmp(argv[4],"faults")==0;
    if(!faults&&std::strcmp(argv[4],"clean")!=0) fail("invalid mode");
    { Udp udp(port(argv[2]),port(argv[3]));
      if(std::strcmp(argv[1],"worker")==0||std::strcmp(argv[1],"serve")==0) worker(udp,faults,std::strcmp(argv[1],"serve")==0);
      else if(std::strcmp(argv[1],"controller")==0) controller(udp,faults);
      else fail("invalid role"); }
#ifdef _WIN32
    WSACleanup();
#endif
}
