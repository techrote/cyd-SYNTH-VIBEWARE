#pragma once
#include <cstdint>
namespace chronocyd {
// Producer-side single-contact gate. BOOT must first be released for three polls.
struct TargetGate {
 bool armed=false,awaitRelease=true,buttonReady=false;
 unsigned history=7;
 template<class Input> void poll(Input& in,bool button,bool down,int x,int y,uint32_t now,bool fault){
  history=((history<<1)|unsigned(button))&7u;
  if(fault){if(armed)in.emergency();armed=false;awaitRelease=true;return;}
  if(history==0)buttonReady=true;
  if(history==7&&buttonReady){buttonReady=false;armed=!armed;awaitRelease=true;in.emergency();return;}
  if(!armed)return;
  if(!down){if(in.send(false,0,0,now))awaitRelease=false;return;}
  if(awaitRelease)return;
  if(x<0||x>=320||y<0||y>=240||(x>=280&&y>=200)){in.emergency();awaitRelease=true;return;}
  if(!in.send(true,x,y,now))awaitRelease=true;
 }
};
// Classic ESP32 internal DAC consumes the upper byte of each 16-bit slot.
// Half-scale signed PCM, DC midpoint for silence; both slots identical.
inline uint16_t dacWord(int16_t sample){return uint16_t((32768+int32_t(sample)/2)&0xff00);}
}
