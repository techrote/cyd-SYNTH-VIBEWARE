// Experimental serial-controlled compute diagnostic, not a display/audio firmware.
// Arduino ESP32 2.0.17 / IDF 4.4.7 ESP-NOW API, fixed unicast peer, radio opt-in.
#include "../../link.hpp"
#include "../../pairing.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#if CLUSTER_FIELD
#include "field_panel.hpp"
#if CLUSTER_DUAL
#include "../../dual.hpp"
#endif
#endif

using namespace cluster;
namespace {
constexpr uint8_t channel=6;
constexpr unsigned rx_depth=8;
StaticQueue_t rx_queue_storage;
alignas(Frame) uint8_t rx_storage[rx_depth*sizeof(Frame)];
QueueHandle_t rx_queue=nullptr;
std::array<uint8_t,6> peer{}, own{};
std::atomic<uint32_t> tx_busy{0}, tx_fail{0}, rx_drop{0}, rx_reject{0};
bool paired=false, attempted=false, remote_enabled=false;
Link controller_link(1);
Worker worker(1);
#if CLUSTER_FIELD
av::Field interactive(1);
field_panel::Panel panel;
#if CLUSTER_DUAL
dual::Peer musical;
#endif
#endif
bool running=false;
uint32_t last_job=0, next_seed=1;
int64_t started_us=0;
Job job{}, active_job{};
char line[96]{};
std::size_t used=0;
bool overflow=false;

void on_receive(const uint8_t* mac,const uint8_t* bytes,int n) {
    if(!mac||!bytes||std::memcmp(mac,peer.data(),peer.size())||n<int(header)||n>int(max_frame)) {
        rx_reject.fetch_add(1,std::memory_order_relaxed); return;
    }
    Frame f; f.size=std::size_t(n); std::memcpy(f.bytes.data(),bytes,f.size);
    // Wi-Fi task, not an ISR; no compute, logging, waiting or allocation here.
    if(xQueueSend(rx_queue,&f,0)!=pdTRUE) rx_drop.fetch_add(1,std::memory_order_relaxed);
}
void on_sent(const uint8_t*,esp_now_send_status_t status) {
    if(status!=ESP_NOW_SEND_SUCCESS) tx_fail.fetch_add(1,std::memory_order_relaxed);
    tx_busy.store(0,std::memory_order_release);
}
bool send_frame(const Frame& f) {
    if(!paired||!remote_enabled||!f.size||f.size>max_frame) return false;
    uint32_t expected=0;
    if(!tx_busy.compare_exchange_strong(expected,1,std::memory_order_acq_rel)) return false;
    if(esp_now_send(peer.data(),f.bytes.data(),f.size)!=ESP_OK) {
        tx_fail.fetch_add(1,std::memory_order_relaxed);
        tx_busy.store(0,std::memory_order_release); return false;
    }
    return true;
}
bool provision(const Pairing& p) {
    if(attempted||p.peer==own||controller_link.pending()||!rx_queue) return false;
#if CLUSTER_FIELD
    if(interactive.pending())return false;
#endif
    attempted=true; peer=p.peer;
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    if(!WiFi.mode(WIFI_STA)) return false;
    WiFi.disconnect(false,false);
    bool initialized=false;
    bool ok=esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)==ESP_OK;
    if(ok) { initialized=esp_now_init()==ESP_OK; ok=initialized; }
    if(ok) ok=esp_now_set_pmk(p.pmk.data())==ESP_OK;
    if(ok) {
        esp_now_peer_info_t info{};
        std::memcpy(info.peer_addr,p.peer.data(),p.peer.size());
        std::memcpy(info.lmk,p.lmk.data(),p.lmk.size());
        info.channel=channel; info.ifidx=WIFI_IF_STA; info.encrypt=true;
        ok=esp_now_add_peer(&info)==ESP_OK;
        wipe(&info,sizeof(info));
    }
    if(ok) ok=esp_now_register_send_cb(on_sent)==ESP_OK;
    if(ok) ok=esp_now_register_recv_cb(on_receive)==ESP_OK;
    if(!ok) {
        if(initialized) esp_now_deinit();
        WiFi.mode(WIFI_OFF); return false;
    }
    controller_link=Link(p.epoch); worker=Worker(p.epoch);
    paired=true; remote_enabled=true;
#if CLUSTER_FIELD
    panel.mute();interactive=av::Field(p.epoch);
#if CLUSTER_DUAL
    musical=dual::Peer(p.epoch,own<peer);musical.network(true);
#else
    interactive.remote(true);
#endif
#endif
    return true;
}
void status() {
    Serial.printf("paired=%u remote_enabled=%u tx_busy=%lu tx_fail=%lu rx_drop=%lu rx_reject=%lu heap=%u min_heap=%u\n",
        unsigned(paired),unsigned(remote_enabled),static_cast<unsigned long>(tx_busy.load()),
        static_cast<unsigned long>(tx_fail.load()),static_cast<unsigned long>(rx_drop.load()),
        static_cast<unsigned long>(rx_reject.load()),unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()));
