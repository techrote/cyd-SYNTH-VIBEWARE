#include "spatial.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>

namespace sf=cluster::spatial;
using Clock=std::chrono::steady_clock;
constexpr std::size_t steps=512, frames=steps/2;
using Frame=std::array<uint16_t,320u*240u>;
static void require(bool ok){if(!ok){std::cerr<<"spatial benchmark mismatch\n";std::exit(1);}}
template<class Fn> static uint64_t time_ns(Fn fn){
    const auto start=Clock::now();fn();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());
}
template<std::size_t N> static void report(const char* name,std::array<uint64_t,N> a){
    uint64_t sum=0;for(auto n:a)sum+=n;
    std::sort(a.begin(),a.end());
    std::cout<<name<<" n="<<N<<" total_ns="<<sum<<" p50_ns="<<a[(N*50+99)/100-1]
        <<" p95_ns="<<a[(N*95+99)/100-1]<<" p99_ns="<<a[(N*99+99)/100-1]<<" max_ns="<<a.back()<<'\n';
}
static void raster(const sf::Pixels& p,Frame& out){
    for(std::size_t y=0;y<240;++y)sf::row(p,y,out.data()+y*320);
}
int main(){
    sf::Field local,worker;sf::Receiver receiver(91);
    std::array<uint64_t,steps> compute{},encode{},decode{};
    std::array<uint64_t,frames> local_render{},remote_render{};
    static Frame a{},b{};
    uint32_t digest=0;
    for(std::size_t i=0;i<steps;++i){
        bool ok=false;
        compute[i]=time_ns([&]{ok=local.advance();});require(ok);
        require(worker.advance());
        sf::Packet packet{};
        encode[i]=time_ns([&]{packet=sf::encode(91,worker);});
        decode[i]=time_ns([&]{ok=receiver.accept(packet.data(),packet.size(),uint32_t(i*50));});
        require(ok&&receiver.sequence()==local.sequence()&&receiver.pixels()==local.pixels());
        digest=digest*33u+sf::digest(receiver.pixels());
        if(i%2==1){
            const auto j=i/2;
            // Alternate ordering to reduce systematic cache/temperature bias.
            if(j%2==0){
                local_render[j]=time_ns([&]{raster(local.pixels(),a);});
                remote_render[j]=time_ns([&]{raster(receiver.pixels(),b);});
            }else{
                remote_render[j]=time_ns([&]{raster(receiver.pixels(),b);});
                local_render[j]=time_ns([&]{raster(local.pixels(),a);});
            }
            require(a==b);
        }
    }
    std::cout<<"PASS matched snapshots="<<steps<<" frames="<<frames<<" digest="<<digest<<" payload_bytes="<<steps*sf::bytes<<'\n';
    report("local_compute",compute);report("worker_encode",encode);report("receiver_decode",decode);
    report("local_raster",local_render);report("remote_raster",remote_render);
    std::cout<<"Host in-process kernel timings only; radio, socket, queue, SPI, touch and audio excluded.\n";
}
