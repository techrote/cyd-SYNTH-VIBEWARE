// Grimoire front-panel engineering profile: NO DAC/I2S/amp output enabled.
// This compile-only milestone exercises the display, touch and real digital
// synthesis path. It is not a playable standalone hardware release.
#include "../panel.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
using namespace grimoire;
namespace {
class Display final:public lgfx::LGFX_Device {
    lgfx::Bus_SPI bus_;
    lgfx::Panel_ILI9341_2 panel_;
public:
    Display(){
        auto bus=bus_.config();
        bus.spi_host=SPI2_HOST;bus.freq_write=40000000;bus.freq_read=8000000;
        bus.spi_3wire=false;bus.pin_sclk=14;bus.pin_mosi=13;bus.pin_miso=12;bus.pin_dc=2;
        bus_.config(bus);panel_.setBus(&bus_);
        auto panel=panel_.config();panel.pin_cs=15;panel.pin_rst=-1;panel.invert=true;panel.bus_shared=false;
        panel_.config(panel);setPanel(&panel_);
    }
};
Display lcd;
SPIClass touchBus(VSPI);
XPT2046_Touchscreen digitizer(33);
BoardSession session;
PanelRaster raster;
constexpr int rows=4;
std::array<std::uint16_t,width*rows> pixels{};
std::array<lgfx::rgb565_t,width*rows> transfer{};
std::array<std::int16_t,128> pcm{};
volatile std::uint32_t digitalWitness=0;
bool ready=false,buttonReady=false;
unsigned buttonHistory=7;
int scan=height,frameSpeed=1,frameDirection=1;
bool frameEditing=false,frameArmed=false;
std::uint32_t frameAt=0,touchAt=0,due=0;
unsigned fraction=0;
void poll(std::uint32_t now){
    const bool boot=digitalRead(0)==LOW;
    buttonHistory=((buttonHistory<<1)|unsigned(boot))&7u;
    if(buttonHistory==0)buttonReady=true;
    if(buttonHistory==7&&buttonReady){
        buttonReady=false;
        if(session.armed())session.disarm();else session.arm();
        return;
    }
    if(boot||buttonHistory!=0)return;
    const auto p=digitizer.getPoint();
    const bool contact=p.z>=400;
    const bool valid=!contact||(p.x>=0&&p.x<=4095&&p.y>=0&&p.y<=4095);
    session.poll(valid,contact,raw_axis(p.y,width),raw_axis(4100-int(p.x),height),now);
}
void digitalBlock(std::uint32_t now){
    const auto late=std::uint32_t(now-due);
    if(late>=0x80000000u)return;
    if(late>10667u){session.disarm();due=now;fraction=0;}
    session.audio(millis(),pcm.data(),pcm.size());
    std::uint32_t hash=digitalWitness;
    for(auto sample:pcm)hash=hash*33u+std::uint16_t(sample);
    digitalWitness=hash; // exercise actual PCM without an unreviewed output driver
    due+=5333u;if(++fraction==3){++due;fraction=0;}
}
void present(std::uint32_t now){
    if(scan==height){
        if(std::uint32_t(now-frameAt)<50)return;
        raster.begin(session.frame());frameAt=now;scan=0;
        frameSpeed=session.speed();frameDirection=session.direction();
        frameEditing=session.editing();frameArmed=session.armed();
    }
    if(!raster.render(scan,rows,pixels.data())){session.disarm();ready=false;return;}
    for(unsigned i=0;i<pixels.size();++i)transfer[i]=pixels[i];
    lcd.pushImage(0,scan,width,rows,transfer.data());lcd.waitDMA();scan+=rows;
    if(scan==height){
        lcd.setTextColor(0xffff);
        const char* top[]={"Amber","Iron","Abyss","Orbit","Panic"};
        const char* bottom[]={"Ribbon","Crown","Pendulum","Sketch",frameDirection<0?"Reverse":"Forward",frameSpeed==4?"Speed 4":frameSpeed==2?"Speed 2":"Speed 1"};
        for(int i=0;i<5;++i){lcd.setCursor(i*64+2,8);lcd.print(top[i]);}
        for(int i=0;i<6;++i){lcd.setCursor(i*width/6+2,215);lcd.print(bottom[i]);}
        lcd.setCursor(4,32);lcd.print(!frameArmed?"BOOT to arm digital test":frameEditing?"Tap 8 points - no DAC":"Digital test only - no DAC");
    }
}
}
void setup(){
    pinMode(0,INPUT_PULLUP);pinMode(21,OUTPUT);digitalWrite(21,LOW);
    if(!lcd.init())return;
    lcd.setRotation(5);if(lcd.width()!=width||lcd.height()!=height)return;
    touchBus.begin(25,39,32,33);if(!digitizer.begin(touchBus))return;
    digitizer.setRotation(0);lcd.fillScreen(std::uint16_t(0));digitalWrite(21,HIGH);
    due=micros();ready=true;
}
void loop(){
    if(!ready){delay(100);return;}
    const auto now=millis();
    if(std::uint32_t(now-touchAt)>=12){touchAt=now;poll(now);}
    digitalBlock(micros());present(now);delay(1);
}
