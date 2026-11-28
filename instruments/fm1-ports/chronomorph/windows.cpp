#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <thread>
#include "runtime.hpp"
using namespace chronocyd;
static Input controls;
static Ring<Engine,4> snapshots;
static std::atomic<bool> closing{false},failed{false},audible{false},silentAfterPlay{false};
static bool smoke=false,held=false;
static int x=0,y=0,frames=0,ticks=0;
static void fill(Runtime& r,int16_t* pcm){
 r.render(controls,GetTickCount(),pcm,128);snapshots.push(r.engine);
 bool sound=std::any_of(pcm,pcm+128,[](int16_t v){return v!=0;});
 if(sound)audible=true;
 if(audible&&!r.engine.playing&&!sound)silentAfterPlay=true;
}
static void audio(){
 Runtime r;
 if(smoke){int16_t pcm[128];while(!closing){fill(r,pcm);Sleep(5);}return;}
 WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=rate*2;
 HWAVEOUT dev=nullptr;
 if(waveOutOpen(&dev,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){failed=true;return;}
 int16_t pcm[3][128]{};WAVEHDR h[3]{};unsigned prepared=0;
 for(unsigned i=0;i<3;++i){h[i].lpData=reinterpret_cast<LPSTR>(pcm[i]);h[i].dwBufferLength=sizeof(pcm[i]);if(waveOutPrepareHeader(dev,&h[i],sizeof(h[i]))!=MMSYSERR_NOERROR){failed=true;break;}++prepared;}
 while(!closing&&!failed){bool work=false;for(unsigned i=0;i<prepared;++i){if(h[i].dwFlags&WHDR_INQUEUE)continue;fill(r,pcm[i]);if(waveOutWrite(dev,&h[i],sizeof(h[i]))!=MMSYSERR_NOERROR){failed=true;break;}work=true;}if(!work)Sleep(1);}
 waveOutReset(dev);for(unsigned i=0;i<prepared;++i)waveOutUnprepareHeader(dev,&h[i],sizeof(h[i]));waveOutClose(dev);
}
static void send(){controls.send(held,x,y,GetTickCount());}
static void cancel(){held=false;controls.emergency();}
static LPARAM position(HWND w,int px,int py){RECT r{};GetClientRect(w,&r);return MAKELPARAM(px*r.right/320,py*r.bottom/240);}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b){
 switch(m){
 case WM_CREATE:if(!SetTimer(w,1,16,nullptr))return -1;return 0;
 case WM_LBUTTONDOWN:held=true;SetCapture(w);[[fallthrough]];
 case WM_MOUSEMOVE:{if(!held)return 0;RECT r{};GetClientRect(w,&r);x=GET_X_LPARAM(b)*320/std::max(1L,r.right);y=GET_Y_LPARAM(b)*240/std::max(1L,r.bottom);send();return 0;}
 case WM_LBUTTONUP:held=false;send();ReleaseCapture();return 0;
 case WM_CAPTURECHANGED:if(held)cancel();return 0;
 case WM_CANCELMODE:case WM_KILLFOCUS:cancel();return 0;
 case WM_KEYDOWN:if(a==VK_ESCAPE){cancel();ReleaseCapture();}return 0;
 case WM_TIMER:
  ++ticks;
  if(smoke){if(ticks==2)SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,position(w,180,220));if(ticks==3)SendMessageA(w,WM_LBUTTONUP,0,0);if(ticks==20)SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,position(w,250,12));if(ticks==21)SendMessageA(w,WM_LBUTTONUP,0,0);if(ticks==55)SendMessageA(w,WM_KEYDOWN,VK_ESCAPE,0);if(ticks==70){DestroyWindow(w);return 0;}}
  send();InvalidateRect(w,nullptr,FALSE);return 0;
 case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);static Engine view;Engine v;for(unsigned n=0;n<3&&snapshots.pop(v);++n)view=v;static uint32_t rgb[320*240];uint16_t line[320];for(int j=0;j<240;++j){row(view,j,line);for(int i=0;i<320;++i){auto c=line[i];rgb[j*320+i]=(((c>>11)&31)*255/31<<16)|(((c>>5)&63)*255/63<<8)|((c&31)*255/31);}}
  BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=320;bi.bmiHeader.biHeight=-240;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
  if(StretchDIBits(dc,0,0,r.right,r.bottom,0,0,320,240,rgb,&bi,DIB_RGB_COLORS,SRCCOPY)!=GDI_ERROR)++frames;
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));const char* labels[]={"I","II","III","IV","Play","Reset","Var","Panic"};for(int i=0;i<8;++i)TextOutA(dc,(i*40+3)*r.right/320,216*r.bottom/240,labels[i],lstrlenA(labels[i]));const char* title=failed?"Audio unavailable - close and retry":"X: tempo / Y: warp";TextOutA(dc,4,4,title,lstrlenA(title));EndPaint(w,&ps);return 0;}
 case WM_DESTROY:KillTimer(w,1);cancel();closing=true;PostQuitMessage(0);return 0;
 }return DefWindowProcA(w,m,a,b);
}
int main(int argc,char**){smoke=argc>1;auto inst=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=inst;wc.lpszClassName="ChronomorphCYD";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);if(!RegisterClassA(&wc))return 2;auto w=CreateWindowA(wc.lpszClassName,"Chronomorph CYD - elastic clocks",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,660,530,nullptr,nullptr,inst,nullptr);if(!w)return 3;std::thread t(audio);MSG msg{};int result;while((result=GetMessageA(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageA(&msg);}closing=true;t.join();if(result<0||failed)return 4;return smoke&&(!audible||!silentAfterPlay||frames<5)?5:0;}
