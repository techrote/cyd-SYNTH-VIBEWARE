#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <thread>
#include "exchange.hpp"
using namespace portx;
static Exchange bus;
static std::atomic<bool> closing{false},failed{false};
static bool down=false,smoke=false;static int x=0,y=0,paints=0,ticks=0;
static void audio(){
 Engine e;uint32_t last=0,alarm=0;
 if(smoke){int16_t b[128];while(!closing){bus.pump(e,last,alarm);e.render(b,128);bus.publish(e.view());Sleep(5);}return;}
 WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=rate*2;
 HWAVEOUT dev=nullptr;if(waveOutOpen(&dev,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){failed=true;return;}
 int16_t pcm[3][128]{};WAVEHDR h[3]{};unsigned prepared=0;
 for(unsigned i=0;i<3;++i){h[i].lpData=reinterpret_cast<LPSTR>(pcm[i]);h[i].dwBufferLength=sizeof(pcm[i]);if(waveOutPrepareHeader(dev,&h[i],sizeof(h[i]))!=MMSYSERR_NOERROR){failed=true;break;}++prepared;}
 while(!closing&&!failed){bool work=false;for(unsigned i=0;i<prepared;++i){if(h[i].dwFlags&WHDR_INQUEUE)continue;bus.pump(e,last,alarm);e.render(pcm[i],128);bus.publish(e.view());if(waveOutWrite(dev,&h[i],sizeof(h[i]))!=MMSYSERR_NOERROR){failed=true;break;}work=true;}if(!work)Sleep(1);}
 waveOutReset(dev);for(unsigned i=0;i<prepared;++i)waveOutUnprepareHeader(dev,&h[i],sizeof(h[i]));waveOutClose(dev);
}
static void send(){if(down&&y>=184&&x>=266){bus.stop();down=false;}else bus.send(down,x,y);}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b){
 switch(m){
 case WM_CREATE:SetTimer(w,1,16,nullptr);return 0;
 case WM_LBUTTONDOWN:down=true;SetCapture(w);[[fallthrough]];
 case WM_MOUSEMOVE:{if(!down)return 0;RECT r;GetClientRect(w,&r);x=GET_X_LPARAM(b)*320/std::max(1L,r.right);y=GET_Y_LPARAM(b)*240/std::max(1L,r.bottom);send();return 0;}
 case WM_LBUTTONUP:down=false;send();ReleaseCapture();return 0;
 case WM_CAPTURECHANGED:if(down){down=false;bus.stop();}return 0;
 case WM_CANCELMODE:case WM_KILLFOCUS:down=false;bus.stop();return 0;
 case WM_KEYDOWN:if(a==VK_ESCAPE){down=false;bus.stop();}return 0;
 case WM_TIMER:++ticks;if(smoke){down=ticks<18;x=110;y=30;}send();InvalidateRect(w,nullptr,FALSE);if(smoke&&ticks>=36)DestroyWindow(w);return 0;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);static uint32_t rgb[320*240];auto v=bus.read();for(int j=0;j<240;++j)for(int i=0;i<320;++i){auto c=pixel(v,i,j);rgb[j*320+i]=(((c>>11)&31)*255/31<<16)|(((c>>5)&63)*255/63<<8)|((c&31)*255/31);}BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=320;bi.bmiHeader.biHeight=-240;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;StretchDIBits(dc,0,0,r.right,r.bottom,0,0,320,240,rgb,&bi,DIB_RGB_COLORS,SRCCOPY);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));const char* labels[]={"Ceramic","Woodglass","Alien","Membrane","Couple","Panic"};for(int i=0;i<6;++i)TextOutA(dc,(i*53+3)*r.right/320,208*r.bottom/240,labels[i],lstrlenA(labels[i]));const char* title=failed?"Audio unavailable - close and retry":"Strike / drag: pitch at strike, damping Y, coupling X";TextOutA(dc,4,4,title,lstrlenA(title));EndPaint(w,&ps);++paints;return 0;}
 case WM_DESTROY:closing=true;bus.stop();PostQuitMessage(0);return 0;
 }return DefWindowProcA(w,m,a,b);
}
int main(int argc,char**){smoke=argc>1;auto inst=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=inst;wc.lpszClassName="PortXeno";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);if(!RegisterClassA(&wc))return 2;auto w=CreateWindowA(wc.lpszClassName,"Xenoresonator CYD interpretation",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,660,530,nullptr,nullptr,inst,nullptr);if(!w)return 3;std::thread t(audio);MSG msg;while(GetMessageA(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}closing=true;t.join();return smoke&&paints<5?4:0;}