#if CLUSTER_FIELD
    const auto& stats=interactive.stats();
    Serial.printf("ready=%u pending=%u remote=%lu local=%lu retry=%lu\n",unsigned(interactive.ready()),unsigned(interactive.pending()),static_cast<unsigned long>(stats.remote),static_cast<unsigned long>(stats.local),static_cast<unsigned long>(stats.retries));
    panel.status();
#if CLUSTER_DUAL
    const auto& d=musical.stats;
    Serial.printf("dual leader=%u synced=%u ppm=%ld samples=%lu sent=%lu played=%lu local=%lu reject=%lu late_ms=%lu\\n",unsigned(musical.leader()),unsigned(musical.synchronized(millis())),static_cast<long>(musical.clock.ppm),static_cast<unsigned long>(d.samples),static_cast<unsigned long>(d.events_sent),static_cast<unsigned long>(d.played),static_cast<unsigned long>(d.local),static_cast<unsigned long>(d.rejected),static_cast<unsigned long>(d.max_late_ms));
#endif
#elif CLUSTER_COORDINATOR
    Serial.printf("ready=%u pending=%u remote=%lu local=%lu retry=%lu hello=%lu accepted=%lu busy=%lu\n",
        unsigned(controller_link.ready()),unsigned(controller_link.pending()),static_cast<unsigned long>(controller_link.stats.remote),
        static_cast<unsigned long>(controller_link.stats.local),static_cast<unsigned long>(controller_link.stats.retries),
        static_cast<unsigned long>(controller_link.stats.hello),static_cast<unsigned long>(controller_link.stats.sent),
        static_cast<unsigned long>(controller_link.stats.busy));
#endif
}
void command(const char* text,std::size_t length) {
    if(length>=5&&std::memcmp(text,"pair ",5)==0) {
        Pairing p;
        const bool ok=parse_pairing(text,length,p)&&provision(p);
        wipe(&p,sizeof(p));
        Serial.println(ok?"pair configured (encrypted unicast requested)":"pair rejected or initialization failed; reboot before retry if attempted");
        return;
    }
    if(std::strcmp(text,"status")==0) {status();return;}
#if CLUSTER_FIELD
#if CLUSTER_DUAL
    if(std::strcmp(text,"local")==0) {remote_enabled=false;musical.network(false);return;}
    if(std::strcmp(text,"remote")==0) {remote_enabled=paired;musical.network(paired);return;}
#else
    if(std::strcmp(text,"local")==0) {remote_enabled=false;interactive.remote(false);return;}
    if(std::strcmp(text,"remote")==0) {remote_enabled=paired;interactive.remote(paired);return;}
#endif
    if(std::strcmp(text,"audio on")==0) {Serial.println(panel.arm()?"audio armed; touch for sound":"audio arm failed");return;}
    if(std::strcmp(text,"mute")==0) {panel.mute();Serial.println("muted/disarmed");return;}
#elif CLUSTER_COORDINATOR
    if(std::strcmp(text,"local")==0) {remote_enabled=false;controller_link.disconnect();Serial.println("local jobs; radio remains initialized if paired");return;}
    if(std::strcmp(text,"remote")==0) {remote_enabled=paired;Serial.println(remote_enabled?"remote enabled":"pair first");return;}
    if(std::strcmp(text,"run")==0&&!controller_link.pending()) {next_seed=1;running=true;last_job=millis()-250u;return;}
    if(std::strcmp(text,"stop")==0) {running=false;return;}
    unsigned x=0,y=0; char extra=0;
    if(std::sscanf(text,"xy %u %u %c",&x,&y,&extra)==2&&x<=65535&&y<=65535) {
        job.x=uint16_t(x);job.y=uint16_t(y);return;
    }
#endif
    Serial.println("unknown or busy command");
}
void serial_poll() {
    for(unsigned i=0;i<64&&Serial.available()>0;++i) {
        const int c=Serial.read();
        if(c=='\n') {
            if(!overflow) {line[used]=0;command(line,used);}
            else Serial.println("line too long");
            wipe(line,sizeof(line));used=0;overflow=false;
        } else if(c!='\r') {
            if(c<32||c>126||used>=sizeof(line)-1) overflow=true;
            else if(!overflow) line[used++]=char(c);
        }
    }
}
#if CLUSTER_COORDINATOR
void completed(Completion c) {
    if(c==Completion::none) return;
    const auto elapsed=esp_timer_get_time()-started_us;
    const auto before=esp_timer_get_time();
    const auto expected=compute(active_job);
    const auto verify_us=esp_timer_get_time()-before;
    uint32_t digest=2166136261u;
    for(auto x:controller_link.grid()) {digest^=x;digest*=16777619u;}
    Serial.printf("job seed=%lu mode=%s elapsed_us=%lld verify_local_us=%lld match=%u grid=%08lx\n",
        static_cast<unsigned long>(active_job.seed),c==Completion::remote?"remote":"local",
        static_cast<long long>(elapsed),static_cast<long long>(verify_us),
        unsigned(controller_link.grid()==expected),static_cast<unsigned long>(digest));
    if(controller_link.grid()!=expected) {running=false;controller_link.disconnect();remote_enabled=false;Serial.println("MISMATCH: stopped");}
}
#endif
} // namespace

