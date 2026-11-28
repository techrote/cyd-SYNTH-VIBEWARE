#include "ble_control.hpp"
#include "field_av.hpp"
#include <cstdio>
#include <cstdlib>
using namespace cluster::ble;
unsigned checks=0;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"BLE check %u failed\n",checks);std::exit(1);}}
int main(){
    const Control c{0x12345678u,1,65535,0};auto b=encode(c);Control d{};
    check(decode(b.data(),b.size(),d));check(d.epoch==c.epoch&&d.seq==1&&d.x==65535&&d.y==0);
    for(std::size_t n=0;n<16;++n)check(!decode(b.data(),n,d));
    check(!decode(b.data(),17,d));check(!decode(nullptr,16,d));
    for(unsigned i=0;i<4;++i){auto bad=b;bad[i]^=128;check(!decode(bad.data(),bad.size(),d));}
    auto bad=encode({0,1,0,0});check(!decode(bad.data(),bad.size(),d));
    bad=encode({1,0,0,0});check(!decode(bad.data(),bad.size(),d));
    auto a=ack(c.epoch,0);uint32_t e=0,s=1;check(read_ack(a.data(),a.size(),e,s)&&e==c.epoch&&s==0);
    for(std::size_t n=0;n<a.size();++n)check(!read_ack(a.data(),n,e,s));
    Modulator m(c.epoch);const XY local{1,65535};
    check(!m.receive({c,10,1},10));m.mode(true,1);
    check(!m.receive({c,10,2},10));check(!m.receive({c,10,1},261));
    check(m.receive({c,10,1},260));check(m.fresh(260));check(!m.fresh(261));
    auto xy=m.effective(local,260);check(xy.x==32768&&xy.y==32767);
    xy=m.effective(local,261);check(xy.x==local.x&&xy.y==local.y);
    check(!m.receive({c,261,1},261));check(m.sequence()==1);
    m.mode(false,2);check(!m.fresh(20));m.mode(true,3);
    check(!m.receive({{c.epoch,2,0,0},20,1},20));check(!m.receive({{1,2,0,0},20,3},20));
    check(m.receive({{c.epoch,2,0,0},0xfffffff0u,3},10));check(m.fresh(10));check(!m.fresh(235));
    for(uint32_t i=3;i<1003;++i){
        check(m.receive({{c.epoch,i,uint16_t(i),uint16_t(i*3)},i,3},i));
        check(!m.receive({{c.epoch,i,0,0},i,3},i));
        xy=m.effective(local,i);check(xy.x==uint16_t((1u+i)/2)&&xy.y==uint16_t((65535u+i*3)/2));
        m.mode(true,3);check(m.fresh(i));
    }
    check(m.receive({{c.epoch,0xffffffffu,0,0},2000,3},2000));
    check(!m.receive({{c.epoch,1,0,0},2001,3},2001));
    m.expire(2251);check(!m.fresh(2000)); // old stamp cannot resurrect after full timer wrap
    Modulator live(55);live.mode(true,1);
    check(live.receive({{55,1,65535,0},5,1},5));
    cluster::av::Field field(1),reference(1);const XY origin{10000,40000};
    auto none=[](const cluster::Frame&){return false;};
    const auto mixed=live.effective(origin,5);
    field.xy(mixed.x,mixed.y);reference.xy(mixed.x,mixed.y);
    field.gate(false);reference.gate(false);field.tick(5,none);reference.tick(5,none);
    check(field.grid()==reference.grid());check(field.shown().x==mixed.x&&field.shown().y==mixed.y);
    check(field.sound()==0); // remote control cannot open gate
    field.gate(true);check(field.sound()!=0);
    live.expire(256);const auto fallback=live.effective(origin,256);
    field.xy(fallback.x,fallback.y);reference.xy(origin.x,origin.y);
    field.gate(false);reference.gate(false);field.tick(256,none);reference.tick(256,none);
    check(field.grid()==reference.grid());check(field.shown().x==origin.x&&field.shown().y==origin.y);
    check(field.sound()==0);
    std::printf("BLE control: %u checks PASS\n",checks);
}
