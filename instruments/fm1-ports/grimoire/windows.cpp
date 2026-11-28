#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <array>
#include <iostream>
#include <cstring>
#include "panel.hpp"
using namespace grimoire;
namespace {
Engine engine;
Touch touch(engine);
HWAVEOUT device=nullptr;
constexpr int blocks=4, block=128;
std::array<std::array<std::int16_t,block>,blocks> audio{};
std::array<WAVEHDR,blocks> headers{};
std::array<std::uint32_t,width*height> frame{};
PanelRaster rasterizer;
std::array<std::uint16_t,width*8> strip{};
bool smoke=false,failed=false;
int paints=0; ULONGLONG last_paint=0;
void flush_audio(){if(device&&waveOutReset(device)!=MMSYSERR_NOERROR)failed=true;}
void emergency(){touch.cancel();if(GetCapture())ReleaseCapture();flush_audio();}
void close_audio(){if(!device)return;emergency();for(auto& h:headers)if(h.dwFlags&WHDR_PREPARED)if(waveOutUnprepareHeader(device,&h,sizeof(h))!=MMSYSERR_NOERROR)failed=true;if(waveOutClose(device)!=MMSYSERR_NOERROR)failed=true;device=nullptr;}
bool open_audio(){WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=rate*2;if(waveOutOpen(&device,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){device=nullptr;return false;}for(int i=0;i<blocks;++i){auto& h=headers[i];h.lpData=reinterpret_cast<LPSTR>(audio[i].data());h.dwBufferLength=block*2;if(waveOutPrepareHeader(device,&h,sizeof(h))!=MMSYSERR_NOERROR){close_audio();return false;}}return true;}
Point mapped(HWND hwnd,int x,int y){RECT r{};GetClientRect(hwnd,&r);int w=std::max(1L,r.right),h=std::max(1L,r.bottom);return {x*width/w,y*height/h};}
void pump(HWND hwnd){if(failed)return;if(touch.contact()&&!smoke){POINT p{};if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)||!GetCursorPos(&p)||!ScreenToClient(hwnd,&p)){emergency();}else {auto q=mapped(hwnd,p.x,p.y);if(touch.move(q.x,q.y))flush_audio();}}if(device){for(int i=0;i<blocks;++i){auto& h=headers[i];if(!(h.dwFlags&WHDR_INQUEUE)){engine.process(audio[i].data(),block);if(waveOutWrite(device,&h,sizeof(h))!=MMSYSERR_NOERROR){failed=true;emergency();break;}}}}const auto now=GetTickCount64();if(now-last_paint>=33){last_paint=now;InvalidateRect(hwnd,nullptr,FALSE);}}
void raster(){
    rasterizer.begin(capture_panel(engine,touch));
    for(int y=0;y<height;y+=8){
        if(!rasterizer.render(y,8,strip.data())){failed=true;return;}
        for(int i=0;i<width*8;++i){
            const auto c=strip[i];
            frame[y*width+i]=((c>>11)*255/31<<16)|(((c>>5)&63)*255/63<<8)|((c&31)*255/31);
        }
    }
}
LRESULT CALLBACK wnd(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
case WM_LBUTTONDOWN:if(!failed){auto p=mapped(hwnd,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));if(touch.down(p.x,p.y))flush_audio();if(touch.contact())SetCapture(hwnd);}InvalidateRect(hwnd,nullptr,FALSE);return 0;
case WM_MOUSEMOVE:if(touch.contact()){auto p=mapped(hwnd,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));if(touch.move(p.x,p.y))flush_audio();}return 0;
case WM_LBUTTONUP:touch.up();if(GetCapture()==hwnd)ReleaseCapture();return 0;
case WM_CAPTURECHANGED:if(touch.contact())emergency();return 0;
case WM_KILLFOCUS:emergency();return 0;
case WM_KEYDOWN:
    if(wp==VK_ESCAPE||wp==' ')emergency();
    else if(!failed&&!touch.contact()){
        if(wp=='R')touch.control(Control::Ribbon);
        else if(wp=='C')touch.control(Control::Crown);
        else if(wp=='P')touch.control(Control::Pendulum);
        else if(wp=='V')touch.control(Control::Reverse);
        else if(wp=='S')touch.control(Control::Speed);
        else if(wp=='E'&&touch.control(Control::Sketch))flush_audio();
    }
    InvalidateRect(hwnd,nullptr,FALSE);return 0;
