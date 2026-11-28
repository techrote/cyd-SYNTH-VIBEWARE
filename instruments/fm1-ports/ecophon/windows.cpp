#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <thread>
#include "runtime.hpp"
using namespace ecophon;
static Input input;
static Views views;
static std::atomic<bool> closing{false}, failed{false}, played{false}, changed{false}, stopped{false}, restarted{false};
static bool smoke=false,held=false,pendingRelease=false;
static int x=0,y=0,frames=0,timers=0;
static void generate(Runtime& rt,int16_t* pcm){
    rt.render(input,GetTickCount(),pcm,128);
    views.publish(rt.engine);
    const bool sound=std::any_of(pcm,pcm+128,[](int16_t v){return v!=0;});
    if(sound){played=true;if(stopped)restarted=true;}
    if(played&&rt.engine.env.fertility>=240&&!rt.engine.muted)changed=true;
    if(changed&&rt.engine.muted&&!sound)stopped=true;
}
static void audio(){
    Runtime rt;views.publish(rt.engine);
    if(smoke){int16_t pcm[128];while(!closing){generate(rt,pcm);Sleep(5);}return;}
    WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=rate;
    fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=rate*2;
    HWAVEOUT dev=nullptr;
    if(waveOutOpen(&dev,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){failed=true;return;}
    int16_t pcm[3][128]{};WAVEHDR hdr[3]{};unsigned prepared=0;
    for(unsigned i=0;i<3;++i){
        hdr[i].lpData=reinterpret_cast<LPSTR>(pcm[i]);hdr[i].dwBufferLength=sizeof(pcm[i]);
        if(waveOutPrepareHeader(dev,&hdr[i],sizeof(hdr[i]))!=MMSYSERR_NOERROR){failed=true;break;}
        ++prepared;
    }
    while(!closing&&!failed){
        bool work=false;
        for(unsigned i=0;i<prepared&&!closing;++i){
            if(hdr[i].dwFlags&WHDR_INQUEUE)continue;
            const bool wasMuted=rt.engine.muted;
            generate(rt,pcm[i]);
            // Backend drain, outside the portable runtime; no claim about physical latency.
            if(!wasMuted&&rt.engine.muted&&waveOutReset(dev)!=MMSYSERR_NOERROR){failed=true;break;}
            if(waveOutWrite(dev,&hdr[i],sizeof(hdr[i]))!=MMSYSERR_NOERROR){failed=true;break;}
            work=true;
        }
        if(!work)Sleep(1);
    }
    if(waveOutReset(dev)!=MMSYSERR_NOERROR)failed=true;
    for(unsigned i=0;i<prepared;++i)if(waveOutUnprepareHeader(dev,&hdr[i],sizeof(hdr[i]))!=MMSYSERR_NOERROR)failed=true;
    if(waveOutClose(dev)!=MMSYSERR_NOERROR)failed=true;
}
static void send(){
    const auto now=GetTickCount();
    if(pendingRelease){if(!input.submit(false,0,0,now))return;pendingRelease=false;}
    if(held)input.submit(true,x,y,now);
}
static void cancel(){
    const bool wasHeld=held;
    held=false;pendingRelease=!wasHeld;
    input.emergency();
}
static void coords(HWND w,LPARAM pos){
    RECT r{};GetClientRect(w,&r);
    int px=GET_X_LPARAM(pos),py=GET_Y_LPARAM(pos);
    if(px<0||py<0||px>=r.right||py>=r.bottom){x=y=-1;return;}
    x=int(px*320L/std::max(1L,r.right));y=int(py*240L/std::max(1L,r.bottom));
}
static LPARAM position(HWND w,int px,int py){
    RECT r{};GetClientRect(w,&r);
    return MAKELPARAM((px*r.right+319)/320,(py*r.bottom+239)/240);
}
static void label(HDC dc,const RECT& r,int x0,int y0,int x1,int y1,const char* s){
    RECT t{x0*r.right/320,y0*r.bottom/240,x1*r.right/320,y1*r.bottom/240};
    DrawTextA(dc,s,-1,&t,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
}
static LRESULT CALLBACK wnd(HWND w,UINT msg,WPARAM a,LPARAM b){
    switch(msg){
    case WM_CREATE:return SetTimer(w,1,16,nullptr)?0:-1;
    case WM_LBUTTONDOWN:held=true;SetCapture(w);coords(w,b);send();return 0;
    case WM_MOUSEMOVE:if(held){coords(w,b);send();}return 0;
    case WM_LBUTTONUP:held=false;pendingRelease=true;send();ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:if(held)cancel();return 0;
    case WM_CANCELMODE:case WM_KILLFOCUS:cancel();if(GetCapture()==w)ReleaseCapture();return 0;
    case WM_KEYDOWN:if(a==VK_ESCAPE){cancel();if(GetCapture()==w)ReleaseCapture();}return 0;
    case WM_TIMER:
        ++timers;
        if(smoke){
            if(timers==3)SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,position(w,20,10));
            if(timers==5)SendMessageA(w,WM_LBUTTONUP,0,0);
            if(timers==12)SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,position(w,300,150));
            if(timers==14)SendMessageA(w,WM_MOUSEMOVE,MK_LBUTTON,position(w,310,150));
            if(timers==24)SendMessageA(w,WM_MOUSEMOVE,MK_LBUTTON,position(w,300,230));
            if(timers==28)SendMessageA(w,WM_LBUTTONUP,0,0);
            if(timers==40)SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,position(w,180,10));
            if(timers==43)SendMessageA(w,WM_LBUTTONUP,0,0);
            if(timers==68){DestroyWindow(w);return 0;}
        }
        send();InvalidateRect(w,nullptr,FALSE);return 0;
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);
        static Engine view=[](){Engine e;e.reset(0,42);e.panic();return e;}();
        views.latest(view);
        static uint32_t rgb[320*240];uint16_t line[320];
        for(unsigned j=0;j<240;++j){row(view,j,line);for(unsigned i=0;i<320;++i){
            const auto c=line[i];rgb[j*320+i]=(((c>>11)&31)*255/31<<16)|(((c>>5)&63)*255/63<<8)|((c&31)*255/31);
        }}
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth=320;bi.bmiHeader.biHeight=-240;bi.bmiHeader.biPlanes=1;
        bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        const int drawn=StretchDIBits(dc,0,0,r.right,r.bottom,0,0,320,240,rgb,&bi,DIB_RGB_COLORS,SRCCOPY);
        if(drawn>0)++frames;
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
        const char* scenes[]={"Mutualism","Scarcity","Bloom","Frozen"};
        const char* memories[]={"Capture A","Capture B","Recall A","Recall B"};
        for(int i=0;i<4;++i){label(dc,r,i*80,0,(i+1)*80,24,scenes[i]);label(dc,r,i*80,192,(i+1)*80,216,memories[i]);}
        label(dc,r,0,80,320,112,"Sharing");
        label(dc,r,0,112,320,126,"Pressure up / Fertility right");
        label(dc,r,0,216,280,240,failed?"Audio unavailable - close and retry":view.muted?"Muted - tap scene":view.frozen?"Resume ecology":"Freeze ecology");
        label(dc,r,280,216,320,240,"Panic");
        EndPaint(w,&ps);return 0;
    }
    case WM_DESTROY:KillTimer(w,1);input.emergency();closing=true;PostQuitMessage(0);return 0;
    }
    return DefWindowProcA(w,msg,a,b);
}
int main(int argc,char** argv){
    if(argc>2||(argc==2&&lstrcmpA(argv[1],"--smoke")!=0))return 1;
    smoke=argc==2;
    auto instance=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.lpfnWndProc=wnd;wc.hInstance=instance;
    wc.lpszClassName="EcophonCYD";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);
    if(!RegisterClassA(&wc))return 2;
    auto window=CreateWindowA(wc.lpszClassName,"Ecophon CYD - acoustic habitats",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,660,530,nullptr,nullptr,instance,nullptr);
    if(!window)return 3;
    std::thread worker(audio);MSG msg{};int result;
    while((result=GetMessageA(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    closing=true;worker.join();
    if(result<0||failed)return 4;
    return smoke&&(!played||!changed||!stopped||!restarted||frames<5)?5:0;
}
