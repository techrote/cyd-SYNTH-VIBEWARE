// XENOBIO original local adapter. Pin/profile facts from reviewed Diviner
// 7b1b37e and in-repo CHRONOGRID c0a866a; no external app code adopted.
#include "../../runtime.hpp"
#include "../../target_input.hpp"
#include "../../labeled.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <LovyanGFX.hpp>
#include <driver/i2s.h>
using namespace xenobio;
namespace {
constexpr unsigned block=128, strip=4;
class LCD : public lgfx::LGFX_Device {
    lgfx::Bus_SPI spi_;
    lgfx::Panel_ILI9341_2 panel_;
public:
    LCD() {
        auto s=spi_.config();
        s.spi_host=SPI2_HOST; s.freq_write=40000000; s.freq_read=8000000;
        s.spi_3wire=false; s.pin_sclk=14; s.pin_mosi=13; s.pin_miso=12; s.pin_dc=2;
        spi_.config(s); panel_.setBus(&spi_);
        auto p=panel_.config(); p.pin_cs=15; p.pin_rst=-1; p.invert=true; p.bus_shared=false;
        panel_.config(p); setPanel(&panel_);
    }
};
LCD lcd;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(33);
Runtime runtime;
TargetInput gate;
std::atomic<bool> audioFault{false};
bool lcdReady=false, audioStarted=false;
unsigned row=240;
uint32_t touchAt=0, frameAt=0;
Engine frame;
std::array<uint16_t,320> pixels{};
std::array<lgfx::rgb565_t,320*strip> transfer{};

void audioTask(void*) {
    std::array<int16_t,block> pcm{};
    std::array<uint16_t,2*block> output{};
    for (;;) {
        runtime.render(pcm.data(),pcm.size());
        bool fault=audioFault.load(std::memory_order_acquire);
        for(unsigned i=0;i<block;++i) {
            const auto word=dac_word(fault?int16_t(0):pcm[i]);
            output[2*i]=word; output[2*i+1]=word;
        }
        size_t sent=0;
        if(i2s_write(I2S_NUM_0,output.data(),sizeof(output),&sent,pdMS_TO_TICKS(20))!=ESP_OK ||
           sent!=sizeof(output)) {
            audioFault.store(true,std::memory_order_release);
            vTaskDelay(1);
        }
    }
}
bool enableAudio() {
    if(audioStarted) return true;
    i2s_config_t cfg{};
    cfg.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate=rate; cfg.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format=I2S_COMM_FORMAT_STAND_MSB;
    cfg.dma_buf_count=4; cfg.dma_buf_len=block; cfg.tx_desc_auto_clear=true;
    if(i2s_driver_install(I2S_NUM_0,&cfg,0,nullptr)!=ESP_OK) return false;
    if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)!=ESP_OK ||
       xTaskCreatePinnedToCore(audioTask,"xeno-audio",6144,nullptr,5,nullptr,0)!=pdPASS) {
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }
    audioStarted=true; return true;
}
void presentStrip(uint32_t now) {
    if(row==240 && uint32_t(now-frameAt)>=50) {
        frame=runtime.snapshot(); row=0; frameAt=now;
    }
    if(row==240) return;
    const unsigned count=std::min(strip,240-row);
    for(unsigned dy=0;dy<count;++dy) {
        labeled_row(frame,int(row+dy),pixels.data());
        for(unsigned x=0;x<320;++x) transfer[dy*320+x]=pixels[x];
    }
    lcd.pushImage(0,int(row),320,int(count),transfer.data());
    lcd.waitDMA(); // no buffer mutation before the driver finishes consuming it
    row+=count;
}
}
void setup() {
    Serial.begin(115200);
    pinMode(0,INPUT_PULLUP); pinMode(21,OUTPUT); digitalWrite(21,LOW);
    lcdReady=lcd.init();
    if(lcdReady) {
        lcd.setRotation(5); lcdReady=lcd.width()==320 && lcd.height()==240;
    }
    if(lcdReady) {
        touchSpi.begin(25,39,32,33); touch.begin(touchSpi); touch.setRotation(0);
        lcd.fillScreen(uint16_t(0)); digitalWrite(21,HIGH);
    }
    Serial.println("XENOBIO engineering candidate, physical HOLD. a=arm, m=mute, BOOT=disarm.");
}
void loop() {
    if(Serial.available()) {
        int cmd=Serial.read();
        if(cmd=='m') gate.disarm(runtime.input);
        if(cmd=='a' && lcdReady && digitalRead(0)==HIGH &&
           !audioFault.load(std::memory_order_acquire)) {
            if(enableAudio()) gate.arm(runtime.input);
            else audioFault.store(true,std::memory_order_release);
        }
    }
    if(digitalRead(0)==LOW || audioFault.load(std::memory_order_acquire))
        gate.disarm(runtime.input);
    uint32_t now=millis();
    if(lcdReady && uint32_t(now-touchAt)>=12) {
        auto p=touch.getPoint();
        touchAt=millis();
        gate.poll(runtime.input,p.z>=400,touch_axis(p.y,320),touch_axis(4100-int(p.x),240));
    }
    if(lcdReady) presentStrip(now);
    delay(1);
}
