// Actual-core desktop audiovisual audition. Optional isolated localhost stream.
#include "udp_transport.hpp"
#include "reaction_view.hpp"
#include "reaction_sound.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <thread>
#include <memory>
#include <cstdio>
namespace sf=cluster::spatial;
namespace {
namespace cp=sf::checkpoint;
sf::ReactionView view(91);
std::unique_ptr<Udp> udp;
std::array<uint32_t,320*240> pixels{};
bool enabled=false,held=false,smoke=false,udp_smoke=false,checkpoint_smoke=false,saw_remote=false,saw_loss=false;
bool manual_started=false,manual_finished=false,manual_seen=false,manual_rejoin=false,catchup_witness=false;
uint32_t start=0,manual_at=0;
unsigned accepted=0,rejected=0,recovered=0,paints=0,local_ticks=0,remote_ticks=0,frozen_polls=0,loss_caught=0;
std::atomic<uint32_t> audio_word{0},audio_stamp{0};
std::atomic<bool> audio_stop{false},audio_ok{false};
bool sound_enabled=false;
unsigned palette=0;
void audio_loop() {
    WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=sf::sound::rate;
    fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=sf::sound::rate*2;
    HWAVEOUT device=nullptr;
    if(waveOutOpen(&device,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)return;
    constexpr unsigned count=4, samples=256;
    std::array<std::array<int16_t,samples>,count> pcm{};
    std::array<WAVEHDR,count> headers{};
    unsigned prepared=0;
    for(;prepared<count;++prepared) {
        auto& h=headers[prepared];h.lpData=reinterpret_cast<LPSTR>(pcm[prepared].data());
        h.dwBufferLength=DWORD(samples*sizeof(int16_t));
        if(waveOutPrepareHeader(device,&h,sizeof(h))!=MMSYSERR_NOERROR)break;
    }
    sf::sound::Voice voice;
    if(prepared==count) {
        audio_ok.store(true);
        bool healthy=true;
        while(!audio_stop.load()&&healthy) {
            for(unsigned i=0;i<count;++i) {
                auto& h=headers[i];
                if(!(h.dwFlags&WHDR_INQUEUE)) {
                    const auto stamp=audio_stamp.load(std::memory_order_acquire);
                    const auto word=uint32_t(ms()-stamp)>120?0:audio_word.load(std::memory_order_acquire);
                    voice.render(word,pcm[i].data(),samples);
                    if(waveOutWrite(device,&h,sizeof(h))!=MMSYSERR_NOERROR) {healthy=false;break;}
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    audio_ok.store(false);
    waveOutReset(device);
    for(unsigned i=0;i<prepared;++i)waveOutUnprepareHeader(device,&headers[i],sizeof(WAVEHDR));
    waveOutClose(device);
}

void mute(){sound_enabled=false;audio_word.store(0,std::memory_order_release);}
void sync_allowed(){view.allow_remote(enabled&&!held);}
void update(HWND w){
    const auto now=ms();
    const auto& f=view.follower();
    if(checkpoint_smoke&&!manual_started&&f.remote()&&f.admitted&&f.view().sequence()<=10){
        manual_started=true;manual_at=now;SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,0);
    }
    if(manual_started&&!manual_finished&&uint32_t(now-manual_at)>=100){
        manual_finished=true;SendMessageA(w,WM_LBUTTONUP,0,0);
    }
    sync_allowed();
    if(udp)for(unsigned i=0;i<24;++i){
        cp::Packet p{};std::size_t n=0;
        if(!udp->receive_bytes(p.data(),p.size(),n))break;
        if(view.remote_allowed()){
            if(view.receive(p.data(),n,ms())){++accepted;if(saw_loss)++recovered;}
            else ++rejected;
        }
    }
    // Bounded burst of cooperative slices, soft desktop-only time budget.
    const bool remote_before=f.remote();
    const auto caught_before=f.caught_up;
    const auto budget_start=Clock::now();
    for(unsigned i=0;i<32;++i){
        if(!view.step(ms())||Clock::now()-budget_start>=2ms)break;
    }
    if(remote_before&&!f.remote()&&view.remote_allowed()&&f.view().sequence()<80&&!saw_loss){
        saw_loss=true;loss_caught=caught_before;
    }
    if(f.remote()){
        saw_remote=true;++remote_ticks;
        if(manual_finished&&!saw_loss)manual_rejoin=true;
    }else{
        ++local_ticks;
        if(manual_started&&!manual_finished&&!view.remote_allowed())manual_seen=true;
        if(saw_loss&&f.view().sequence()<51&&f.caught_up>loss_caught)catchup_witness=true;
    }
    if(view.frozen())++frozen_polls;
    view.present(ms());
    audio_word.store(sf::sound::controls(view.image(),palette,sound_enabled),std::memory_order_release);
    audio_stamp.store(ms(),std::memory_order_release);
    InvalidateRect(w,nullptr,FALSE);
    if(smoke&&uint32_t(now-start)>=(checkpoint_smoke?6000u:(udp_smoke?4000u:600u)))DestroyWindow(w);
}
LRESULT CALLBACK wnd(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE: return SetTimer(w,1,16,nullptr)?0:-1;
    case WM_TIMER: update(w);return 0;
    case WM_LBUTTONDOWN:held=true;sync_allowed();SetCapture(w);return 0;
    case WM_LBUTTONUP:held=false;sync_allowed();ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:case WM_CANCELMODE:held=false;sync_allowed();return 0;
    case WM_KILLFOCUS:mute();held=false;sync_allowed();return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE)DestroyWindow(w);
        else if(wp=='R'&&udp&&!(lp&(1L<<30))){enabled=!enabled;sync_allowed();}
        else if(wp=='S'&&!(lp&(1L<<30))){if(sound_enabled)mute();else sound_enabled=true;}
        else if(wp=='M')mute();
        else if(wp>='1'&&wp<='4')palette=unsigned(wp-'1');
        return 0;
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);
        RECT client{};GetClientRect(w,&client);
        std::array<uint16_t,320> row{};
        for(std::size_t y=0;y<240;++y){
            sf::row(view.image(),y,row.data());
            for(std::size_t x=0;x<320;++x){
                const auto p=row[x];
                pixels[y*320+x]=uint32_t((p>>11)&31)*255/31*65536u+
                    uint32_t((p>>5)&63)*255/63*256u+uint32_t(p&31)*255/31;
            }
        }
        BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth=320;bmi.bmiHeader.biHeight=-240;
        bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
        const int height=std::max(1,int(client.bottom)-32);
        const int result=StretchDIBits(dc,0,0,int(client.right),height,0,0,320,240,pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        RECT bar{0,LONG(height),client.right,client.bottom};
        FillRect(dc,&bar,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(225,230,235));
        char status[200];
        const auto& f=view.follower();
        std::snprintf(status,sizeof(status),"%s%s | L%u R%u C%u | %s P%u | S: sound M: mute 1-4: palette R: remote",
            view.frozen()?"CATCH-UP":(f.remote()?"REMOTE":"LOCAL"),view.blending()&&!view.frozen()?" (blend)":"",
            unsigned(f.local().sequence()),unsigned(f.view().sequence()),unsigned(f.saved_sequence()),audio_ok.load()?(sound_enabled?"sound on":"muted"):"audio unavailable",palette+1);
        DrawTextA(dc,status,-1,&bar,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
        if(result!=GDI_ERROR&&result!=0)++paints;
        EndPaint(w,&ps);return 0;
    }
    case WM_DESTROY:mute();KillTimer(w,1);PostQuitMessage(0);return 0;
    default:return DefWindowProcA(w,msg,wp,lp);
    }
}
unsigned port(const char* s){
    char* end=nullptr;const auto n=std::strtoul(s,&end,10);
    if(!*s||*end||n<1024||n>65535)fail("invalid port");
    return unsigned(n);
}
}
int main(int argc,char** argv){
    checkpoint_smoke=argc==4&&std::strcmp(argv[1],"--checkpoint-smoke")==0;
    udp_smoke=checkpoint_smoke||(argc==4&&std::strcmp(argv[1],"--udp-smoke")==0);
    smoke=udp_smoke||(argc==2&&std::strcmp(argv[1],"--smoke")==0);
    const bool network=udp_smoke||(argc==4&&std::strcmp(argv[1],"--udp")==0);
    if(argc!=1&&!smoke&&!network)fail("usage: cluster_reaction_view [--smoke | --udp own-port peer-port | --udp-smoke own-port peer-port | --checkpoint-smoke own-port peer-port]");
    if(network){
        const auto own=port(argv[2]),peer=port(argv[3]);
        if(own==peer)fail("distinct ports required");
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup");
        udp=std::make_unique<Udp>(own,peer);enabled=true;
    }
    const auto instance=GetModuleHandleA(nullptr);
    WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=instance;wc.lpszClassName="CYDReactionView";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if(!RegisterClassA(&wc))fail("RegisterClass");
    HWND w=CreateWindowExA(0,wc.lpszClassName,"VIBEWARE / REACTION - distributed audiovisual synthesis",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,800,640,nullptr,nullptr,instance,nullptr);
    if(!w)fail("CreateWindow");
    start=ms();sync_allowed();view.present(start);
    std::thread audio;
    if(!smoke)audio=std::thread(audio_loop);
    ShowWindow(w,SW_SHOW);UpdateWindow(w);
    if(udp_smoke)std::cout<<"READY"<<std::endl;
    MSG msg{};int rc=0;while((rc=GetMessageA(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    audio_stop.store(true);if(audio.joinable())audio.join();
    if(network){udp.reset();WSACleanup();}
    if(smoke){
        std::cout<<"GDI paints="<<paints<<" local_ticks="<<local_ticks<<" remote_ticks="<<remote_ticks
            <<" local_seq="<<view.follower().local().sequence()<<" accepted="<<accepted<<" rejected="<<rejected
            <<" recovered="<<recovered<<" loss="<<saw_loss<<" admitted="<<view.follower().admitted
            <<" catchup="<<catchup_witness<<" frozen_polls="<<frozen_polls<<" manual="<<manual_seen
            <<" manual_rejoin="<<manual_rejoin<<'\n';
        if(paints<2||view.follower().local().sequence()<2||!local_ticks)return 1;
        if(checkpoint_smoke&&(!catchup_witness||!frozen_polls||!manual_seen||!manual_rejoin||view.follower().admitted<5))return 1;
        if(udp_smoke&&(!remote_ticks||accepted<20||!rejected||!saw_loss||!recovered))return 1;
    }
    return rc<0?1:0;
}
