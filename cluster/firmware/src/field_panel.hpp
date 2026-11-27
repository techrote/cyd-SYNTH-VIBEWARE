#pragma once
// Isolated classic ESP32-2432S028R candidate. Hardware configuration facts
// from cyd-diviner 7b1b37e/src/hal_cyd.cpp; independent application.
#include "../../field_av.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <LovyanGFX.hpp>
#include <driver/i2s.h>
#include <esp_timer.h>
#include <atomic>
namespace field_panel {
using namespace cluster;
class Screen : public lgfx::LGFX_Device {
    lgfx::Bus_SPI spi_;
    lgfx::Panel_ILI9341_2 panel_;
public:
    Screen() {
        auto bus=spi_.config();
        bus.spi_host=SPI2_HOST;bus.freq_write=40000000;bus.freq_read=8000000;
        bus.spi_3wire=false;bus.pin_sclk=14;bus.pin_mosi=13;bus.pin_miso=12;bus.pin_dc=2;
        spi_.config(bus);panel_.setBus(&spi_);
        auto display=panel_.config();
        display.pin_cs=15;display.pin_rst=-1;display.invert=true;display.bus_shared=false;
        panel_.config(display);setPanel(&panel_);
    }
};
class Panel {
    static constexpr unsigned block=128;
    Screen screen_;
    SPIClass touch_bus_{VSPI};
    XPT2046_Touchscreen touch_{33};
    bool display_=false,contact_=false,armed_=false,audio_started_=false;
    std::atomic<uint32_t> word_{0},lease_{0},audio_blocks_{0},audio_errors_{0},audio_max_us_{0};
    std::atomic<bool> audio_fault_{false};
    uint32_t last_touch_=0,last_frame_=0,frames_=0,slice_max_us_=0,touch_max_us_=0;
    unsigned y_=av::height;
    uint16_t touch_x_=32768,touch_y_=32768;
    Grid snapshot_{};
    Job snapshot_job_{};
    bool snapshot_remote_=false;
    std::array<uint16_t,av::width> rgb_{};
    std::array<lgfx::rgb565_t,av::width> line_{};
    static void audio_task(void* arg) {
        auto& self=*static_cast<Panel*>(arg);
        av::Voice voice;
        std::array<int16_t,block> mono{};
        std::array<uint16_t,block*2> stereo{};
        for(;;) {
            const auto stamp=self.lease_.load(std::memory_order_acquire);
            const uint32_t age=uint32_t(millis()-stamp);
            const uint32_t word=age>120||self.audio_fault_.load()?0:self.word_.load(std::memory_order_acquire);
            const int64_t before=esp_timer_get_time();
            voice.render(word,mono.data(),mono.size());
            for(unsigned i=0;i<block;++i) {
                const uint16_t v=uint16_t(32768+int32_t(mono[i])/2);
                stereo[i*2]=v;stereo[i*2+1]=v;
            }
            const uint32_t compute_us=uint32_t(esp_timer_get_time()-before);
            if(compute_us>self.audio_max_us_.load())self.audio_max_us_.store(compute_us);
            size_t written=0;
            const auto rc=i2s_write(I2S_NUM_0,stereo.data(),sizeof(stereo),&written,pdMS_TO_TICKS(20));
            if(rc!=ESP_OK||written!=sizeof(stereo)) {
                self.audio_errors_.fetch_add(1);
                self.audio_fault_.store(true);
                vTaskDelay(1);
            } else self.audio_blocks_.fetch_add(1);
        }
    }
    static uint16_t axis(int raw) {
        return uint16_t(uint32_t(std::clamp(raw,200,3900)-200)*65535u/3700u);
    }
public:
    bool begin() {
        pinMode(0,INPUT_PULLUP);pinMode(21,OUTPUT);digitalWrite(21,LOW);
        display_=screen_.init();
        if(display_) {
            screen_.setRotation(5);
            display_=screen_.width()==int(av::width)&&screen_.height()==int(av::height);
        }
        if(display_) {
            screen_.fillScreen(uint16_t(0));digitalWrite(21,HIGH);
            touch_bus_.begin(25,39,32,33);touch_.begin(touch_bus_);touch_.setRotation(0);
        }
        return display_;
    }
    bool arm() {
        if(!display_||audio_fault_.load()||digitalRead(0)==LOW)return false;
        if(!audio_started_) {
            i2s_config_t config{};
            config.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN);
            config.sample_rate=av::rate;config.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
            config.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
            config.communication_format=I2S_COMM_FORMAT_STAND_MSB;
            config.dma_buf_count=4;config.dma_buf_len=block;config.tx_desc_auto_clear=true;
            if(i2s_driver_install(I2S_NUM_0,&config,0,nullptr)!=ESP_OK)return false;
            if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)!=ESP_OK||
                xTaskCreatePinnedToCore(audio_task,"field-audio",4096,this,5,nullptr,0)!=pdPASS) {
                i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);i2s_driver_uninstall(I2S_NUM_0);return false;
            }
            audio_started_=true;
        }
        armed_=true;return true;
    }
    void mute() {armed_=false;word_.store(0,std::memory_order_release);}
    void input(av::Field& field,uint32_t now) {
        if(digitalRead(0)==LOW||audio_fault_.load())mute();
        if(display_&&uint32_t(now-last_touch_)>=12) {
            last_touch_=now;const auto before=esp_timer_get_time();
            const auto p=touch_.getPoint();
            touch_max_us_=std::max(touch_max_us_,uint32_t(esp_timer_get_time()-before));
            contact_=p.z>=400;
            if(contact_){touch_x_=axis(p.y);touch_y_=axis(4100-int(p.x));field.xy(touch_x_,touch_y_);}
        }
        field.gate(contact_&&armed_&&uint32_t(now-last_touch_)<=40);
    }
    uint16_t x()const{return touch_x_;}
    uint16_t y()const{return touch_y_;}
    void output(const av::Field& field,uint32_t now,bool timed=false,uint32_t timed_word=0,bool follower=false,bool synced=false) {
        word_.store(armed_&&contact_&&uint32_t(now-last_touch_)<=40?(timed?timed_word:field.sound()):0,std::memory_order_release);
        lease_.store(now,std::memory_order_release);
        if(!display_)return;
        if(y_==av::height) {
            if(uint32_t(now-last_frame_)<100)return;
            last_frame_=now;y_=0;snapshot_=field.grid();snapshot_job_=field.shown();
            snapshot_remote_=field.last_completion()==Completion::remote;
        }
        const auto before=esp_timer_get_time();
        screen_.startWrite();
        for(unsigned count=0;count<8&&y_<av::height;++count,++y_) {
            av::row(snapshot_,snapshot_job_,y_,rgb_.data());
            for(unsigned x=0;x<av::width;++x)line_[x]=rgb_[x];
            screen_.pushImage(0,int(y_),int(av::width),1,line_.data());
        }
        if(y_==av::height) {
            screen_.fillRect(0,224,320,16,uint16_t(0));
            screen_.setTextColor(uint16_t(0xffff));screen_.setTextSize(1);
            screen_.drawString(timed?(follower?(synced?"FOLLOW SYNC":"FOLLOW LOCAL"):"LEADER"):(snapshot_remote_?"REMOTE":"LOCAL"),3,227);
            screen_.drawString(audio_fault_.load()?"AUDIO FAULT":(armed_?"AUDIO ARMED":"MUTED"),90,227);
            if(timed)screen_.fillRect(305,227,10,10,timed_word?uint16_t(0xffe0):uint16_t(0x4208));
            ++frames_;
        }
        screen_.endWrite();
        slice_max_us_=std::max(slice_max_us_,uint32_t(esp_timer_get_time()-before));
    }
    void status() const {
        Serial.printf("panel=%u frames=%lu slice_max_us=%lu touch_max_us=%lu audio_started=%u armed=%u audio_blocks=%lu audio_write_errors=%lu audio_compute_max_us=%lu audio_fault=%u\n",
            unsigned(display_),static_cast<unsigned long>(frames_),static_cast<unsigned long>(slice_max_us_),
            static_cast<unsigned long>(touch_max_us_),unsigned(audio_started_),unsigned(armed_),
            static_cast<unsigned long>(audio_blocks_.load()),static_cast<unsigned long>(audio_errors_.load()),
            static_cast<unsigned long>(audio_max_us_.load()),unsigned(audio_fault_.load()));
    }
};
} // namespace field_panel
