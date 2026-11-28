#pragma once
#include "checkpoint_follow.hpp"
#include "spatial_handoff.hpp"
namespace cluster::spatial {
// Single control-loop owner; painting uses only the complete presented image.
class ReactionView {
    checkpoint::Follower follower_;
    Handoff handoff_;
    bool allowed_=false;
public:
    explicit ReactionView(uint32_t epoch):follower_(epoch){}
    const auto& follower()const{return follower_;}
    const auto& image()const{return handoff_.pixels();}
    bool remote_allowed()const{return allowed_;}
    bool blending()const{return handoff_.fading();}
    bool frozen()const{return allowed_&&!follower_.remote()&&follower_.catchup_target()!=0;}
    void allow_remote(bool allow){
        if(allowed_&&!allow)follower_.suspend_remote();
        allowed_=allow;
    }
    bool receive(const uint8_t* p,std::size_t n,uint32_t now){
        return allowed_&&follower_.receive(p,n,now);
    }
    bool step(uint32_t now){
        const auto before=follower_.slices;
        follower_.tick(now);
        return follower_.slices!=before;
    }
    void present(uint32_t now){
        // Freeze on autonomous catch-up. Explicit local override is immediate.
        if(frozen())return;
        const bool remote=allowed_&&follower_.remote();
        handoff_.select(remote?follower_.view().pixels():follower_.local().pixels(),
                        remote,now,!allowed_);
    }
};
}
