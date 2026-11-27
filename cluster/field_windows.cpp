// Native Windows evaluator; same portable job, renderer and synthesis as replay.
// No physical hardware required. Loopback UDP is optional; no LAN connection.
#define NOMINMAX
#include "udp_transport.hpp"
#include "field_av.hpp"
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <atomic>
#include <memory>
#include <cstdio>
#include <thread>
using namespace cluster;
namespace {
using cluster::av::Field;
Field field;
std::unique_ptr<Udp> udp;
std::atomic<uint32_t> audio_word{0};
std::atomic<bool> audio_stop{false},audio_ok{false};
std::array<uint32_t,av::width*av::height> pixels{};
bool held=false,smoke=false;
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
                    voice.render(audio_word.load(),pcm[i].data(),samples);
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
    if(udp&&field.remote_enabled())for(unsigned i=0;i<8;++i) {Frame f;if(!udp->receive(f))break;field.receive(f,now);}
    auto send=[](const Frame& f){return udp&&udp->send(f);};
    field.tick(now,send);
    audio_word.store(field.sound());
    ++ticks;
    InvalidateRect(w,nullptr,FALSE);
    if(smoke&&ticks>=30)DestroyWindow(w);
}
LRESULT CALLBACK window_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: if(!SetTimer(w,1,16,nullptr))return -1;return 0;
    case WM_TIMER:
        if(smoke&&ticks==2){touch(w,100,100);field.gate(true);}
        if(smoke&&ticks==15){touch(w,400,260);release();}
        update(w);return 0;
    case WM_LBUTTONDOWN:held=true;SetCapture(w);touch(w,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));field.gate(true);return 0;
    case WM_MOUSEMOVE:if(held)touch(w,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));return 0;
    case WM_LBUTTONUP:release();ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:case WM_CANCELMODE:case WM_KILLFOCUS:release();return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE)DestroyWindow(w);
        else if(wp=='M')release();
        else if(wp=='R'&&udp)field.remote(!field.remote_enabled());
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
        std::snprintf(status,sizeof(status),"%s | last %s | %s | Hold/drag: sound  M: mute  R: remote  Esc: quit",
            field.remote_enabled()?(field.ready()?"UDP ready":"UDP searching"):"Local",
            field.last_completion()==Completion::remote?"remote":"local",
            audio_ok.load()?"audio on":"audio unavailable/off");
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
    smoke=argc==2&&std::strcmp(argv[1],"--smoke")==0;
    const bool network=argc==4&&std::strcmp(argv[1],"--udp")==0;
    if(argc!=1&&!smoke&&!network)fail("usage: cluster_field [--smoke | --udp own-port peer-port]");
    if(network) {
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup failed");
        auto port=[](const char* s){char* end=nullptr;const auto n=std::strtoul(s,&end,10);if(!*s||*end||n<1024||n>65535)fail("invalid port");return unsigned(n);};
        udp=std::make_unique<Udp>(port(argv[2]),port(argv[3]));field.remote(true);
    }
    const auto instance=GetModuleHandleA(nullptr);
    WNDCLASSA wc{};wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.lpszClassName="CYDClusterField";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if(!RegisterClassA(&wc))fail("RegisterClass failed");
    HWND w=CreateWindowExA(0,wc.lpszClassName,"VIBEWARE / VECTORFIELD - native cluster experiment",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,800,640,nullptr,nullptr,instance,nullptr);
    if(!w)fail("CreateWindow failed");
    std::thread audio;
    if(!smoke)audio=std::thread(audio_loop);
    ShowWindow(w,SW_SHOW);UpdateWindow(w);
    MSG msg{};int rc=0;while((rc=GetMessageA(&msg,nullptr,0,0))>0) {TranslateMessage(&msg);DispatchMessageA(&msg);}
    audio_stop.store(true);if(audio.joinable())audio.join();
    if(network){udp.reset();WSACleanup();}
    if(smoke) {std::cout<<"GDI paint count="<<paints<<" control ticks="<<ticks<<" local jobs="<<field.stats().local<<'\n';if(paints<2||field.stats().local<2)return 1;}
    return rc<0?1:0;
}
