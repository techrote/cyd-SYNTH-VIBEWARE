// Independent adapter; board facts reviewed from Diviner and HYBRID.
#include "../../core.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <LovyanGFX.hpp>
#include <driver/i2s.h>
using namespace gridseq;
class Display:public lgfx::LGFX_Device{
    lgfx::Bus_SPI bus_;lgfx::Panel_ILI9341_2 panel_;
public:
    Display(){
        auto b=bus_.config();b.spi_host=SPI2_HOST;b.freq_write=40000000;b.freq_read=8000000;
        b.spi_3wire=false;b.pin_sclk=14;b.pin_mosi=13;b.pin_miso=12;b.pin_dc=2;
        bus_.config(b);panel_.setBus(&bus_);
        auto p=panel_.config();p.pin_cs=15;p.pin_rst=-1;p.invert=true;p.bus_shared=false;
        panel_.config(p);setPanel(&panel_);
    }
};
static Display display;static SPIClass touchBus(VSPI);static XPT2046_Touchscreen touch(33);
static Surface surface;static Exchange exchange;static Control frameControl;
static uint32_t frameHead=0;static std::atomic<uint32_t> head{0};
static std::atomic<bool> fault{false};static bool ready=false,started=false,armed=false,awaitRelease=true;
static uint32_t inputAt=0,frameAt=0;static unsigned scan=height;
static std::array<uint16_t,width> rowbuf{};
static std::array<lgfx::rgb565_t,width> line{};
static void audioTask(void*){
    Engine e;Control c;std::array<int16_t,block> pcm{};std::array<uint16_t,block*2> dac{};
    for(;;){
        exchange.read(c);
        if(fault.load())c.play=false;
        e.render(c,pcm.data(),pcm.size(),&head);
        for(unsigned i=0;i<block;++i){
            uint16_t v=uint16_t(32768+int32_t(pcm[i])/2);
            dac[2*i]=dac[2*i+1]=v;
        }
        size_t written=0;
        if(i2s_write(I2S_NUM_0,dac.data(),sizeof(dac),&written,pdMS_TO_TICKS(20))!=ESP_OK||written!=sizeof(dac)){
            fault=true;vTaskDelay(1);
        }
    }
}
static bool startAudio(){
    if(started)return true;
    i2s_config_t cfg{};cfg.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate=rate;cfg.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;cfg.communication_format=I2S_COMM_FORMAT_STAND_MSB;
    cfg.dma_buf_count=4;cfg.dma_buf_len=block;cfg.tx_desc_auto_clear=true;
    if(i2s_driver_install(I2S_NUM_0,&cfg,0,nullptr)!=ESP_OK)return false;
    if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)!=ESP_OK||
       xTaskCreatePinnedToCore(audioTask,"grid-audio",6144,nullptr,5,nullptr,0)!=pdPASS){
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);i2s_driver_uninstall(I2S_NUM_0);return false;
    }
    started=true;return true;
}
static int axis(int n,int extent){return (std::clamp(n,200,3900)-200)*(extent-1)/3700;}
void setup(){
    Serial.begin(115200);pinMode(0,INPUT_PULLUP);pinMode(21,OUTPUT);digitalWrite(21,LOW);
    ready=display.init();if(ready){display.setRotation(5);ready=display.width()==320&&display.height()==240;}
    if(ready){
        touchBus.begin(25,39,32,33);touch.begin(touchBus);touch.setRotation(0);
        display.fillScreen(uint16_t(0));digitalWrite(21,HIGH);
    }
    surface.cancel();exchange.publish(surface.c);
    Serial.println("CHRONOGRID engineering candidate; muted. a=arm, m=mute. Physical HOLD.");
}
void loop(){
    if(Serial.available()){
        int c=Serial.read();
        if(c=='a'&&ready&&!fault.load()&&digitalRead(0)==HIGH&&startAudio()){surface.cancel();awaitRelease=true;armed=true;}
        if(c=='m'){armed=false;awaitRelease=true;surface.cancel();exchange.publish(surface.c);}
    }
    if(digitalRead(0)==LOW||fault.load()){
        armed=false;awaitRelease=true;surface.cancel();exchange.publish(surface.c);
    }
    uint32_t now=millis();
    if(ready&&uint32_t(now-inputAt)>=12){
        auto p=touch.getPoint();inputAt=millis();
        bool down=p.z>=400;
        if(!down)awaitRelease=false;
        surface.touch(down&&!awaitRelease,axis(p.y,320),axis(4100-int(p.x),240));
        if(!armed)surface.c.play=false;
        exchange.publish(surface.c);
    }
    if(ready){
        if(scan==height&&uint32_t(now-frameAt)>=50){
            frameControl=surface.c;frameHead=head.load(std::memory_order_acquire);
            if(!armed){frameControl.play=false;frameHead=0;}
            scan=0;frameAt=now;
        }
        if(scan<height){
            display.startWrite();
            for(unsigned n=0;n<4&&scan<height;++n,++scan){
                gridseq::row(frameControl,frameHead,scan,rowbuf.data());
                for(unsigned x=0;x<width;++x)line[x]=rowbuf[x];
                display.pushImage(0,int(scan),int(width),1,line.data());
            }
            display.endWrite();
        }
    }
    delay(1);
}
