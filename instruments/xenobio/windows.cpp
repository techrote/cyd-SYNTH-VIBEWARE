#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include "runtime.hpp"
#include "labeled.hpp"
#include <chrono>
#include <cstring>
#include <thread>
using namespace xenobio;
static Runtime runtime;
static std::atomic<bool> stopping{false};
static std::atomic<int> audioStatus{0};
static std::array<uint32_t,320*240> pixels{};
static bool held=false,smoke=false,painted=false,paintedPanic=false,sounded=false,resumed=false;
static unsigned ticks=0;
using Block=std::array<int16_t,128>;
static bool nonzero(const Block& b){return std::any_of(b.begin(),b.end(),[](int v){return v!=0;});}
static void audio(){
    WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;
    fmt.nSamplesPerSec=rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=rate*2;
    HWAVEOUT dev=nullptr;
    if(waveOutOpen(&dev,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){audioStatus=-1;return;}
    std::array<Block,4> pcm{};std::array<WAVEHDR,4> hdr{};unsigned prepared=0;
    for(unsigned i=0;i<hdr.size();++i){hdr[i].lpData=reinterpret_cast<LPSTR>(pcm[i].data());hdr[i].dwBufferLength=sizeof(Block);
        if(waveOutPrepareHeader(dev,&hdr[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR)break;++prepared;}
    audioStatus=prepared==hdr.size()?1:-1;
    while(!stopping.load()&&audioStatus.load()==1){
        // Discard queued hardware blocks on a known emergency. No driver I/O in Runtime::render.
        if(runtime.input.emergency_pending()&&waveOutReset(dev)!=MMSYSERR_NOERROR){audioStatus=-1;break;}
        bool queued=false;
        for(unsigned i=0;i<hdr.size();++i)if(!(hdr[i].dwFlags&WHDR_INQUEUE)){
            runtime.render(pcm[i].data(),pcm[i].size());
            if(waveOutWrite(dev,&hdr[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){audioStatus=-1;break;}queued=true;
        }
        if(!queued)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    waveOutReset(dev);for(unsigned i=0;i<prepared;++i)waveOutUnprepareHeader(dev,&hdr[i],sizeof(WAVEHDR));waveOutClose(dev);
}
static void point(HWND h,LPARAM l,bool down){
    RECT r{};GetClientRect(h,&r);int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
    if(x<0||y<0||x>=r.right||y>=r.bottom||r.right<=0||r.bottom<=0){x=-1;y=-1;}
    else{x=x*320/r.right;y=y*240/r.bottom;}
    if(down)runtime.input.down(x,y);else runtime.input.move(x,y);
}
static void cancel(){held=false;runtime.input.cancel();}
static LPARAM coord(HWND h,int x,int y){RECT r{};GetClientRect(h,&r);return MAKELPARAM((2*x+1)*r.right/640,(2*y+1)*r.bottom/480);}
static void click(HWND h,int x,int y){auto p=coord(h,x,y);SendMessageA(h,WM_LBUTTONDOWN,MK_LBUTTON,p);SendMessageA(h,WM_LBUTTONUP,0,p);}
static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_LBUTTONDOWN:if(!held){held=true;SetCapture(h);point(h,l,true);}return 0;
    case WM_MOUSEMOVE:if(held)point(h,l,false);return 0;
    case WM_LBUTTONUP:if(held){held=false;runtime.input.up();ReleaseCapture();}return 0;
    case WM_CAPTURECHANGED:if(held)cancel();return 0;
    case WM_CANCELMODE:case WM_KILLFOCUS:cancel();if(GetCapture()==h)ReleaseCapture();return 0;
    case WM_KEYDOWN:if(w==VK_ESCAPE){cancel();if(GetCapture()==h)ReleaseCapture();}return 0;
    case WM_TIMER:{
        ++ticks;
        if(smoke){
            if(ticks==1){Block b{};runtime.render(b.data(),b.size());click(h,130,10); }
            if(ticks==8){click(h,120,230);SendMessageA(h,WM_LBUTTONDOWN,MK_LBUTTON,coord(h,120,100));}
            if(ticks==9)SendMessageA(h,WM_MOUSEMOVE,MK_LBUTTON,coord(h,210,140));
            if(ticks==10)SendMessageA(h,WM_LBUTTONUP,0,coord(h,210,140));
            if(ticks==25){SendMessageA(h,WM_LBUTTONDOWN,MK_LBUTTON,coord(h,120,100));SendMessageA(h,WM_MOUSEMOVE,MK_LBUTTON,coord(h,310,230));SendMessageA(h,WM_LBUTTONUP,0,coord(h,310,230));}
            if(ticks==28)click(h,250,230);
            for(unsigned i=0;i<6;++i){Block b{};runtime.render(b.data(),b.size());
                if(ticks<25)sounded|=nonzero(b);
                if(ticks>=25&&ticks<28&&nonzero(b)){PostQuitMessage(3);return 0;}
                if(ticks>=28)resumed|=nonzero(b);
            }
            if(ticks>=25&&ticks<28&&!runtime.snapshot().panic){PostQuitMessage(4);return 0;}
        }
        if(smoke||ticks%3==0){InvalidateRect(h,nullptr,FALSE);UpdateWindow(h);}
        if(!smoke&&ticks%20==0){const int s=audioStatus.load();SetWindowTextA(h,s==1?"XENOBIO | audio active | select a scene":s<0?"XENOBIO | audio unavailable":"XENOBIO | opening audio");}
        if(smoke&&ticks>=55){KillTimer(h,1);PostQuitMessage(painted&&paintedPanic&&sounded&&resumed?0:2);}
        return 0;}
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);
        const auto e=runtime.snapshot();std::array<uint16_t,320> line{};
        for(int y=0;y<240;++y){labeled_row(e,y,line.data());for(int x=0;x<320;++x){auto v=line[x];pixels[y*320+x]=((v>>11)*255/31<<16)|(((v>>5)&63)*255/63<<8)|((v&31)*255/31);}}
        BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bmi.bmiHeader.biWidth=320;bmi.bmiHeader.biHeight=-240;bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
        SetStretchBltMode(dc,COLORONCOLOR);const int result=StretchDIBits(dc,0,0,r.right,r.bottom,0,0,320,240,pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        if(result>0){if(e.notes>0&&!e.panic)painted=true;if(ticks>=25&&ticks<28&&e.panic)paintedPanic=true;}
        EndPaint(h,&ps);return 0;}
    case WM_DESTROY:cancel();PostQuitMessage(smoke?5:0);return 0;
    }return DefWindowProcA(h,m,w,l);
}
int main(int argc,char** argv){
    smoke=argc>1&&std::strcmp(argv[1],"--smoke")==0;
    WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=GetModuleHandleA(nullptr);wc.lpszClassName="XenobioWindow";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);
    if(!RegisterClassA(&wc))return 1;
    HWND h=CreateWindowA(wc.lpszClassName,"XENOBIO | select a scene",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,660,520,nullptr,nullptr,wc.hInstance,nullptr);
    if(!h)return 1;ShowWindow(h,SW_SHOW);if(!SetTimer(h,1,smoke?33:12,nullptr))return 1;
    std::thread worker;if(!smoke)worker=std::thread(audio);
    MSG msg{};int result=0;while((result=GetMessageA(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    stopping.store(true);if(worker.joinable())worker.join();return result<0?1:int(msg.wParam);
}
