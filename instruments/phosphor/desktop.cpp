#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include "desktop_session.hpp"
using namespace phosphor;
namespace {
DesktopSession session;
std::array<uint16_t,320*240> pixels{};
uint64_t micros(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void SDLCALL audio(void*,Uint8* stream,int len){
 auto* p=reinterpret_cast<int16_t*>(stream);
 for(int i=0;i<len/2;++i)p[i]=session.sample();
 if(len%2)stream[len-1]=0;
}
bool render(SDL_Renderer* r,SDL_Texture* texture){
 for(int y=0;y<240;++y)session.visual().row(y,pixels.data()+y*320);
 if(SDL_UpdateTexture(texture,nullptr,pixels.data(),320*2)<0||SDL_RenderClear(r)<0||SDL_RenderCopy(r,texture,nullptr,nullptr)<0)return false;
 return true;
}
void pointer(SDL_Window* w,int x,int y,bool down){
 int width=0,height=0;SDL_GetWindowSize(w,&width,&height);
 if(width<=0||height<=0||x<0||y<0||x>=width||y>=height){session.panic();return;}
 const int px=int(int64_t(x)*320/width),py=int(int64_t(y)*240/height);
 if(down)session.down(px,py);else session.move(px,py);
}
void event(SDL_Window* w,const SDL_Event& e,bool& running){
 switch(e.type){
 case SDL_QUIT:running=false;break;
 case SDL_MOUSEBUTTONDOWN:if(e.button.button==SDL_BUTTON_LEFT){pointer(w,e.button.x,e.button.y,true);if(session.dragging())SDL_CaptureMouse(SDL_TRUE);}break;
 case SDL_MOUSEMOTION:if(session.dragging())pointer(w,e.motion.x,e.motion.y,false);break;
 case SDL_MOUSEBUTTONUP:if(e.button.button==SDL_BUTTON_LEFT){session.up();SDL_CaptureMouse(SDL_FALSE);}break;
 case SDL_WINDOWEVENT:if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST||e.window.event==SDL_WINDOWEVENT_HIDDEN){session.panic();SDL_CaptureMouse(SDL_FALSE);}break;
 case SDL_KEYDOWN:if(e.key.keysym.sym==SDLK_ESCAPE||e.key.keysym.sym==SDLK_SPACE){session.panic();SDL_CaptureMouse(SDL_FALSE);}break;
 default:break;
 }
}
bool smoke(SDL_Window* w,SDL_Renderer* r,SDL_Texture* texture){
 const char* driver=SDL_GetCurrentVideoDriver();
 if(!driver||std::strcmp(driver,"dummy")==0||std::strcmp(driver,"offscreen")==0)return false;
 std::cout<<"SDL video driver="<<driver<<'\n';
 bool running=true;SDL_Event e{};int width=0,height=0;SDL_GetWindowSize(w,&width,&height);
 if(!render(r,texture))return false;
 auto before=pixels;SDL_RenderPresent(r);session.advance(0);
 e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=width/12;e.button.y=height/24;event(w,e,running);
 e.button.x=width/2;e.button.y=height/2;event(w,e,running);
 std::array<int16_t,800> pcm{};unsigned audible=0;
 for(int i=1;i<=60;++i){session.advance(uint64_t(i)*33334);audio(nullptr,reinterpret_cast<Uint8*>(pcm.data()),int(sizeof(pcm)));for(auto v:pcm)audible+=v!=0;}
 if(!render(r,texture))return false;
 bool changed=pixels!=before;
 int rw=0,rh=0;if(SDL_GetRendererOutputSize(r,&rw,&rh)<0||rw<1||rh<1||rw>2048||rh>2048)return false;
 // Read one pixel in the selected white scene header from the actual renderer.
 SDL_Rect probe{rw/12,rh/24,1,1};uint32_t rgba=0;
 bool read=SDL_RenderReadPixels(r,&probe,SDL_PIXELFORMAT_ARGB8888,&rgba,4)==0&&(rgba&0xffffff)==0xffffff;
 SDL_RenderPresent(r);e.type=SDL_MOUSEBUTTONUP;event(w,e,running);bool released=!session.visual().held();
 e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;event(w,e,running);audio(nullptr,reinterpret_cast<Uint8*>(pcm.data()),int(sizeof(pcm)));bool quiet=std::all_of(pcm.begin(),pcm.end(),[](int16_t v){return v==0;});
 std::cout<<"SDL graphical smoke audible="<<audible<<" changed="<<changed<<" readback="<<read<<" released="<<released<<" quiet="<<quiet<<'\n';
 return audible>40000&&changed&&read&&released&&quiet;
}
}
int main(int argc,char** argv){
 const bool test=argc==2&&std::strcmp(argv[1],"--smoke")==0;
 SDL_SetMainReady();if(SDL_Init(SDL_INIT_VIDEO|(test?0:SDL_INIT_AUDIO))<0){std::cerr<<SDL_GetError()<<'\n';return 1;}
 SDL_Window* w=SDL_CreateWindow("PHOSPHOR | Ember Glacial Orchid Verdure Brass Afterimage | bottom: Freeze Clear Panic",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_RESIZABLE);
 SDL_Renderer* r=w?SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE):nullptr;
 SDL_Texture* texture=r?SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,320,240):nullptr;
 int status=0;SDL_AudioDeviceID device=0;
 if(!texture){std::cerr<<SDL_GetError()<<'\n';status=2;}
 else if(test)status=smoke(w,r,texture)?0:3;
 else{
  SDL_AudioSpec want{},got{};want.freq=rate;want.format=AUDIO_S16SYS;want.channels=1;want.samples=256;want.callback=audio;
  device=SDL_OpenAudioDevice(nullptr,0,&want,&got,0);
  if(!device||got.freq!=rate||got.format!=AUDIO_S16SYS||got.channels!=1){std::cerr<<"24 kHz mono audio unavailable: "<<SDL_GetError()<<'\n';status=4;}
  else{
   session.advance(micros());SDL_PauseAudioDevice(device,0);bool running=true;uint64_t last=0;
   while(running){
    auto now=micros();session.advance(now);SDL_Event e{};for(int i=0;i<128&&SDL_PollEvent(&e);++i)event(w,e,running);
    if(session.dragging()&&(!(SDL_GetMouseState(nullptr,nullptr)&SDL_BUTTON_LMASK)||!(SDL_GetWindowFlags(w)&SDL_WINDOW_INPUT_FOCUS))){session.panic();SDL_CaptureMouse(SDL_FALSE);}
    if(now-last>=33333){last=now;if(!render(r,texture)){status=5;break;}SDL_RenderPresent(r);}
    SDL_Delay(1);
   }
  }
 }
 session.panic();if(device){SDL_PauseAudioDevice(device,1);SDL_CloseAudioDevice(device);}if(texture)SDL_DestroyTexture(texture);if(r)SDL_DestroyRenderer(r);if(w)SDL_DestroyWindow(w);SDL_Quit();return status;
}
