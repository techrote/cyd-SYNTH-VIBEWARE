#pragma once
// Compile-only candidate for the classic ESP32 GPIO26 DAC. Exclusive TG0/T0,
// DAC2 and DAC digital-route ownership are requirements of this standalone
// profile, not dynamically discovered reservations. No other audio driver.
#include "clock_sink.hpp"
#include "timer_lifecycle.hpp"
#include <Arduino.h>
#include <driver/dac.h>
#include <driver/timer.h>
#include <esp_intr_alloc.h>
#include <esp_pm.h>
#include <soc/rtc_io_reg.h>
#include <soc/soc.h>
namespace cyd_platform {
// Single translation unit, static-lifetime sink in internal DRAM. Name kept
// for the exact linked disassembly audit. The IDF wrapper may take a spinlock.
extern "C" bool IRAM_ATTR __attribute__((noinline,used)) p15_sample_tick(void* arg){
    const auto code=static_cast<ClockSink*>(arg)->tick();
    REG_SET_FIELD(RTC_IO_PAD_DAC2_REG,RTC_IO_PDAC2_DAC,code);
    return false;
}
class Esp32TimerDac {
    ClockSink& sink_;
#if CONFIG_PM_ENABLE
    esp_pm_lock_handle_t lock_=nullptr;
#endif
public:
    explicit Esp32TimerDac(ClockSink& s):sink_(s){}
    bool owner_core()const{return xPortGetCoreID()==1;}
    bool acquire_clock(){
#if CONFIG_PM_ENABLE
        if(esp_pm_lock_create(ESP_PM_APB_FREQ_MAX,0,"p15dac",&lock_)!=ESP_OK)return false;
        if(esp_pm_lock_acquire(lock_)!=ESP_OK){esp_pm_lock_delete(lock_);return false;}
#endif
        if(getApbFrequency()==80000000u)return true;
        release_clock();return false;
    }
    bool release_clock(){
#if CONFIG_PM_ENABLE
        if(esp_pm_lock_release(lock_)!=ESP_OK)return false;
        return esp_pm_lock_delete(lock_)==ESP_OK;
#else
        return true;
#endif
    }
    bool init_timer(){
        timer_config_t c{};c.alarm_en=TIMER_ALARM_EN;c.counter_en=TIMER_PAUSE;
        c.intr_type=TIMER_INTR_LEVEL;c.counter_dir=TIMER_COUNT_UP;
        c.auto_reload=TIMER_AUTORELOAD_EN;c.divider=2;
        return timer_init(TIMER_GROUP_0,TIMER_0,&c)==ESP_OK;
    }
    bool configure_timer(){
        return timer_set_counter_value(TIMER_GROUP_0,TIMER_0,0)==ESP_OK&&
               timer_set_alarm_value(TIMER_GROUP_0,TIMER_0,1667)==ESP_OK;
    }
    bool install(){return timer_isr_callback_add(TIMER_GROUP_0,TIMER_0,p15_sample_tick,&sink_,ESP_INTR_FLAG_IRAM)==ESP_OK;}
    void midpoint(){REG_SET_FIELD(RTC_IO_PAD_DAC2_REG,RTC_IO_PDAC2_DAC,ClockSink::midpoint);}
    bool prepare_dac(){
        if(dac_i2s_disable()!=ESP_OK||!disable())return false;
        midpoint();return true;
    }
    bool enable(){return dac_output_enable(DAC_CHANNEL_2)==ESP_OK;}
    bool start_timer(){return timer_start(TIMER_GROUP_0,TIMER_0)==ESP_OK;}
    bool pause(){return timer_pause(TIMER_GROUP_0,TIMER_0)==ESP_OK;}
    bool disable(){return dac_output_disable(DAC_CHANNEL_2)==ESP_OK;}
    bool remove(){return timer_isr_callback_remove(TIMER_GROUP_0,TIMER_0)==ESP_OK;}
    bool deinit(){return timer_deinit(TIMER_GROUP_0,TIMER_0)==ESP_OK;}
};
}
