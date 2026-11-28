#include "touch.hpp"
#include <cassert>
#include <iostream>
using namespace grimoire;
static void tap(Touch& t,int x,int y){t.down(x,y);t.up();}
static void silent(Engine& e){for(int i=0;i<2400;++i)assert(e.sample()==0);}
static int audible(Engine& e){int n=0;for(int i=0;i<2400;++i)n+=e.sample()!=0;return n;}
int main(){
    Engine e;Touch t(e);silent(e);
    assert(source_y(32)==32&&source_y(207)==233&&source_y(208)==-1&&source_y(233)==-1&&source_y(234)==234);
    assert(touch_point(0,32).x==0&&touch_point(319,207).x==127&&touch_point(319,207).y==127);
    for(int y=32;y<208;++y){assert(source_y(y)>=32&&source_y(y)<=233);if(y>32)assert(source_y(y)>source_y(y-1));}
    tap(t,80,220);assert(t.shape()==1&&e.snapshot().path[0].x==crown[0].x);
    tap(t,135,220);assert(t.shape()==2&&e.snapshot().path[0].x==pendulum[0].x);
    tap(t,20,220);assert(t.shape()==0);
    tap(t,240,220);assert(t.direction()==-1);tap(t,295,220);assert(t.speed()==2);tap(t,295,220);assert(t.speed()==4);tap(t,295,220);assert(t.speed()==1);
    assert(!t.down(200,12));t.up();assert(e.snapshot().auto_on&&audible(e)>2000);
    t.down(30,80);assert(e.snapshot().manual);t.move(260,180);assert(e.snapshot().xy.x>100);t.up();assert(!e.snapshot().manual&&e.snapshot().auto_on&&audible(e)>2000);
    assert(t.down(185,220));t.up();assert(t.editing());silent(e);
    // A held contact cannot add multiple points or start the orbit while editing.
    t.down(20,50);for(int i=0;i<100;++i){t.move(150,100);t.down(60,90);}assert(t.points()==1);t.up();tap(t,200,10);assert(t.editing()&&!e.snapshot().auto_on);silent(e);
    for(int i=1;i<8;++i)tap(t,20+i*40,50+i*20);
    assert(!t.editing()&&t.shape()==3&&t.points()==0);silent(e);
    auto s=e.snapshot();assert(s.path[7].x>s.path[0].x&&s.path[7].y>s.path[0].y);
    tap(t,200,10);assert(audible(e)>2000);
    t.down(160,120);assert(t.move(300,12));assert(t.contact());silent(e);t.move(160,120);silent(e);t.up();assert(!t.contact());
    t.down(160,120);assert(audible(e)>2000);assert(t.cancel());assert(!t.contact());silent(e);t.move(30,50);silent(e);
    t.down(160,120);assert(audible(e)>2000);for(int i=0;i<7000;++i)e.sample();assert(!e.snapshot().manual&&e.snapshot().envelope==0);t.up();
    assert(t.down(90,10));t.up();assert(e.snapshot().material==1);silent(e);
    t.down(-1,120);assert(!t.contact());t.down(320,120);assert(!t.contact());tap(t,100,237);silent(e);
    for(int y=208;y<234;++y)for(int x=0;x<320;++x)assert(t.toolbar_pixel(x,y)!=0);
    std::cout<<"PASS touch-only materials, all shapes, sketch, speed, reverse, manual override, drag panic, cancel, lease and layout\n";
}
