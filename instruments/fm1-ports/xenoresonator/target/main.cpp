#include "../exchange.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
#include <driver/i2s.h>
using namespace portx;
class Display:public lgfx::LGFX_Device{
 lgfx::Bus_SPI bus;lgfx::Panel_ILI9341_2 panel;
 public:Display(){auto b=bus.config();b.spi_host=SPI2_HOST;b.freq_write=40000000;b.freq_read=8000000;b.spi_3wire=false;b.pin_sclk=14;b.pin_mosi=13;b.pin_miso=12;b.pin_dc=2;bus.config(b);panel.setBus(&bus);auto c=panel.config();c.pin_cs=15;c.pin_rst=-1;c.invert=true;c.bus_shared=false;panel.config(c);setPanel(&panel);}
};
static Display lcd;static SPIClass touchSpi(VSPI);static XPT2046_Touchscreen touch(33);
static Exchange bus;static std::atomic<bool> armed{false},fault{false};static bool screen=false;
static void audio(void*){Engine e;uint32_t last=0,alarm=0;int16_t pcm[128];uint16_t stereo[256];while(true){
 if(!armed.load()){e.panic();e.release();last=bus.input.load();}else bus.pump(e,last,alarm);
 e.render(pcm,128);bus.publish(e.view());for(unsigned i=0;i<128;++i){int v=std::clamp(32768+int(pcm[i])/2,0,65535);stereo[2*i]=stereo[2*i+1]=uint16_t(v&0xff00);}size_t written=0;if(i2s_write(I2S_NUM_0,stereo,sizeof(stereo),&written,pdMS_TO_TICKS(20))!=ESP_OK||written!=sizeof(stereo)){armed=false;fault=true;i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);vTaskDelete(nullptr);}
 }}
void setup(){Serial.begin(115200);pinMode(21,OUTPUT);digitalWrite(21,LOW);pinMode(0,INPUT_PULLUP);screen=lcd.init();if(!screen)return;lcd.setRotation(5);digitalWrite(21,HIGH);touchSpi.begin(25,39,32,33);touch.begin(touchSpi);touch.setRotation(0);
 i2s_config_t c{};c.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN);c.sample_rate=rate;c.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;c.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;c.communication_format=I2S_COMM_FORMAT_STAND_MSB;c.dma_buf_count=4;c.dma_buf_len=128;c.tx_desc_auto_clear=true;
 if(i2s_driver_install(I2S_NUM_0,&c,0,nullptr)!=ESP_OK){fault=true;return;}if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)!=ESP_OK||xTaskCreatePinnedToCore(audio,"port-xeno",4096,nullptr,5,nullptr,0)!=pdPASS){fault=true;i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);i2s_driver_uninstall(I2S_NUM_0);}
}
void loop(){if(!screen){delay(100);return;}static int row=0;static View v;static uint16_t line[320];static bool boot=false,blocked=false;static uint8_t debounce=0;static uint32_t scanned=0;uint32_t now=millis();if(now-scanned>=12){scanned=now;auto p=touch.getPoint();bool d=p.z>=400;int x=std::clamp(int(p.y)-200,0,3700)*319/3700,y=std::clamp(3900-int(p.x),0,3700)*239/3700;debounce=((debounce<<1)|(digitalRead(0)==LOW))&7;if(debounce==7&&!boot){boot=true;armed=!armed.load()&&!fault.load();bus.stop();blocked=true;}if(!debounce)boot=false;if(!d)blocked=false;if(d&&y>=184&&x>=266){bus.stop();blocked=true;}if(armed&&!blocked)bus.send(d,x,y);else bus.send(false,0,0);}
 if(row==0)v=bus.read();for(int j=0;j<8;++j){for(int x=0;x<320;++x)line[x]=pixel(v,x,row+j);lcd.pushImage(0,row+j,320,1,line);}row=(row+8)%240;if(row==0){lcd.setTextColor(0xffff);lcd.setCursor(2,2);lcd.print(fault?"AUDIO FAULT":armed?"ARMED / strike":"BOOT to arm");const char* labels[]={"Cer","Wood","Alien","Mem","Couple","Panic"};for(int i=0;i<6;++i){lcd.setCursor(i*53+2,210);lcd.print(labels[i]);}}delay(1);}
