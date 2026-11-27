#include "field_av.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
using namespace cluster;
using namespace cluster::av;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n'; std::exit(1); } } while(false)
static void le16(std::ofstream& f,uint16_t n) {char b[2]; b[0]=char(n&255u);b[1]=char(n>>8);f.write(b,2);}
static void le32(std::ofstream& f,uint32_t n) {le16(f,uint16_t(n));le16(f,uint16_t(n>>16));}
static void wav(const std::string& path,const std::vector<int16_t>& pcm) {
    std::ofstream f(path,std::ios::binary);CHECK(bool(f));
    const uint32_t bytes=uint32_t(pcm.size()*2);
    f.write("RIFF",4);le32(f,36+bytes);f.write("WAVEfmt ",8);le32(f,16);le16(f,1);le16(f,1);
    le32(f,rate);le32(f,rate*2);le16(f,2);le16(f,16);f.write("data",4);le32(f,bytes);
    for(auto s:pcm) le16(f,uint16_t(s));
    CHECK(bool(f));
}
static void ppm(const std::string& path,const Field& field) {
    std::ofstream f(path,std::ios::binary);CHECK(bool(f));f<<"P6\n"<<width<<" "<<height<<"\n255\n";
    std::array<uint16_t,width> pixels{};
    for(unsigned y=0;y<height;++y) {
        row(field.grid(),field.shown(),y,pixels.data());
        for(auto p:pixels) {char b[]={char(((p>>11)&31)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};f.write(b,3);}
    }
    CHECK(bool(f));
}
int main(int argc,char** argv) {
    Field local(91),remote(91);Worker worker(91);Voice voice,partitioned;
    remote.remote(true);Frame response{};unsigned deliveries=0,drops=0;
    auto no_send=[](const Frame&){return false;};
    auto send=[&](const Frame& f) {
        View v;CHECK(parse(f,v));
        if(v.kind==Kind::job&&v.seq%4==0) {++drops;return true;}
        response=worker.receive(f);return true;
    };
    std::vector<int16_t> pcm;std::array<int16_t,240> block{},other{};
    std::array<uint16_t,width> a{},b{};
    unsigned nonzero=0;int peak=0;uint32_t previous=0;
    // 300 ten-ms steps. At each 100-ms job boundary compare both paths
    // after the bounded 60-ms fallback window. Disable/re-enable remote too.
    for(uint32_t t=0;t<3000;t+=10) {
        if(t%100==0) {
            const uint16_t x=uint16_t(t*43u),y=uint16_t(t*21u+12345u);
            local.xy(x,y);remote.xy(x,y);
        }
        if(t==1200)remote.remote(false);
        if(t==1700)remote.remote(true);
        const bool gate=t>=100&&t<2400;
        local.gate(gate);remote.gate(gate);
        if(response.size) {const auto r=response;response={};remote.receive(r,t);++deliveries;}
        local.tick(t,no_send);remote.tick(t,send);
        if(t%100==80) {
            CHECK(local.grid()==remote.grid());
            CHECK(local.shown().seed==remote.shown().seed);
            CHECK(local.sound()==remote.sound());
            for(unsigned y=0;y<height;y+=17) {
                row(local.grid(),local.shown(),y,a.data());row(remote.grid(),remote.shown(),y,b.data());CHECK(a==b);
            }
        }
        const auto word=remote.sound();
        if(word!=previous)++nonzero;
        previous=word;
        voice.render(word,block.data(),block.size());
        partitioned.render(word,other.data(),73);partitioned.render(word,other.data()+73,167);
        CHECK(block==other);
        for(auto sample:block) {peak=std::max(peak,std::abs(int(sample)));pcm.push_back(sample);}
        if(t>=2700) for(auto sample:block) CHECK(sample==0);
    }
    CHECK(remote.stats().remote>0&&remote.stats().local>0&&drops>0&&deliveries>0);
    CHECK(nonzero>5&&peak>100&&peak<=9000);
    CHECK(pcm.size()==72000);
    if(argc==2) {wav(std::string(argv[1])+".wav",pcm);ppm(std::string(argv[1])+".ppm",remote);}
    else CHECK(argc==1);
    std::cout<<"PASS field checks="<<checks<<" remote="<<remote.stats().remote<<" local="<<remote.stats().local
             <<" dropped_tx="<<drops<<" peak_pcm="<<peak<<" controls_changes="<<nonzero<<" samples="<<pcm.size()<<'\n';
}
