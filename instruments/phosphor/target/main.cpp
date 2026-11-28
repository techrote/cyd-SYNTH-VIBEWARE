// Source-only CYD front panel. No DAC/I2S/amp configuration or output.
#include "../board_session.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <esp_timer.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
namespace {
class Display final:public lgfx::LGFX_Device {
 lgfx::Bus_SPI bus_;
 lgfx::Panel_ILI9341_2 panel_;
public:
 Display(){
  auto b=bus_.config();b.spi_host=SPI2_HOST;b.freq_write=40000000;b.freq_read=8000000;
  b.spi_3wire=false;b.pin_sclk=14;b.pin_mosi=13;b.pin_miso=12;b.pin_dc=2;
  bus_.config(b);panel_.setBus(&bus_);
  auto p=panel_.config();p.pin_cs=15;p.pin_rst=-1;p.invert=true;p.bus_shared=false;
  panel_.config(p);setPanel(&panel_);
 }
};
Display lcd;
SPIClass touchBus(VSPI);
XPT2046_Touchscreen digitizer(33);
phosphor::BoardSession session;
constexpr int rows=4;
std::array<uint16_t,320*rows> pixels{};
std::array<lgfx::rgb565_t,320*rows> transfer{};
std::array<int16_t,128> pcm{};
volatile uint32_t digitalWitness=0;
uint64_t pollAt=0,frameAt=0;
int scan=240;
bool ready=false;
int axis(int raw,int span){return std::clamp((raw-200)*span/3700,0,span-1);}
void poll(uint64_t now){
 if(now-pollAt<12000)return;
 pollAt=now;const auto p=digitizer.getPoint();const bool contact=p.z>=400;
 const bool valid=!contact||(p.x>=0&&p.x<=4095&&p.y>=0&&p.y<=4095);
 session.poll(valid,contact,axis(p.y,320),axis(4100-int(p.x),240),now);
}
void present(uint64_t now){
 if(scan==240){if(now-frameAt<66667)return;session.capture();scan=0;frameAt=now;}
 if(!session.strip(scan,rows,pixels.data())){ready=false;return;}
 for(unsigned i=0;i<pixels.size();++i)transfer[i]=pixels[i];
 lcd.pushImage(0,scan,320,rows,transfer.data());lcd.waitDMA();scan+=rows;
 if(scan==240){
  lcd.setTextColor(0xffff,0x2945);
  const char* names[]={"Ember","Glacial","Orchid","Verdure","Brass","After"};
  for(int i=0;i<6;++i){lcd.setCursor((i*320+5)/6+2,5);lcd.print(names[i]);}
  lcd.setCursor(4,225);lcd.print("Freeze");lcd.setCursor(124,225);lcd.print("Clear");lcd.setCursor(244,225);lcd.print("Panic");
 }
}
}
void setup(){
 pinMode(21,OUTPUT);digitalWrite(21,LOW);
 if(!lcd.init())return;
 lcd.setRotation(5);if(lcd.width()!=320||lcd.height()!=240)return;
 touchBus.begin(25,39,32,33);if(!digitizer.begin(touchBus))return;
 digitizer.setRotation(0);lcd.fillScreen(uint16_t(0));digitalWrite(21,HIGH);ready=true;
}
void loop(){
 if(!ready){delay(100);return;}
 const uint64_t now=esp_timer_get_time();poll(now);session.advance(now);
 if(session.digital(esp_timer_get_time(),pcm.data())){
  uint32_t h=digitalWitness;for(auto v:pcm)h=h*33u+uint16_t(v);digitalWitness=h;
 }
 present(esp_timer_get_time());delay(1);
}
