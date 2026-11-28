// Native Windows evaluator; same portable job, renderer and synthesis as replay.
// No physical hardware required. Loopback UDP is optional; no LAN connection.
#define NOMINMAX
#include "udp_transport.hpp"
#include "field_av.hpp"
#ifdef CLUSTER_MULTI_DESKTOP
#include "multi_field.hpp"
#endif
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <atomic>
#include <memory>
#include <cstdio>
#include <thread>
using namespace cluster;
namespace {
#ifdef CLUSTER_MULTI_DESKTOP
av::MultiField field;
std::array<std::unique_ptr<Udp>,2> udp;
unsigned nodes=0;
bool has_network(){return nodes!=0;}
#else
av::Field field;
std::unique_ptr<Udp> udp;
bool has_network(){return bool(udp);}
#endif
std::atomic<uint32_t> audio_word{0},audio_stamp{0};
std::atomic<bool> audio_stop{false},audio_ok{false};
std::array<uint32_t,av::width*av::height> pixels{};
bool held=false,smoke=false,remote_smoke=false;
unsigned paints=0,ticks=0;
void audio_loop() {
    WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=av::rate;
    fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=av::rate*2;
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
    av::Voice voice;
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
void release() {held=false;field.gate(false);audio_word.store(0);}
void touch(HWND w,int x,int y) {
    RECT r{};GetClientRect(w,&r);
    const int ww=std::max(1,int(r.right)),hh=std::max(1,int(r.bottom)-32);
    field.xy(uint16_t(std::clamp(x,0,ww-1)*65535LL/std::max(1,ww-1)),
             uint16_t(std::clamp(y,0,hh-1)*65535LL/std::max(1,hh-1)));
}
void update(HWND w) {
    const auto now=ms();
#ifdef CLUSTER_MULTI_DESKTOP
    if(field.remote_enabled())for(unsigned slot=0;slot<nodes;++slot)for(unsigned i=0;i<4;++i) {
        Frame f; if(!udp[slot]->receive(f))break; field.receive(slot,f,now);
    }
    auto send=[](unsigned slot,const Frame& f){return slot<nodes&&udp[slot]->send(f);};
#else
    if(udp&&field.remote_enabled())for(unsigned i=0;i<8;++i) {Frame f;if(!udp->receive(f))break;field.receive(f,now);}
    auto send=[](const Frame& f){return udp&&udp->send(f);};
#endif
    field.tick(now,send);
    audio_word.store(field.sound(),std::memory_order_release);
    audio_stamp.store(now,std::memory_order_release);
    ++ticks;
    InvalidateRect(w,nullptr,FALSE);
#ifdef CLUSTER_MULTI_DESKTOP
    if(smoke&&ticks>=(remote_smoke?240u:60u))DestroyWindow(w);
#else
    if(smoke&&ticks>=(remote_smoke?120u:30u))DestroyWindow(w);
#endif
}
LRESULT CALLBACK window_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: if(!SetTimer(w,1,16,nullptr))return -1;return 0;
    case WM_TIMER:
        if(smoke&&ticks==2)SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(100,100));
        if(smoke&&ticks==15) {
            SendMessageA(w,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(400,260));
            SendMessageA(w,WM_LBUTTONUP,0,MAKELPARAM(400,260));
        }
        if(remote_smoke&&ticks==30)SendMessageA(w,WM_KEYDOWN,'R',0);
        if(remote_smoke&&ticks==50)SendMessageA(w,WM_KEYDOWN,'R',0);
        update(w);return 0;
    case WM_LBUTTONDOWN:held=true;SetCapture(w);touch(w,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));field.gate(true);return 0;
    case WM_MOUSEMOVE:if(held)touch(w,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));return 0;
    case WM_LBUTTONUP:release();ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:case WM_CANCELMODE:case WM_KILLFOCUS:release();return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE)DestroyWindow(w);
        else if(wp=='M')release();
        else if(wp=='R'&&has_network())field.remote(!field.remote_enabled());
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);
        std::array<uint16_t,av::width> stripe{};
        for(unsigned y=0;y<av::height;++y) {
            av::row(field.grid(),field.shown(),y,stripe.data());
            for(unsigned x=0;x<av::width;++x) {
                const auto p=stripe[x];
                pixels[y*av::width+x]=uint32_t((p>>11)&31)*255/31*65536u+
                    uint32_t((p>>5)&63)*255/63*256u+uint32_t(p&31)*255/31;
            }
        }
        BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth=LONG(av::width);bmi.bmiHeader.biHeight=-LONG(av::height);
        bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
        const int image_height=std::max(1,int(client.bottom)-32);
        const int result=StretchDIBits(dc,0,0,int(client.right),image_height,0,0,int(av::width),int(av::height),
                    pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        RECT bar{0,LONG(image_height),client.right,client.bottom};
        FillRect(dc,&bar,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(225,230,235));
        char status[220];
#ifdef CLUSTER_MULTI_DESKTOP
        std::snprintf(status,sizeof(status),"%s | %s | %s | Hold/drag: sound  M: mute  R: remote  Esc: quit",
            field.remote_enabled()&&has_network()?"UDP enabled":"Local",
            field.mode(),audio_ok.load()?"audio on":"audio unavailable/off");
#else
        std::snprintf(status,sizeof(status),"%s | last %s | %s | Hold/drag: sound  M: mute  R: remote  Esc: quit",
            field.remote_enabled()?(field.ready()?"UDP ready":"UDP searching"):"Local",
            field.last_completion()==Completion::remote?"remote":"local",
            audio_ok.load()?"audio on":"audio unavailable/off");
#endif
        DrawTextA(dc,status,-1,&bar,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
        if(result!=GDI_ERROR&&result!=0)++paints;
        EndPaint(w,&ps);return 0;
    }
    case WM_DESTROY:release();KillTimer(w,1);PostQuitMessage(0);return 0;
    default:return DefWindowProcA(w,msg,wp,lp);
    }
}
}
int main(int argc,char** argv) {
#ifdef CLUSTER_MULTI_DESKTOP
    const bool endpoint_args=argc==4||argc==6;
    remote_smoke=endpoint_args&&std::strcmp(argv[1],"--udp-smoke")==0;
    smoke=remote_smoke||(argc==2&&std::strcmp(argv[1],"--smoke")==0);
    const bool network=remote_smoke||(endpoint_args&&std::strcmp(argv[1],"--udp")==0);
    if(argc!=1&&!smoke&&!network)fail("usage: cluster_multi_field [--smoke | --udp own0 peer0 [own1 peer1] | --udp-smoke own0 peer0 [own1 peer1]]");
#else
    remote_smoke=argc==4&&std::strcmp(argv[1],"--udp-smoke")==0;
    smoke=remote_smoke||(argc==2&&std::strcmp(argv[1],"--smoke")==0);
    const bool network=remote_smoke||(argc==4&&std::strcmp(argv[1],"--udp")==0);
    if(argc!=1&&!smoke&&!network)fail("usage: cluster_field [--smoke | --udp own-port peer-port | --udp-smoke own-port peer-port]");
#endif
    if(network) {
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup failed");
        auto port=[](const char* s){char* end=nullptr;const auto n=std::strtoul(s,&end,10);if(!*s||*end||n<1024||n>65535)fail("invalid port");return unsigned(n);};
#ifdef CLUSTER_MULTI_DESKTOP
        nodes=unsigned((argc-2)/2);
        std::array<unsigned,4> ports{};
        for(unsigned i=0;i<2*nodes;++i) {
            ports[i]=port(argv[2+i]);
            for(unsigned j=0;j<i;++j)if(ports[i]==ports[j])fail("endpoint ports must be distinct");
        }
        for(unsigned i=0;i<nodes;++i)udp[i]=std::make_unique<Udp>(ports[2*i],ports[2*i+1]);
        // Development loopback endpoints are distinct; both existing workers use test epoch 91.
        field.configure(nodes,91,91);
#else
        udp=std::make_unique<Udp>(port(argv[2]),port(argv[3]));field.remote(true);
#endif
    }
    const auto instance=GetModuleHandleA(nullptr);
    WNDCLASSA wc{};wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.lpszClassName="CYDClusterField";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if(!RegisterClassA(&wc))fail("RegisterClass failed");
#ifdef CLUSTER_MULTI_DESKTOP
    const char* title="VIBEWARE / VECTORFIELD - two-cohort cluster";
#else
    const char* title="VIBEWARE / VECTORFIELD - native cluster experiment";
#endif
    HWND w=CreateWindowExA(0,wc.lpszClassName,title,
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,800,640,nullptr,nullptr,instance,nullptr);
    if(!w)fail("CreateWindow failed");
    std::thread audio;
    if(!smoke)audio=std::thread(audio_loop);
    ShowWindow(w,SW_SHOW);UpdateWindow(w);
    MSG msg{};int rc=0;while((rc=GetMessageA(&msg,nullptr,0,0))>0) {TranslateMessage(&msg);DispatchMessageA(&msg);}
    audio_stop.store(true);if(audio.joinable())audio.join();
    if(network){
#ifdef CLUSTER_MULTI_DESKTOP
        for(auto& socket:udp)socket.reset();
#else
        udp.reset();
#endif
        WSACleanup();
    }
    if(smoke) {
        std::cout<<"GDI paint count="<<paints<<" control ticks="<<ticks<<" local jobs="<<field.stats().local<<" remote jobs="<<field.stats().remote<<'\n';
#ifdef CLUSTER_MULTI_DESKTOP
        if(paints<2||field.stats().local<2||field.stats().batches<2)return 1;
        for(unsigned slot=0;slot<nodes;++slot) {
            const auto& st=field.link_stats(slot);
            std::cout<<"slot="<<slot<<" remote="<<st.remote<<" local="<<st.local<<'\n';
            if(remote_smoke&&st.remote<2)return 1;
        }
#else
        if(paints<2||field.stats().local<2||(remote_smoke&&field.stats().remote<2))return 1;
#endif
    }
    return rc<0?1:0;
}
