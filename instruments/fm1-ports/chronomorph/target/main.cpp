// Original Chronomorph adapter. Board configuration facts reviewed from Diviner
// 7b1b37eec6ac126f334a0b008d91135071ea60fa and local qualified CYD candidates.
#include "../runtime.hpp"
#include "../target_gate.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
#include <driver/i2s.h>
using namespace chronocyd;
class Display:public lgfx::LGFX_Device {
 lgfx::Bus_SPI bus;lgfx::Panel_ILI9341_2 panel;
 public:Display(){
  auto b=bus.config();b.spi_host=SPI2_HOST;b.freq_write=40000000;b.freq_read=8000000;
  b.spi_3wire=false;b.pin_sclk=14;b.pin_mosi=13;b.pin_miso=12;b.pin_dc=2;
  bus.config(b);panel.setBus(&bus);
  auto c=panel.config();c.pin_cs=15;c.pin_rst=-1;c.invert=true;c.bus_shared=false;
  panel.config(c);setPanel(&panel);
 }
};
static Display lcd;static SPIClass touchBus(VSPI);static XPT2046_Touchscreen touch(33);
static Input input;static Ring<Engine,4> views;static TargetGate gate;
static std::atomic<bool> fault{false};static bool ready=false,started=false;
static void audioTask(void*){
 Runtime runtime;int16_t pcm[128];uint16_t slots[256];
 for(;;){
  runtime.render(input,millis(),pcm,128);views.push(runtime.engine);
  for(unsigned i=0;i<128;++i)slots[2*i]=slots[2*i+1]=dacWord(pcm[i]);
  size_t written=0;
  if(i2s_write(I2S_NUM_0,slots,sizeof(slots),&written,pdMS_TO_TICKS(20))!=ESP_OK||written!=sizeof(slots)){
   fault.store(true);i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);vTaskDelete(nullptr);return;
  }
 }
}
static bool startAudio(){
 if(started)return true;
 i2s_config_t c{};c.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN);
 c.sample_rate=rate;c.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;c.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
 c.communication_format=I2S_COMM_FORMAT_STAND_MSB;c.dma_buf_count=4;c.dma_buf_len=128;c.tx_desc_auto_clear=true;
 if(i2s_driver_install(I2S_NUM_0,&c,0,nullptr)!=ESP_OK)return false;
 if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)!=ESP_OK||
    xTaskCreatePinnedToCore(audioTask,"chrono-audio",6144,nullptr,5,nullptr,0)!=pdPASS){
  i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);i2s_driver_uninstall(I2S_NUM_0);return false;
 }
 started=true;return true;
}
static int axis(int n,int extent){return (std::clamp(n,200,3900)-200)*(extent-1)/3700;}
void setup(){
 pinMode(0,INPUT_PULLUP);pinMode(21,OUTPUT);digitalWrite(21,LOW);
 ready=lcd.init();if(!ready)return;
 lcd.setRotation(5);ready=lcd.width()==320&&lcd.height()==240;if(!ready)return;
 touchBus.begin(25,39,32,33);touch.begin(touchBus);touch.setRotation(0);
 lcd.fillScreen(uint16_t(0));digitalWrite(21,HIGH);
}
void loop(){
 if(!ready){delay(100);return;}
 static uint32_t inputAt=0,frameAt=0;static unsigned scan=240;static Engine view;
 static uint16_t raw[320];static lgfx::rgb565_t line[320];
 const uint32_t now=millis();
 if(now-inputAt>=12){
  inputAt=now;auto p=touch.getPoint();const bool down=p.z>=400;
  gate.poll(input,digitalRead(0)==LOW,down,axis(p.y,320),axis(4100-int(p.x),240),now,fault.load());
  if(gate.armed&&!started&&!fault.load()&&!startAudio())fault.store(true);
 }
 if(scan==240&&now-frameAt>=50){
  Engine next;for(unsigned n=0;n<3&&views.pop(next);++n)view=next;
  if(!gate.armed||fault.load())view.stop();
  scan=0;frameAt=now;
 }
 if(scan<240){
  lcd.startWrite();
  for(unsigned n=0;n<4&&scan<240;++n,++scan){
   row(view,int(scan),raw);for(unsigned x=0;x<320;++x)line[x]=raw[x];
   lcd.pushImage(0,int(scan),320,1,line);
  }
  lcd.endWrite();
  if(scan==240){
   lcd.setTextColor(0xffff);lcd.setCursor(2,2);
   lcd.print(fault.load()?"AUDIO FAULT":gate.armed?"ARMED / X tempo / Y warp":"BOOT to arm");
   const char* labels[]={"I","II","III","IV","Play","Reset","Var","Panic"};
   for(unsigned i=0;i<8;++i){lcd.setCursor(i*40+2,216);lcd.print(labels[i]);}
  }
 }
 delay(1);
}