void setup() {
    Serial.begin(115200);
    rx_queue=xQueueCreateStatic(rx_depth,sizeof(Frame),rx_storage,&rx_queue_storage);
    if(esp_read_mac(own.data(),ESP_MAC_WIFI_STA)!=ESP_OK) {own={}; attempted=true;}
    Serial.printf("cluster role=%s mac=%02x%02x%02x%02x%02x%02x channel=%u queue=%u; radio not started by application\n",
        CLUSTER_COORDINATOR?"coordinator":"worker",unsigned(own[0]),unsigned(own[1]),unsigned(own[2]),
        unsigned(own[3]),unsigned(own[4]),unsigned(own[5]),unsigned(channel),unsigned(rx_queue!=nullptr));
    Serial.println("pair <peer-mac12> <epoch8> <pmk32> <lmk32> (hex; no echo); status; coordinator: run stop local remote xy X Y");
#if CLUSTER_FIELD
    Serial.println(panel.begin()?"field panel ready (candidate); audio off":"field panel init failed");
    Serial.println("touch: attractor; audio on: arm GPIO26; mute/BOOT: disarm; local/remote: transport");
#endif
}
void loop() {
    serial_poll();
    for(unsigned i=0;i<4;++i) {
        Frame f; if(!rx_queue||xQueueReceive(rx_queue,&f,0)!=pdTRUE) break;
        if(!remote_enabled) continue;
#if CLUSTER_FIELD
#if CLUSTER_DUAL
        musical.receive(f,millis(),send_frame);
#else
        interactive.receive(f,millis());
#endif
#elif CLUSTER_COORDINATOR
        completed(controller_link.receive(f,millis()));
#else
        auto result=worker.receive(f);
        if(result.size) send_frame(result); // Busy response can be retried by coordinator.
#endif
    }
#if CLUSTER_FIELD
    const uint32_t now=millis();panel.input(interactive,now);interactive.tick(now,send_frame);
#if CLUSTER_DUAL
    musical.tick(millis(),send_frame);panel.output(interactive,millis(),true,musical.sound(millis()));
#else
    panel.output(interactive,millis());
#endif
#elif CLUSTER_COORDINATOR
    completed(controller_link.tick(millis(),send_frame));
    if(running&&!controller_link.pending()&&uint32_t(millis()-last_job)>=250) {
        last_job=millis();job.seed=next_seed++;job.particles=512;job.steps=64;
        active_job=job;
        started_us=esp_timer_get_time();
        if(!controller_link.start(active_job,last_job,60,send_frame)) {running=false;Serial.println("start failed");}
    }
#endif
    delay(1);
}
