// Visual-only actual-core desktop audition. Optional isolated localhost stream.
#include "udp_transport.hpp"
#include "reaction_view.hpp"
#include <windows.h>
#include <memory>
#include <cstdio>
namespace sf=cluster::spatial;
namespace {
namespace cp=sf::checkpoint;
sf::ReactionView view(91);
std::unique_ptr<Udp> udp;
std::array<uint32_t,320*240> pixels{};
bool enabled=false,held=false,smoke=false,udp_smoke=false,checkpoint_smoke=false,saw_remote=false,saw_loss=false;
bool manual_started=false,manual_finished=false,manual_seen=false,manual_rejoin=false,catchup_witness=false;
uint32_t start=0,manual_at=0;
unsigned accepted=0,rejected=0,recovered=0,paints=0,local_ticks=0,remote_ticks=0,frozen_polls=0,loss_caught=0;
void sync_allowed(){view.allow_remote(enabled&&!held);}
void update(HWND w){
    const auto now=ms();
    const auto& f=view.follower();
    if(checkpoint_smoke&&!manual_started&&f.remote()&&f.admitted&&f.view().sequence()<=10){
        manual_started=true;manual_at=now;SendMessageA(w,WM_LBUTTONDOWN,MK_LBUTTON,0);
    }
    if(manual_started&&!manual_finished&&uint32_t(now-manual_at)>=100){
        manual_finished=true;SendMessageA(w,WM_LBUTTONUP,0,0);
    }
    sync_allowed();
    if(udp)for(unsigned i=0;i<24;++i){
        cp::Packet p{};std::size_t n=0;
        if(!udp->receive_bytes(p.data(),p.size(),n))break;
        if(view.remote_allowed()){
            if(view.receive(p.data(),n,ms())){++accepted;if(saw_loss)++recovered;}
            else ++rejected;
        }
    }
    // Bounded burst of cooperative slices, soft desktop-only time budget.
    const bool remote_before=f.remote();
    const auto caught_before=f.caught_up;
    const auto budget_start=Clock::now();
    for(unsigned i=0;i<32;++i){
        if(!view.step(ms())||Clock::now()-budget_start>=2ms)break;
    }
    if(remote_before&&!f.remote()&&view.remote_allowed()&&f.view().sequence()<80&&!saw_loss){
        saw_loss=true;loss_caught=caught_before;
    }
    if(f.remote()){
        saw_remote=true;++remote_ticks;
        if(manual_finished&&!saw_loss)manual_rejoin=true;
    }else{
        ++local_ticks;
        if(manual_started&&!manual_finished&&!view.remote_allowed())manual_seen=true;
        if(saw_loss&&f.view().sequence()<51&&f.caught_up>loss_caught)catchup_witness=true;
    }
    if(view.frozen())++frozen_polls;
    view.present(ms());
    InvalidateRect(w,nullptr,FALSE);
    if(smoke&&uint32_t(now-start)>=(checkpoint_smoke?6000u:(udp_smoke?4000u:600u)))DestroyWindow(w);
}
LRESULT CALLBACK wnd(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE: return SetTimer(w,1,16,nullptr)?0:-1;
    case WM_TIMER: update(w);return 0;
    case WM_LBUTTONDOWN:held=true;sync_allowed();SetCapture(w);return 0;
    case WM_LBUTTONUP:held=false;sync_allowed();ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:case WM_CANCELMODE:case WM_KILLFOCUS:held=false;sync_allowed();return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE)DestroyWindow(w);
        else if(wp=='R'&&udp&&!(lp&(1L<<30))){enabled=!enabled;sync_allowed();}
        return 0;
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);
        RECT client{};GetClientRect(w,&client);
        std::array<uint16_t,320> row{};
        for(std::size_t y=0;y<240;++y){
            sf::row(view.image(),y,row.data());
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
        const auto& f=view.follower();
        std::snprintf(status,sizeof(status),"%s%s | L%u R%u C%u | Hold: local  R: remote  Esc: quit",
            view.frozen()?"CATCH-UP":(f.remote()?"REMOTE":"LOCAL"),view.blending()&&!view.frozen()?" (blend)":"",
            unsigned(f.local().sequence()),unsigned(f.view().sequence()),unsigned(f.saved_sequence()));
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
    checkpoint_smoke=argc==4&&std::strcmp(argv[1],"--checkpoint-smoke")==0;
    udp_smoke=checkpoint_smoke||(argc==4&&std::strcmp(argv[1],"--udp-smoke")==0);
    smoke=udp_smoke||(argc==2&&std::strcmp(argv[1],"--smoke")==0);
    const bool network=udp_smoke||(argc==4&&std::strcmp(argv[1],"--udp")==0);
    if(argc!=1&&!smoke&&!network)fail("usage: cluster_reaction_view [--smoke | --udp own-port peer-port | --udp-smoke own-port peer-port | --checkpoint-smoke own-port peer-port]");
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
    start=ms();sync_allowed();view.present(start);
    ShowWindow(w,SW_SHOW);UpdateWindow(w);
    if(udp_smoke)std::cout<<"READY"<<std::endl;
    MSG msg{};int rc=0;while((rc=GetMessageA(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    if(network){udp.reset();WSACleanup();}
    if(smoke){
        std::cout<<"GDI paints="<<paints<<" local_ticks="<<local_ticks<<" remote_ticks="<<remote_ticks
            <<" local_seq="<<view.follower().local().sequence()<<" accepted="<<accepted<<" rejected="<<rejected
            <<" recovered="<<recovered<<" loss="<<saw_loss<<" admitted="<<view.follower().admitted
            <<" catchup="<<catchup_witness<<" frozen_polls="<<frozen_polls<<" manual="<<manual_seen
            <<" manual_rejoin="<<manual_rejoin<<'\n';
        if(paints<2||view.follower().local().sequence()<2||!local_ticks)return 1;
        if(checkpoint_smoke&&(!catchup_witness||!frozen_polls||!manual_seen||!manual_rejoin||view.follower().admitted<5))return 1;
        if(udp_smoke&&(!remote_ticks||accepted<20||!rejected||!saw_loss||!recovered))return 1;
    }
    return rc<0?1:0;
}
