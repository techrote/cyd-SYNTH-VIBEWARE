#pragma once
#include "runtime.hpp"
namespace ecophon {
// Producer-side physical arming gate. A fault is sticky until reboot.
struct TargetGate {
    bool armed=false, releaseRequired=true, buttonReady=false, failed=false, contactActive=false;
    unsigned history=7;
    void stop(Input& in){armed=false;releaseRequired=true;contactActive=false;in.emergency();}
    void poll(Input& in,bool boot,bool down,int x,int y,uint32_t now,bool driverFault){
        history=((history<<1)|unsigned(boot))&7u;
        if(driverFault){if(!failed)stop(in);failed=true;}
        if(failed)return;
        if(history==0)buttonReady=true;
        if(history==7&&buttonReady){
            buttonReady=false;
            if(armed)stop(in);
            else {in.emergency();armed=true;releaseRequired=true;contactActive=false;}
            return;
        }
        if(!armed)return;
        if(boot||history!=0)return;
        if(!down){
            if(!releaseRequired&&!contactActive)return;
            if(in.submit(false,0,0,now)){releaseRequired=false;contactActive=false;}
            else if(contactActive)stop(in);
            return;
        }
        if(x<0||x>=320||y<0||y>=240||(x>=280&&y>=216)){stop(in);return;}
        if(releaseRequired)return;
        if(!in.submit(true,x,y,now))stop(in);else contactActive=true;
    }
};
// Candidate landscape transform, raw ADC range 200..3900. Physical calibration pending.
inline int touchAxis(int raw,int extent){return (std::clamp(raw,200,3900)-200)*(extent-1)/3700;}
inline uint16_t dacWord(int16_t v){return uint16_t((32768+int32_t(v)/2)&0xff00);}
}
