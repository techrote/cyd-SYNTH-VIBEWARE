#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include "core.hpp"
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>
using namespace gridseq;
static Surface surface;static Exchange exchange;
static std::atomic<uint32_t> head{0};static std::atomic<bool> stop{false};
static std::atomic<int> audioStatus{0};
static std::array<uint32_t,width*height> pixels{};
static bool held=false,smoke=false;static unsigned ticks=0;
static Engine smokeEngine;static bool painted=false,sounded=false;
static void audio(){
    WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;
    fmt.nSamplesPerSec=rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=rate*2;
    HWAVEOUT dev=nullptr;
    if(waveOutOpen(&dev,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){audioStatus=-1;return;}
    Engine e;Control c;std::array<std::array<int16_t,block>,4> pcm{};std::array<WAVEHDR,4> hdr{};
    unsigned prepared=0;
    for(unsigned i=0;i<hdr.size();++i){
        hdr[i].lpData=reinterpret_cast<LPSTR>(pcm[i].data());hdr[i].dwBufferLength=sizeof(pcm[i]);
        if(waveOutPrepareHeader(dev,&hdr[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR)break;
        ++prepared;
    }
    audioStatus=prepared==hdr.size()?1:-1;
    while(!stop&&audioStatus==1){
        bool queued=false;
        for(unsigned i=0;i<hdr.size();++i)if(!(hdr[i].dwFlags&WHDR_INQUEUE)){
            exchange.read(c);e.render(c,pcm[i].data(),block,&head);
            if(waveOutWrite(dev,&hdr[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){audioStatus=-1;break;}
            queued=true;
        }
        if(!queued)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    waveOutReset(dev);
    for(unsigned i=0;i<prepared;++i)waveOutUnprepareHeader(dev,&hdr[i],sizeof(WAVEHDR));
    waveOutClose(dev);
}
static void publish(){exchange.publish(surface.c);}
static void contact(HWND h,bool down,LPARAM l){
    RECT r{};GetClientRect(h,&r);
    int x=GET_X_LPARAM(l)*320/std::max(1L,r.right);
    int y=GET_Y_LPARAM(l)*240/std::max(1L,r.bottom);
    surface.touch(down,x,y);publish();
}
static void cancel(){held=false;surface.cancel();publish();}
static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_LBUTTONDOWN:surface.touch(false,0,0);held=true;SetCapture(h);contact(h,true,l);return 0;
    case WM_MOUSEMOVE:if(held)contact(h,true,l);return 0;
    case WM_LBUTTONUP:held=false;contact(h,false,l);ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:if(held)cancel();return 0;
    case WM_KILLFOCUS:cancel();return 0;
    case WM_KEYDOWN:if(w==VK_ESCAPE){cancel();ReleaseCapture();}return 0;
    case WM_TIMER:{
        ++ticks;
        if(smoke){
            if(ticks==1){surface.touch(false,0,0);surface.touch(true,32,220);surface.touch(false,0,0);}
            if(ticks==8){surface.touch(true,48,25);surface.touch(true,170,25);surface.touch(false,0,0);}
            if(ticks==12){surface.touch(true,225,220);surface.touch(false,0,0);}
            if(ticks==24)surface.cancel();
        }
        publish();
        if(smoke){
            Control c;exchange.read(c);std::array<int16_t,block> b{};
            for(unsigned i=0;i<6;++i){smokeEngine.render(c,b.data(),b.size(),&head);
                if(std::any_of(b.begin(),b.end(),[](int16_t v){return v!=0;}))sounded=true;
                if(ticks>=24&&std::any_of(b.begin(),b.end(),[](int16_t v){return v!=0;})){PostQuitMessage(3);return 0;}
            }
        }
        if(smoke||ticks%3==0){
            InvalidateRect(h,nullptr,FALSE);UpdateWindow(h);
        }
        if(ticks%15==0){
            char title[160];std::snprintf(title,sizeof(title),"CHRONOGRID | %s | %u BPM | %s",
                scenes[surface.c.scene].name,surface.c.bpm,
                smoke?"graphical replay":(audioStatus==1?"audio active":(audioStatus<0?"audio unavailable":"audio opening")));
            SetWindowTextA(h,title);
        }
        if(smoke&&ticks>=35){KillTimer(h,1);PostQuitMessage(painted&&sounded?0:2);}
        return 0;}
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);
        std::array<uint16_t,width> line{};
        const auto snapshot=head.load(std::memory_order_acquire);
        for(unsigned y=0;y<height;++y){row(surface.c,snapshot,y,line.data());for(unsigned x=0;x<width;++x){
            auto v=line[x];pixels[y*width+x]=((v>>11)*255/31<<16)|(((v>>5)&63)*255/63<<8)|((v&31)*255/31);
        }}
        BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bmi.bmiHeader.biWidth=width;
        bmi.bmiHeader.biHeight=-int(height);bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;
        bmi.bmiHeader.biCompression=BI_RGB;SetStretchBltMode(dc,COLORONCOLOR);
        int rc=StretchDIBits(dc,0,0,r.right,r.bottom,0,0,width,height,pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        if(rc!=GDI_ERROR&&rc>0&&(!smoke||((snapshot&(1u<<16))&&ticks>5)))painted=true;
        EndPaint(h,&ps);return 0;}
    case WM_DESTROY:cancel();PostQuitMessage(0);return 0;
    }return DefWindowProcA(h,m,w,l);
}
int main(int argc,char** argv){
    smoke=argc>1&&std::strcmp(argv[1],"--smoke")==0;
    WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=GetModuleHandleA(nullptr);
    wc.lpszClassName="ChronoGridWindow";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);
    if(!RegisterClassA(&wc))return 1;
    HWND h=CreateWindowA(wc.lpszClassName,"CHRONOGRID",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
        660,520,nullptr,nullptr,wc.hInstance,nullptr);
    if(!h)return 1;ShowWindow(h,SW_SHOW);if(!SetTimer(h,1,smoke?33:12,nullptr))return 1;
    std::thread worker;if(!smoke)worker=std::thread(audio);
    MSG msg{};int ret=0;while((ret=GetMessageA(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    stop=true;if(worker.joinable())worker.join();return ret<0?1:int(msg.wParam);
}
