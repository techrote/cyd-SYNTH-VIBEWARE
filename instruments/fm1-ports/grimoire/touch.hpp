#pragma once
#include "core.hpp"

namespace grimoire {
// Logical 320x240 single-contact surface. Top: existing materials/orbit/panic;
// bottom: six direct performance controls plus a six-pixel envelope meter.
constexpr int pad_end=208, toolbar_end=234;
constexpr int source_y(int y) {
    return y<32||y>=toolbar_end ? y : y<pad_end ? 32+(y-32)*201/175 : -1;
}
constexpr Point touch_point(int x,int y) {
    return clamp({(std::clamp(x,8,311)-8)*127/303,
                  (source_y(std::clamp(y,32,pad_end-1))-38)*127/193});
}
enum class Control { Ribbon, Crown, Pendulum, Sketch, Reverse, Speed };
class Touch {
    Engine& e_;
    enum class Contact { None, Pad, Button };
    Contact contact_=Contact::None;
    Path sketch_{};
    int count_=0, direction_=1, speed_=1, shape_=0;
    bool editing_=false;
public:
    explicit Touch(Engine& e):e_(e) {}
    bool contact() const { return contact_!=Contact::None; }
    bool editing() const { return editing_; }
    int points() const { return count_; }
    int direction() const { return direction_; }
    int speed() const { return speed_; }
    int shape() const { return shape_; }
    // true means caller must discard already queued PCM as well as muting core.
    bool cancel() { e_.panic(); contact_=Contact::None; editing_=false; count_=0; return true; }
    bool control(Control c) {
        switch(c) {
        case Control::Ribbon: case Control::Crown: case Control::Pendulum:
            editing_=false;count_=0;shape_=int(c);
            e_.set_path(c==Control::Ribbon?ribbon:c==Control::Crown?crown:pendulum);return false;
        case Control::Sketch:
            if(editing_) { editing_=false; count_=0; return false; }
            e_.panic(); editing_=true; count_=0; return true;
        case Control::Reverse: direction_=-direction_;e_.direction(direction_);return false;
        case Control::Speed: speed_=speed_==4?1:speed_*2;e_.speed(speed_);return false;
        }
        return false;
    }
    bool down(int x,int y) {
        if(contact()||x<0||x>=width||y<0||y>=height) return false;
        contact_=Contact::Button;
        if(y<32) {
            int group=x/64;
            if(group<3) { e_.material(group);editing_=false;count_=0;return true; }
            if(group==4||e_.snapshot().auto_on) { e_.panic();editing_=false;count_=0;return true; }
            if(!editing_) e_.start_orbit();
            return false;
        }
        if(y>=pad_end) {
            return y<toolbar_end ? control(static_cast<Control>(x*6/width)) : false;
        }
        const auto p=touch_point(x,y);
        if(editing_) {
            sketch_[count_++]=p;
            if(count_==8) { e_.set_path(sketch_);shape_=3;editing_=false;count_=0; }
            return false;
        }
        contact_=Contact::Pad;e_.down(p);return false;
    }
    bool move(int x,int y) {
        if(contact_!=Contact::Pad) return false;
        if(x>=256&&x<width&&y>=0&&y<32) {
            e_.panic();contact_=Contact::Button;return true;
        }
        e_.move(touch_point(x,y));return false;
    }
    void up() { if(contact_==Contact::Pad)e_.up();contact_=Contact::None; }
    std::uint16_t toolbar_pixel(int x,int y) const {
        if(x<0||x>=width||y<pad_end||y>=toolbar_end)return 0;
        int group=x*6/width;
        if(x==0||x*6/width!=(x-1)*6/width||y==pad_end)return 0x0842;
        bool active=group==3?editing_||shape_==3:group==4?direction_<0:group==5?speed_>1:!editing_&&shape_==group;
        return active?0x5d8b:0x2104;
    }
};
}
