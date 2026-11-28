#pragma once
#include "reaction_view.hpp"
#include "reaction_sound.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace cluster::spatial::gallery {
// Deterministic logical-time audition of the same presentation and DSP used
// by the interactive viewer. No sound device, sockets or RF in this fixture.
using Check=void(*)(bool,const char*);
constexpr uint32_t duration_ms=8000, epoch=91;
constexpr unsigned samples_per_ms=sound::rate/1000;
static_assert(sound::rate%1000==0);
struct Stats {
    uint32_t hash=2166136261u;
    unsigned peak=0, max_delta=0, nonzero=0, frozen=0, caught=0, admitted=0;
    int64_t sum=0;
};
inline void le16(std::ostream& out,uint16_t v){
    const uint8_t b[]={uint8_t(v),uint8_t(v>>8)};
    out.write(reinterpret_cast<const char*>(b),2);
}
inline void le32(std::ostream& out,uint32_t v){
    const uint8_t b[]={uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)};
    out.write(reinterpret_cast<const char*>(b),4);
}
inline void wav(const std::filesystem::path& file,const std::vector<int16_t>& pcm,Check check){
    std::ofstream out(file,std::ios::binary);
    check(bool(out),"gallery WAV open");
    const auto pcm_bytes=uint32_t(pcm.size()*2);
    out.write("RIFF",4);le32(out,36u+pcm_bytes);out.write("WAVEfmt ",8);
    le32(out,16);le16(out,1);le16(out,1);le32(out,sound::rate);
    le32(out,sound::rate*2);le16(out,2);le16(out,16);
    out.write("data",4);le32(out,pcm_bytes);
    for(const auto sample:pcm)le16(out,uint16_t(sample));
    out.close();check(bool(out),"gallery WAV write");
}
inline Stats render(unsigned palette,bool remote,bool fault,
                    std::vector<int16_t>& pcm,Check check,bool focused=false,bool coloured=false){
    ReactionView view(epoch);
    ReactionField worker;
    ReactionField::Checkpoint state{},last_remote_state{},actual{};
    sound::Voice voice;
    Stats stats;
    bool loss=false,rejoin=false;
    int previous=0;
    pcm.clear();pcm.reserve(std::size_t(duration_ms)*samples_per_ms);
    view.allow_remote(remote);
    view.present(0);
    for(uint32_t now=0;now<duration_ms;++now){
        const bool connected=remote&&(!fault||now<2400u||now>=4000u);
        if(remote&&now%50u==0){
            check(worker.advance(),"gallery worker advance");
            if(connected){
                const auto frame=encode(epoch,worker);
                check(view.receive(frame.data(),frame.size(),now),"gallery snapshot admission");
                if(worker.sequence()%5u==0){
                    check(worker.export_checkpoint(state),"gallery checkpoint export");
                    checkpoint::Prepared prepared(state);
                    for(std::size_t i=0;i<checkpoint::parts;++i){
                        const auto packet=prepared.packet(epoch,worker.sequence(),i);
                        check(view.receive(packet.data(),packet.size(),now),"gallery fragment admission");
                    }
                }
                if(fault&&worker.sequence()==48u)
                    check(worker.export_checkpoint(last_remote_state),"gallery loss reference");
            }
        }
        const auto before=view.image();
        const auto caught=view.follower().caught_up;
        view.step(now);
        if(view.follower().caught_up!=caught){
            check(fault&&now>=2400u&&now<4000u,"gallery unexpected catch-up");
            check(view.follower().local().sequence()==48u,"gallery recovery sequence");
            check(view.follower().local().export_checkpoint(actual)&&actual==last_remote_state,
                  "gallery exact recovery state");
        }
        view.present(now);
        if(view.frozen()){
            ++stats.frozen;
            check(view.image()==before,"gallery frozen image continuity");
        }
        if(remote&&now>=2400u&&now<4000u&&!view.follower().remote())loss=true;
        if(loss&&now>=4000u&&view.follower().remote())rejoin=true;
        auto word=sound::controls(view.image(),palette,now<duration_ms-200u);
        if(focused){
            // Eight-second traversal of all four corners, same local field as scene 1.
            constexpr unsigned xs[]={0,256,256,0},ys[]={0,0,256,256};
            const unsigned edge=(now/2000u)%4u,next=(edge+1u)%4u,t=now%2000u;
            const unsigned x=(xs[edge]*(2000u-t)+xs[next]*t)/2000u;
            const unsigned y=(ys[edge]*(2000u-t)+ys[next]*t)/2000u;
            word=sound::focus_word(word,x,y);
        }
        if(coloured)word=sound::colour_word(word,(now/2000u)%4u);
        std::array<int16_t,samples_per_ms> block{};
        voice.render(word,block.data(),block.size());
        for(const auto sample:block){
            const int v=sample;
            const auto magnitude=unsigned(v<0?-v:v);
            const int d=v-previous;
            stats.peak=std::max(stats.peak,magnitude);
            stats.max_delta=std::max(stats.max_delta,unsigned(d<0?-d:d));
            previous=v;stats.sum+=v;stats.nonzero+=v!=0?1u:0u;
            const uint16_t bits=uint16_t(sample);
            const uint8_t b[]={uint8_t(bits),uint8_t(bits>>8)};
            stats.hash=checkpoint::hash(b,2,stats.hash);
            pcm.push_back(sample);
        }
    }
    stats.caught=view.follower().caught_up;
    stats.admitted=view.follower().admitted;
    check(pcm.size()==std::size_t(duration_ms)*samples_per_ms,"gallery sample count");
    check(stats.peak>0&&stats.peak<=24192u&&stats.nonzero>sound::rate,"gallery audio bounds");
    check(std::all_of(pcm.end()-sound::rate/10,pcm.end(),[](int16_t v){return v==0;}),
          "gallery final silence");
    if(fault){
        check(loss&&rejoin&&stats.frozen>0&&stats.caught==1&&stats.admitted>0,
              "gallery loss, catch-up and rejoin");
    }else{
        check(!loss&&!rejoin&&!stats.frozen&&!stats.caught,"gallery clean state");
    }
    check(view.follower().cold_losses==0,"gallery no cold fallback");
    if(remote)check(view.follower().remote()&&stats.admitted>0,"gallery remote ending");
    else check(!view.follower().remote()&&!stats.admitted,"gallery standalone ending");
    return stats;
}
inline void run(Check check,const std::filesystem::path* folder=nullptr){
    struct Scene {const char* name;unsigned palette;bool remote,fault,focused=false,coloured=false;};
    constexpr Scene scenes[]={
        {"01-local-open-fifths",0,false,false},
        {"02-local-higher-fifths",1,false,false},
        {"03-local-dark-intervals",2,false,false},
        {"04-local-major",3,false,false},
        {"05-remote-clean",0,true,false},
        {"06-remote-blackout",0,true,true},
        {"07-local-focus-orbit",0,false,false,true},
        {"08-local-spectral-colour",0,false,false,false,true}};
    std::ofstream receipt;
    if(folder){
        check(std::filesystem::create_directory(*folder),"gallery requires new output directory");
        receipt.open(*folder/"metrics.csv",std::ios::binary);
        check(bool(receipt),"gallery receipt open");
        receipt<<"scene,samples,peak,max_delta,sum,nonzero,pcm_fnv32,frozen_ms,catchups,checkpoints\n";
    }
    std::vector<int16_t> pcm;
    std::array<uint32_t,8> hashes{};
    std::size_t index=0;
    for(const auto& scene:scenes){
        const auto s=render(scene.palette,scene.remote,scene.fault,pcm,check,scene.focused,scene.coloured);
        hashes[index++]=s.hash;
        if(folder){
            wav(*folder/(std::string(scene.name)+".wav"),pcm,check);
            receipt<<scene.name<<','<<pcm.size()<<','<<s.peak<<','<<s.max_delta<<','<<s.sum
                   <<','<<s.nonzero<<','<<s.hash<<','<<s.frozen<<','<<s.caught<<','<<s.admitted<<'\n';
        }
        std::cout<<"REACTION_GALLERY "<<scene.name<<" samples="<<pcm.size()<<" peak="<<s.peak
                 <<" delta="<<s.max_delta<<" sum="<<s.sum<<" hash="<<s.hash
                 <<" frozen_ms="<<s.frozen<<" caught="<<s.caught<<" checkpoints="<<s.admitted<<'\n';
    }
    for(std::size_t i=0;i<4;++i)for(std::size_t j=i+1;j<4;++j)
        check(hashes[i]!=hashes[j],"gallery distinct palettes");
    check(hashes[4]!=hashes[5],"gallery audible fault-path difference");
    check(hashes[0]!=hashes[6],"gallery focus changes actual-core PCM");
    check(hashes[0]!=hashes[7],"gallery colour changes actual-core PCM");
    if(folder){receipt.close();check(bool(receipt),"gallery receipt write");}
}
} // namespace cluster::spatial::gallery
