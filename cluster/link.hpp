#pragma once
#include "cluster.hpp"

namespace cluster {
// The transport calls receive only for the configured peer. send() is nonblocking
// and returns whether it accepted a frame; acceptance is not application delivery.
// Single-owner control loop; no calls from audio or radio callback contexts.
struct LinkStats { uint32_t hello=0, sent=0, busy=0, retries=0, remote=0, local=0; };
class Link {
    Coordinator core_;
    Frame request_{};
    uint32_t last_send_=0, last_hello_=0;
    bool hello_sent_=false;
    Completion finish(Completion c) {
        if(c==Completion::none) return c;
        if(c==Completion::remote) ++stats.remote;
        else {
            ++stats.local;
            if(request_.size) { core_.disconnect(); hello_sent_=false; }
        }
        request_={}; return c;
    }
    template<class Send> void transmit(const Frame& f, Send& send) {
        if(send(f)) ++stats.sent; else ++stats.busy;
    }
public:
    explicit Link(uint32_t epoch):core_(epoch) {}
    LinkStats stats{};
    const Grid& grid() const { return core_.grid; }
    bool ready() const { return core_.ready(); }
    bool pending() const { return core_.pending(); }
    template<class Send> bool start(const Job& job,uint32_t now,uint32_t budget,Send& send) {
        if(core_.pending()) return false;
        request_=core_.start(job,now,budget);
        if(!core_.pending()) return false;
        last_send_=now;
        if(request_.size) transmit(request_,send);
        return true;
    }
    template<class Send> Completion tick(uint32_t now,Send& send,bool allow_hello=true) {
        const auto done=finish(core_.poll(now));
        if(core_.pending() && request_.size && uint32_t(now-last_send_)>=10) {
            ++stats.retries; last_send_=now; transmit(request_,send);
        }
        if(allow_hello&&!core_.pending()&&!core_.ready()&&(!hello_sent_||uint32_t(now-last_hello_)>=100)) {
            hello_sent_=true; last_hello_=now; ++stats.hello; transmit(core_.hello(),send);
        }
        return done;
    }
    Completion receive(const Frame& f,uint32_t now) {
        if(!core_.pending()) { if(!core_.ready()) core_.negotiate(f); return Completion::none; }
        return finish(core_.receive(f,now));
    }
    void disconnect() { core_.disconnect(); hello_sent_=false; }
};
} // namespace cluster
