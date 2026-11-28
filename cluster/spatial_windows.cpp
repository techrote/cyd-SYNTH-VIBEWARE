// Visual-only actual-core desktop audition. Optional isolated localhost stream.
#include "udp_transport.hpp"
#include "spatial.hpp"
#include "spatial_handoff.hpp"
#include <windows.h>
#include <memory>
#include <cstdio>
namespace sf=cluster::spatial;
namespace {
sf::ReactionField local;
sf::Receiver rx(91,1);
sf::Handoff handoff;
std::unique_ptr<Udp> udp;
std::array<uint32_t,320*240> pixels{};
bool enabled=false,held=false,smoke=false,udp_smoke=false,saw_remote=false,saw_loss=false;
uint32_t start=0,last_step=0;
unsigned accepted=0,rejected=0,recovered=0,paints=0,local_ticks=0,remote_ticks=0;
void update(HWND w){
    if(udp)for(unsigned i=0;i<8;++i){
        sf::Packet p{};std::size_t n=0;
        if(!udp->receive_bytes(p.data(),p.size(),n))break;
        if(enabled){
            if(rx.accept(p.data(),n,ms())){++accepted;if(saw_loss)++recovered;}
            else ++rejected;
        }
    }
    const auto now=ms();
    const bool fresh=rx.fresh(now);
    const bool remote=enabled&&!held&&fresh;
    if(remote){saw_remote=true;++remote_ticks;}
    else {
        ++local_ticks;
        if(enabled&&!held&&saw_remote&&!fresh)saw_loss=true;
        if(uint32_t(now-last_step)>=50){last_step=now;local.advance();}
    }
    if(remote)last_step=now; // no catch-up on return from remote
    handoff.select(remote?rx.pixels():local.pixels(),remote,now,held||!enabled);
    InvalidateRect(w,nullptr,FALSE);
    if(smoke&&uint32_t(now-start)>=(udp_smoke?4000u:600u))DestroyWindow(w);
}
LRESULT CALLBACK wnd(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE: return SetTimer(w,1,16,nullptr)?0:-1;
    case WM_TIMER: update(w);return 0;
    case WM_LBUTTONDOWN:held=true;SetCapture(w);return 0;
    case WM_LBUTTONUP:held=false;ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:case WM_CANCELMODE:case WM_KILLFOCUS:held=false;return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE)DestroyWindow(w);
        else if(wp=='R'&&udp&&!(lp&(1L<<30))){enabled=!enabled;if(!enabled)rx.disconnect();}
        return 0;
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);
        RECT client{};GetClientRect(w,&client);
        std::array<uint16_t,320> row{};
        for(std::size_t y=0;y<240;++y){
            sf::row(handoff.pixels(),y,row.data());
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
        std::snprintf(status,sizeof(status),"%s%s | local %u remote %u | Hold: local  R: remote  Esc: quit",
            handoff.remote()?"REMOTE":"LOCAL",handoff.fading()?" (visual blend)":"",
            unsigned(local.sequence()),unsigned(rx.sequence()));
        DrawTextA(dc,status,-1,&bar,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
        if(result!=GDI_ERROR&&result!=0)++paints;
        EndPaint(w,&ps);return 0;
    }
    case WM_DESTROY:KillTimer(w,1);PostQuitMessage(0);return 0;
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
    udp_smoke=argc==4&&std::strcmp(argv[1],"--udp-smoke")==0;
    smoke=udp_smoke||(argc==2&&std::strcmp(argv[1],"--smoke")==0);
    const bool network=udp_smoke||(argc==4&&std::strcmp(argv[1],"--udp")==0);
    if(argc!=1&&!smoke&&!network)fail("usage: cluster_reaction_view [--smoke | --udp own-port peer-port | --udp-smoke own-port peer-port]");
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
    HWND w=CreateWindowExA(0,wc.lpszClassName,"VIBEWARE / REACTION - distributed visual synthesis",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,800,640,nullptr,nullptr,instance,nullptr);
    if(!w)fail("CreateWindow");
    start=last_step=ms();handoff.select(local.pixels(),false,start);
    ShowWindow(w,SW_SHOW);UpdateWindow(w);
    if(udp_smoke)std::cout<<"READY"<<std::endl;
    MSG msg{};int rc=0;while((rc=GetMessageA(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    if(network){udp.reset();WSACleanup();}
    if(smoke){
        std::cout<<"GDI paints="<<paints<<" local_ticks="<<local_ticks<<" remote_ticks="<<remote_ticks
            <<" local_seq="<<local.sequence()<<" accepted="<<accepted<<" rejected="<<rejected
            <<" recovered="<<recovered<<" loss="<<saw_loss<<'\n';
        if(paints<2||local.sequence()<2||!local_ticks)return 1;
        if(udp_smoke&&(!remote_ticks||accepted<20||!rejected||!saw_loss||!recovered))return 1;
    }
    return rc<0?1:0;
}