case WM_TIMER:pump(hwnd);return 0;
case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);raster();BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;StretchDIBits(dc,0,0,r.right,r.bottom,0,0,width,height,frame.data(),&bi,DIB_RGB_COLORS,SRCCOPY);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));const char* labels[]={"Amber","Iron","Abyss","Orbit","Panic"};for(int i=0;i<5;++i)TextOutA(dc,int(r.right*i/5+4),5,labels[i],int(std::strlen(labels[i])));const char* controls[]={"Ribbon","Crown","Pendulum","Sketch",touch.direction()<0?"Reverse":"Forward",touch.speed()==4?"Speed 4":touch.speed()==2?"Speed 2":"Speed 1"};for(int i=0;i<6;++i)TextOutA(dc,int(r.right*i/6+3),int(r.bottom*211/height),controls[i],int(std::strlen(controls[i])));const char* hint=failed?"Audio fault: restart app":touch.editing()?"Tap 8 points to replace path":"XY play | Orbit latch | Sketch 8 points | Panic";TextOutA(dc,4,int(r.bottom*32/height),hint,int(std::strlen(hint)));EndPaint(hwnd,&ps);++paints;return 0;}
case WM_DESTROY:KillTimer(hwnd,1);close_audio();PostQuitMessage(0);return 0;
}return DefWindowProcA(hwnd,msg,wp,lp);}
}
int main(int argc,char** argv){smoke=argc==2&&std::strcmp(argv[1],"--smoke")==0;auto inst=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=inst;wc.lpszClassName="GrimoireStandalone";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);if(!RegisterClassA(&wc))return 2;HWND hwnd=CreateWindowA(wc.lpszClassName,"Grimoire CYD - Orbit Gate",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,660,530,nullptr,nullptr,inst,nullptr);if(!hwnd)return 3;ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);if(smoke){auto before=frame;RECT r{};GetClientRect(hwnd,&r);SendMessage(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(r.right/2,r.bottom/2));int nonzero=0;for(int i=0;i<4096;++i)nonzero+=engine.sample()!=0;SendMessage(hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(r.right*3/4,r.bottom/3));for(int i=0;i<1024;++i)engine.sample();InvalidateRect(hwnd,nullptr,FALSE);UpdateWindow(hwnd);bool changed=frame!=before;SendMessage(hwnd,WM_LBUTTONUP,0,0);for(int i=0;i<4096;++i)engine.sample();bool released=engine.snapshot().envelope==0;SendMessage(hwnd,WM_KEYDOWN,VK_ESCAPE,0);bool quiet=true;for(int i=0;i<4096;++i)quiet&=engine.sample()==0;auto tap=[&](int x,int y){SendMessage(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x*r.right/width,y*r.bottom/height));SendMessage(hwnd,WM_LBUTTONUP,0,0);};tap(80,220);bool toolbar=touch.shape()==1;tap(240,220);toolbar=toolbar&&touch.direction()==-1;tap(295,220);toolbar=toolbar&&touch.speed()==2;tap(185,220);toolbar=toolbar&&touch.editing();for(int i=0;i<8;++i)tap(20+i*40,50+i*20);toolbar=toolbar&&!touch.editing()&&touch.shape()==3;tap(200,10);toolbar=toolbar&&engine.snapshot().auto_on;tap(300,10);toolbar=toolbar&&engine.snapshot().muted;bool ok=nonzero>1000&&changed&&released&&quiet&&paints>=2&&toolbar&&!failed;std::cout<<"GUI smoke nonzero="<<nonzero<<" changed="<<changed<<" released="<<released<<" quiet="<<quiet<<" toolbar="<<toolbar<<" paints="<<paints<<'\n';DestroyWindow(hwnd);return ok?0:4;}if(!open_audio()){MessageBoxA(hwnd,"Cannot open 24 kHz mono audio device.","Grimoire",MB_OK|MB_ICONERROR);DestroyWindow(hwnd);return 5;}timeBeginPeriod(1);if(!SetTimer(hwnd,1,5,nullptr)){DestroyWindow(hwnd);timeEndPeriod(1);return 6;}MSG msg{};while(GetMessageA(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}timeEndPeriod(1);return failed?7:0;}
