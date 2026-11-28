#pragma once
#include "core.hpp"
namespace cyd::platform {
// Exact CHRONOGRID M4 row dependencies; not a generic renderer contract.
class DirtyGrid {
    gridseq::Control previous_{}, current_{};
    uint32_t previousHead_=0, head_=0;
    unsigned cursor_=240, dirty_=0;
    bool valid_=false;
    static unsigned band(unsigned y) { return y<24?0:y<200?1+(y-24)/44:5; }
    static unsigned end(unsigned b) { return b==0?24:b==5?240:24+b*44; }
    static unsigned headKey(uint32_t h,unsigned t) { return h&0x10000u?1u+((h>>(4*t))&15u):0; }
    void skip() {
        while(cursor_<240 && !(dirty_&(1u<<band(cursor_)))) cursor_=end(band(cursor_));
        if(cursor_==240){previous_=current_;previousHead_=head_;valid_=true;}
    }
public:
    bool busy()const{return cursor_<240;}
    // Call on display reset/loss or before any external drawing. Aborts partial frame.
    void invalidate(){valid_=false;cursor_=240;}
    bool begin(const gridseq::Control& c,uint32_t h){
        if(busy())return false;
        current_=c;head_=h;dirty_=valid_?0u:63u;
        if(c.scene!=previous_.scene)dirty_|=1;
        if(c.play!=previous_.play||c.bpm!=previous_.bpm)dirty_|=32;
        for(unsigned t=0;t<4;++t){
            const unsigned bit=1u<<t;
            if(c.masks[t]!=previous_.masks[t]||c.accents[t]!=previous_.accents[t]||
               ((c.mute^previous_.mute)&bit)||((c.accentEdit^previous_.accentEdit)&bit)||
               c.rules[t]!=previous_.rules[t]||c.divs[t]!=previous_.divs[t]||
               headKey(h,t)!=headKey(previousHead_,t))dirty_|=2u<<t;
        }
        cursor_=0;skip();return true;
    }
    // At most four rows. Sink returns true only after synchronous consumption.
    // A rejected submission retains the cursor; invalidate() forces full recovery.
    template<class Sink> unsigned step(Sink&& sink){
        if(!busy())return 0;
        const unsigned n=std::min(4u,end(band(cursor_))-cursor_);
        if(!sink(current_,head_,cursor_,n))return 0;
        cursor_+=n;skip();return n;
    }
};
}
