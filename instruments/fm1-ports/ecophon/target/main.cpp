// Local adapter, not authorized installable firmware. Board facts reviewed at
// Diviner 7b1b37eec6ac126f334a0b008d91135071ea60fa; local PORT Chronomorph
// e57f1bc20de0924ec976ff6abce7b5cae903f1a2 supplies a compile-qualified profile.
#include "../target_gate.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
#include <driver/i2s.h>
using namespace ecophon;
namespace {
constexpr unsigned block=128, strip=4;
class Screen final : public lgfx::LGFX_Device {
    lgfx::Bus_SPI bus;
    lgfx::Panel_ILI9341_2 panel;
public:
    Screen(){
        auto b=bus.config();
        b.spi_host=SPI2_HOST; b.freq_write=40000000; b.freq_read=8000000;
        b.spi_3wire=false; b.pin_sclk=14; b.pin_mosi=13; b.pin_miso=12; b.pin_dc=2;
        bus.config(b);panel.setBus(&bus);
        auto p=panel.config();p.pin_cs=15;p.pin_rst=-1;p.invert=true;p.bus_shared=false;
        panel.config(p);setPanel(&panel);
    }
};
Screen lcd;
SPIClass touchBus(VSPI);
XPT2046_Touchscreen touch(33);
Input input;
Views views;
TargetGate gate;
std::atomic<bool> audioFault{false};
bool ready=false,started=false;
bool silenceDriver(){
    // Disconnect analog output before clearing the DMA ring. This is a
    // candidate backend sequence, not a measured hardware mute latency.
    if(i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE)!=ESP_OK)return false;
    if(i2s_stop(I2S_NUM_0)!=ESP_OK)return false;
    if(i2s_zero_dma_buffer(I2S_NUM_0)!=ESP_OK)return false;
    return i2s_start(I2S_NUM_0)==ESP_OK;
}
void audioTask(void*){
    Runtime rt;
    uint32_t serviced=rt.panic_epoch;
    bool dacOn=false;
    std::array<int16_t,block> pcm{};
    std::array<uint16_t,2*block> slots{};
    views.publish(rt.engine);
    for(;;){
        rt.render(input,millis(),pcm.data(),pcm.size());
        views.publish(rt.engine);
        if(serviced!=rt.panic_epoch||(dacOn&&rt.engine.muted)){
            if(!silenceDriver())break;
            serviced=rt.panic_epoch;dacOn=false;
            // Do not submit the pre-drain block, even if restarted in pump.
            continue;
        }
        if(input.fault.load(std::memory_order_acquire))continue;
        for(unsigned i=0;i<block;++i)slots[2*i]=slots[2*i+1]=dacWord(pcm[i]);
        size_t sent=0;
        if(i2s_write(I2S_NUM_0,slots.data(),sizeof(slots),&sent,pdMS_TO_TICKS(20))!=ESP_OK||
           sent!=sizeof(slots))break;
        if(!dacOn&&!rt.engine.muted&&!input.fault.load(std::memory_order_acquire)){
            if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)!=ESP_OK)break;
            dacOn=true;
        }
    }
    audioFault.store(true,std::memory_order_release);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    i2s_stop(I2S_NUM_0);
    vTaskDelete(nullptr);
}
bool startAudio(){
    if(started)return true;
    i2s_config_t c{};
    c.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN);
    c.sample_rate=rate;c.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
    c.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
    c.communication_format=I2S_COMM_FORMAT_STAND_MSB;
    c.dma_buf_count=4;c.dma_buf_len=block;c.tx_desc_auto_clear=true;
    if(i2s_driver_install(I2S_NUM_0,&c,0,nullptr)!=ESP_OK)return false;
    if(i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE)!=ESP_OK||
       xTaskCreatePinnedToCore(audioTask,"ecophon-audio",6144,nullptr,5,nullptr,0)!=pdPASS){
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
        i2s_driver_uninstall(I2S_NUM_0);return false;
    }
    started=true;return true;
}
void present(uint32_t now){
    static uint32_t frameAt=0;
    static unsigned scan=240;
    static Engine frame=[](){Engine e;e.reset(0,42);e.panic();return e;}();
    static std::array<uint16_t,320> raw{};
    static std::array<lgfx::rgb565_t,320*strip> transfer{};
    if(scan==240&&uint32_t(now-frameAt)>=50){
        views.latest(frame);
        if(!gate.armed||audioFault.load())frame.panic();
        scan=0;frameAt=now;
    }
    if(scan==240)return;
    unsigned count=std::min(strip,240-scan);
    for(unsigned i=0;i<count;++i){
        row(frame,scan+i,raw.data());
        for(unsigned x=0;x<320;++x)transfer[i*320+x]=raw[x];
    }
    lcd.pushImage(0,int(scan),320,int(count),transfer.data());
    lcd.waitDMA();
    scan+=count;
    if(scan==240){
        lcd.setTextColor(0xffff);
        const char* scenes[]={"Mutualism","Scarcity","Bloom","Frozen"};
        const char* memories[]={"Capture A","Capture B","Recall A","Recall B"};
        for(int i=0;i<4;++i){
            lcd.setCursor(i*80+2,8);lcd.print(scenes[i]);
            lcd.setCursor(i*80+2,199);lcd.print(memories[i]);
        }
        lcd.setCursor(4,88);lcd.print("Sharing");
        lcd.setCursor(4,114);lcd.print("Pressure up / Fertility right");
        lcd.setCursor(4,224);
        lcd.print(audioFault.load()?"Audio fault - reboot":!gate.armed?"BOOT to arm":frame.muted?"Release then select scene":frame.frozen?"Resume ecology":"Freeze ecology / BOOT disarm");
        lcd.setCursor(283,224);lcd.print("Panic");
    }
}
}
void setup(){
    pinMode(0,INPUT_PULLUP);
    pinMode(21,OUTPUT);digitalWrite(21,LOW);
    if(!lcd.init())return;
    lcd.setRotation(5);
    if(lcd.width()!=320||lcd.height()!=240)return;
    touchBus.begin(25,39,32,33);
    if(!touch.begin(touchBus))return;
    touch.setRotation(0);
    lcd.fillScreen(uint16_t(0));digitalWrite(21,HIGH);ready=true;
}
void loop(){
    if(!ready){delay(100);return;}
    static uint32_t touchAt=0;
    uint32_t now=millis();
    if(uint32_t(now-touchAt)>=12){
        auto p=touch.getPoint();touchAt=millis();
        gate.poll(input,digitalRead(0)==LOW,p.z>=400,touchAxis(p.y,320),
                  touchAxis(4100-int(p.x),240),touchAt,audioFault.load());
        if(gate.armed&&!started&&!audioFault.load()&&!startAudio())audioFault.store(true);
    }
    present(now);
    delay(1);
}
