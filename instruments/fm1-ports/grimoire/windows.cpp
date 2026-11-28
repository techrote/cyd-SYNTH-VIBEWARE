#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <array>
#include <iostream>
#include <cstring>
#include "core.hpp"
using namespace grimoire;
namespace {
Engine engine;
HWAVEOUT device=nullptr;
constexpr int blocks=4, block=128;
std::array<std::array<std::int16_t,block>,blocks> audio{};
std::array<WAVEHDR,blocks> headers{};
std::array<std::uint32_t,width*height> frame{};
std::array<std::uint16_t,width> strip{};
bool dragging=false,editing=false,smoke=false,failed=false;
Path draft{}; int points=0,dir=1,tempo=1,paints=0; ULONGLONG last_paint=0;
void emergency(){engine.panic();dragging=editing=false;points=0;if(GetCapture())ReleaseCapture();if(device&&waveOutReset(device)!=MMSYSERR_NOERROR)failed=true;}
void close_audio(){if(!device)return;emergency();for(auto& h:headers)if(h.dwFlags&WHDR_PREPARED)if(waveOutUnprepareHeader(device,&h,sizeof(h))!=MMSYSERR_NOERROR)failed=true;if(waveOutClose(device)!=MMSYSERR_NOERROR)failed=true;device=nullptr;}
bool open_audio(){WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=rate*2;if(waveOutOpen(&device,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){device=nullptr;return false;}for(int i=0;i<blocks;++i){auto& h=headers[i];h.lpData=reinterpret_cast<LPSTR>(audio[i].data());h.dwBufferLength=block*2;if(waveOutPrepareHeader(device,&h,sizeof(h))!=MMSYSERR_NOERROR){close_audio();return false;}}return true;}
Point mapped(HWND hwnd,int x,int y){RECT r{};GetClientRect(hwnd,&r);int w=std::max(1L,r.right),h=std::max(1L,r.bottom);return clamp({(x*width/w-8)*127/303,(y*height/h-38)*127/193});}
void pump(HWND hwnd){if(failed)return;if(dragging&&!smoke){POINT p{};if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)||!GetCursorPos(&p)||!ScreenToClient(hwnd,&p)){engine.up();dragging=false;if(GetCapture()==hwnd)ReleaseCapture();}else engine.move(mapped(hwnd,p.x,p.y));}if(device){for(int i=0;i<blocks;++i){auto& h=headers[i];if(!(h.dwFlags&WHDR_INQUEUE)){engine.process(audio[i].data(),block);if(waveOutWrite(device,&h,sizeof(h))!=MMSYSERR_NOERROR){failed=true;emergency();break;}}}}const auto now=GetTickCount64();if(now-last_paint>=33){last_paint=now;InvalidateRect(hwnd,nullptr,FALSE);}}
void raster(){auto s=engine.snapshot();for(int y=0;y<height;++y){row(s,y,strip.data());for(int x=0;x<width;++x){auto c=strip[x];frame[y*width+x]=((c>>11)*255/31<<16)|(((c>>5)&63)*255/63<<8)|((c&31)*255/31);}}}
LRESULT CALLBACK wnd(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
case WM_LBUTTONDOWN:{RECT r{};GetClientRect(hwnd,&r);int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);if(y*height/std::max(1L,r.bottom)<32){int group=std::clamp(int(x*5/std::max(1L,r.right)),0,4);if(group<3){emergency();engine.material(group);}else if(group==3){if(engine.snapshot().auto_on){emergency();}else if(!failed)engine.start_orbit();}else emergency();}else if(!failed){auto p=mapped(hwnd,x,y);if(editing){draft[points++]=p;if(points==8){engine.set_path(draft);editing=false;points=0;}}else{dragging=true;SetCapture(hwnd);engine.down(p);}}InvalidateRect(hwnd,nullptr,FALSE);return 0;}
case WM_MOUSEMOVE:if(dragging)engine.move(mapped(hwnd,GET_X_LPARAM(lp),GET_Y_LPARAM(lp)));return 0;
case WM_LBUTTONUP:if(dragging){engine.up();dragging=false;ReleaseCapture();}return 0;
case WM_CAPTURECHANGED:if(dragging)emergency();return 0;
case WM_KILLFOCUS:emergency();return 0;
case WM_KEYDOWN:if(wp==VK_ESCAPE||wp==' '){emergency();}else if(wp=='R')engine.set_path(ribbon);else if(wp=='C')engine.set_path(crown);else if(wp=='P')engine.set_path(pendulum);else if(wp=='V'){dir=-dir;engine.direction(dir);}else if(wp=='S'){tempo=tempo==4?1:tempo*2;engine.speed(tempo);}else if(wp=='E'){emergency();editing=true;}InvalidateRect(hwnd,nullptr,FALSE);return 0;
case WM_TIMER:pump(hwnd);return 0;
case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);raster();BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;StretchDIBits(dc,0,0,r.right,r.bottom,0,0,width,height,frame.data(),&bi,DIB_RGB_COLORS,SRCCOPY);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));const char* labels[]={"Amber","Iron","Abyss","Orbit","Panic"};for(int i=0;i<5;++i)TextOutA(dc,int(r.right*i/5+4),5,labels[i],int(std::strlen(labels[i])));const char* hint=failed?"Audio fault: closed / restart app":editing?"Tap 8 points to replace path":"R/C/P shapes | V reverse | S speed | E sketch | Esc panic";TextOutA(dc,4,int(r.bottom*32/height),hint,int(std::strlen(hint)));EndPaint(hwnd,&ps);++paints;return 0;}
case WM_DESTROY:KillTimer(hwnd,1);close_audio();PostQuitMessage(0);return 0;
}return DefWindowProcA(hwnd,msg,wp,lp);}
}
int main(int argc,char** argv){smoke=argc==2&&std::strcmp(argv[1],"--smoke")==0;auto inst=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=inst;wc.lpszClassName="GrimoireStandalone";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);if(!RegisterClassA(&wc))return 2;HWND hwnd=CreateWindowA(wc.lpszClassName,"Grimoire CYD - Orbit Gate",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,660,530,nullptr,nullptr,inst,nullptr);if(!hwnd)return 3;ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);if(smoke){auto before=frame;RECT r{};GetClientRect(hwnd,&r);SendMessage(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(r.right/2,r.bottom/2));int nonzero=0;for(int i=0;i<4096;++i)nonzero+=engine.sample()!=0;SendMessage(hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(r.right*3/4,r.bottom/3));for(int i=0;i<1024;++i)engine.sample();InvalidateRect(hwnd,nullptr,FALSE);UpdateWindow(hwnd);bool changed=frame!=before;SendMessage(hwnd,WM_LBUTTONUP,0,0);for(int i=0;i<4096;++i)engine.sample();bool released=engine.snapshot().envelope==0;SendMessage(hwnd,WM_KEYDOWN,VK_ESCAPE,0);bool quiet=true;for(int i=0;i<4096;++i)quiet&=engine.sample()==0;bool ok=nonzero>1000&&changed&&released&&quiet&&paints>=2;std::cout<<"GUI smoke nonzero="<<nonzero<<" changed="<<changed<<" released="<<released<<" quiet="<<quiet<<" paints="<<paints<<'\n';DestroyWindow(hwnd);return ok?0:4;}if(!open_audio()){MessageBoxA(hwnd,"Cannot open 24 kHz mono audio device.","Grimoire",MB_OK|MB_ICONERROR);DestroyWindow(hwnd);return 5;}timeBeginPeriod(1);if(!SetTimer(hwnd,1,5,nullptr)){DestroyWindow(hwnd);timeEndPeriod(1);return 6;}MSG msg{};while(GetMessageA(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}timeEndPeriod(1);return failed?7:0;}
