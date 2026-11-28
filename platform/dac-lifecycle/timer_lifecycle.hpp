#pragma once
namespace cyd_platform {
// Single-use, serialized owner. Driver and sink must outlive any ISR, including
// a failed removal. Never call from an ISR or from a different core.
template<class Driver, class Sink> class TimerLifecycle {
    Driver& driver_;
    Sink& sink_;
    bool used_=false, clock_=false, timer_=false, handler_=false, dac_=false;
    bool live_=false, clean_=true;
    bool cleanup(){
        sink_.invalidate();
        bool ok=true;
        if(timer_) ok=driver_.pause()&&ok;
        if(dac_){driver_.midpoint();ok=driver_.disable()&&ok;}
        if(handler_) ok=driver_.remove()&&ok;
        bool freed=true;
        if(timer_){freed=driver_.deinit();ok=freed&&ok;}
        // Retain the clock lock if timer deinitialization failed.
        if(clock_&&freed)ok=driver_.release_clock()&&ok;
        live_=false;clean_=ok;return ok;
    }
public:
    TimerLifecycle(Driver& driver,Sink& sink):driver_(driver),sink_(sink){}
    TimerLifecycle(const TimerLifecycle&)=delete;
    TimerLifecycle& operator=(const TimerLifecycle&)=delete;
    bool start(){
        if(used_||!driver_.owner_core())return false;
        used_=true;sink_.invalidate();
        if(!(clock_=driver_.acquire_clock()))return false;
        if(!(timer_=driver_.init_timer())||!driver_.configure_timer()||
           !(handler_=driver_.install())){cleanup();return false;}
        dac_=true; // prepare/enable may partially succeed; always disable on failure.
        if(!driver_.prepare_dac()||!driver_.enable()||!driver_.start_timer()){
            cleanup();return false;
        }
        live_=true;return true;
    }
    bool stop(){
        if(!driver_.owner_core())return false;
        if(!live_)return clean_;
        return cleanup();
    }
    bool live()const{return live_;}
    bool cleanup_ok()const{return clean_;}
};
}
