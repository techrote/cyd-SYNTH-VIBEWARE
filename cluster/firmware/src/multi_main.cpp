#ifdef CLUSTER_MULTI
// Serial-only mixed-device characterisation candidate. No LCD, speaker, touch,
// default credentials or boot-time radio; does not replace the field instrument.
#include "../../multi.hpp"
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
using namespace cluster;
namespace {
constexpr uint8_t channel=6;
constexpr unsigned rx_depth=8;
struct Packet { Frame frame; unsigned slot; };
StaticQueue_t queue_state;
alignas(Packet) uint8_t queue_storage[rx_depth*sizeof(Packet)];
QueueHandle_t rx=nullptr;
std::array<uint8_t,6> own{};
std::array<std::array<uint8_t,6>,2> peers{};
std::array<Pairing,2> staging{};
std::array<bool,2> staged{};
std::array<Frame,2> outgoing{};
std::atomic<uint32_t> busy{0}, rejected{0}, dropped{0}, tx_fail{0};
unsigned configured_nodes=0, round_robin=0;
bool attempted=false, configured=false, running=false;
Multi multi(0,1,2);
Cohorts current{};
unsigned seed=1;
uint32_t next_start=0, remote_before=0, local_before=0;
int64_t started_us=0;
char line[112]{};
std::size_t used=0;
bool overflow=false;

void on_receive(const uint8_t* mac,const uint8_t* data,int n) {
    if(!mac||!data||n<int(header)||n>int(max_frame)) {
        rejected.fetch_add(1,std::memory_order_relaxed); return;
    }
    unsigned slot=0;
    for(;slot<configured_nodes;++slot) if(!std::memcmp(mac,peers[slot].data(),6)) break;
    if(slot==configured_nodes) { rejected.fetch_add(1,std::memory_order_relaxed); return; }
    Packet p{}; p.slot=slot; p.frame.size=std::size_t(n);
    std::memcpy(p.frame.bytes.data(),data,p.frame.size);
    if(xQueueSend(rx,&p,0)!=pdTRUE) dropped.fetch_add(1,std::memory_order_relaxed);
}
void on_sent(const uint8_t*,esp_now_send_status_t status) {
    if(status!=ESP_NOW_SEND_SUCCESS) tx_fail.fetch_add(1,std::memory_order_relaxed);
    busy.store(0,std::memory_order_release);
}
// One bounded pending frame per peer; round-robin arbitration prevents a
// retrying peer from monopolising the single allowed in-flight radio send.
bool enqueue(unsigned slot,const Frame& f) {
    if(slot>=configured_nodes||!f.size||f.size>max_frame||outgoing[slot].size) return false;
    outgoing[slot]=f; return true;
}
void service_tx() {
    if(!configured_nodes||busy.load(std::memory_order_acquire)) return;
    for(unsigned i=0;i<configured_nodes;++i) {
        const unsigned slot=(round_robin+i)%configured_nodes;
        auto& f=outgoing[slot];
        if(!f.size) continue;
        uint32_t expected=0;
        if(!busy.compare_exchange_strong(expected,1,std::memory_order_acq_rel)) return;
        round_robin=(slot+1)%configured_nodes;
        if(esp_now_send(peers[slot].data(),f.bytes.data(),f.size)!=ESP_OK) {
            tx_fail.fetch_add(1,std::memory_order_relaxed);
            busy.store(0,std::memory_order_release);
        }
        f={}; return;
    }
}
bool configure(unsigned nodes) {
    if(attempted||nodes>2||!rx) return false;
    for(unsigned i=0;i<nodes;++i) if(!staged[i]) return false;
    if(nodes==2&&(staging[0].peer==staging[1].peer||staging[0].epoch==staging[1].epoch||
                  staging[0].pmk!=staging[1].pmk||staging[0].lmk==staging[1].lmk)) return false;
    attempted=true; configured_nodes=nodes;
    multi=Multi(nodes,nodes?staging[0].epoch:1,nodes==2?staging[1].epoch:2);
    for(unsigned i=0;i<nodes;++i) peers[i]=staging[i].peer;
    bool initialized=false,ok=true;
    if(nodes) {
        WiFi.persistent(false); WiFi.setAutoReconnect(false);
        ok=WiFi.mode(WIFI_STA);
        if(ok) { WiFi.disconnect(false,false); ok=esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)==ESP_OK; }
        if(ok) { initialized=esp_now_init()==ESP_OK; ok=initialized; }
        if(ok) ok=esp_now_set_pmk(staging[0].pmk.data())==ESP_OK;
        for(unsigned i=0;ok&&i<nodes;++i) {
            esp_now_peer_info_t info{};
            std::memcpy(info.peer_addr,staging[i].peer.data(),6);
            std::memcpy(info.lmk,staging[i].lmk.data(),16);
            info.channel=channel; info.ifidx=WIFI_IF_STA; info.encrypt=true;
            ok=esp_now_add_peer(&info)==ESP_OK; wipe(&info,sizeof(info));
        }
        if(ok) ok=esp_now_register_send_cb(on_sent)==ESP_OK;
        if(ok) ok=esp_now_register_recv_cb(on_receive)==ESP_OK;
    }
    wipe(staging.data(),sizeof(staging)); staged={};
    if(!ok) {
        if(initialized) esp_now_deinit();
        WiFi.mode(WIFI_OFF); return false;
    }
    configured=true; return true;
}
void status() {
    Serial.printf("configured=%u nodes=%u active=%u running=%u batches=%lu remote=%lu local=%lu heap=%u min_heap=%u tx_busy=%lu tx_fail=%lu rx_drop=%lu rx_reject=%lu\n",
        unsigned(configured),configured_nodes,unsigned(multi.active()),unsigned(running),
        static_cast<unsigned long>(multi.stats.batches),static_cast<unsigned long>(multi.stats.remote),static_cast<unsigned long>(multi.stats.local),
        unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),static_cast<unsigned long>(busy.load()),
        static_cast<unsigned long>(tx_fail.load()),static_cast<unsigned long>(dropped.load()),static_cast<unsigned long>(rejected.load()));
    for(unsigned i=0;i<configured_nodes;++i) {
        const auto& s=multi.link_stats(i);
        Serial.printf("slot=%u ready=%u remote=%lu local=%lu retry=%lu enqueue=%lu busy=%lu\n",i,unsigned(multi.ready(i)),
            static_cast<unsigned long>(s.remote),static_cast<unsigned long>(s.local),static_cast<unsigned long>(s.retries),
            static_cast<unsigned long>(s.sent),static_cast<unsigned long>(s.busy));
    }
}
void command(const char* text,std::size_t n) {
    if(n==93&&!std::memcmp(text,"pair",4)&&(text[4]=='0'||text[4]=='1')&&text[5]==' ') {
        const unsigned slot=unsigned(text[4]-'0');
        char normalized[93]{};
        std::memcpy(normalized,text,4); std::memcpy(normalized+4,text+5,88);
        Pairing p{};
        const bool ok=!attempted&&parse_pairing(normalized,92,p)&&p.peer!=own;
        if(ok) { wipe(&staging[slot],sizeof(Pairing)); staging[slot]=p; staged[slot]=true; }
        wipe(normalized,sizeof(normalized)); wipe(&p,sizeof(p));
        Serial.println(ok?"peer staged (no radio)":"peer rejected"); return;
    }
    if(!std::strcmp(text,"status")) { status(); return; }
    if(!std::strcmp(text,"stop")) { running=false; return; }
    if(n==5&&!std::memcmp(text,"run ",4)&&text[4]>='0'&&text[4]<='2') {
        const unsigned nodes=unsigned(text[4]-'0');
        if(running||multi.active()|| (configured&&nodes!=configured_nodes)||(!configured&&!configure(nodes))) {
            Serial.println("run rejected; check staging, reboot for changed profile/fresh epochs"); return;
        }
        seed=1; running=true; next_start=millis()+200u;
        Serial.println("60 matched batches; reference compute measured outside batch; stop ends new batches"); return;
    }
    Serial.println("unknown command");
}
void serial_poll() {
    for(unsigned i=0;i<64&&Serial.available()>0;++i) {
        const int c=Serial.read();
        if(c=='\n') {
            if(!overflow) { line[used]=0; command(line,used); }
            else Serial.println("line too long");
            wipe(line,sizeof(line)); used=0; overflow=false;
        } else if(c!='\r') {
            if(c<32||c>126||used>=sizeof(line)-1) overflow=true;
            else if(!overflow) line[used++]=char(c);
        }
    }
}
void completed() {
    const int64_t elapsed=esp_timer_get_time()-started_us;
    const int64_t before=esp_timer_get_time();
    const auto expected=local_cohorts(current);
    const int64_t reference=esp_timer_get_time()-before;
    const bool match=expected==multi.grid;
    uint32_t digest=2166136261u;
    for(auto x:multi.grid) { digest^=x; digest*=16777619u; }
    Serial.printf("batch seed=%u nodes=%u elapsed_us=%lld verify_local_us=%lld remote=%lu local=%lu match=%u grid=%08lx\n",
        seed,configured_nodes,static_cast<long long>(elapsed),static_cast<long long>(reference),
        static_cast<unsigned long>(multi.stats.remote-remote_before),static_cast<unsigned long>(multi.stats.local-local_before),
        unsigned(match),static_cast<unsigned long>(digest));
    if(!match||seed>=60) { running=false; status(); }
    ++seed;
    next_start=millis()+150u;
}
} // namespace
void setup() {
    Serial.begin(115200);
    rx=xQueueCreateStatic(rx_depth,sizeof(Packet),queue_storage,&queue_state);
    if(esp_read_mac(own.data(),ESP_MAC_WIFI_STA)!=ESP_OK) attempted=true;
    Serial.printf("cluster multi coordinator mac=%02x%02x%02x%02x%02x%02x channel=%u queue=%u; no boot radio\n",
        unsigned(own[0]),unsigned(own[1]),unsigned(own[2]),unsigned(own[3]),unsigned(own[4]),unsigned(own[5]),unsigned(channel),unsigned(rx!=nullptr));
    Serial.println("pair0/pair1 <peer-mac12> <epoch8> <pmk32> <lmk32>; run 0|1|2; stop; status");
}
void loop() {
    serial_poll();
    if(!configured) { delay(1); return; }
    const bool was_active=multi.active();
    for(unsigned i=0;i<4;++i) {
        Packet p{}; if(xQueueReceive(rx,&p,0)!=pdTRUE) break;
        multi.receive(p.slot,p.frame,millis());
    }
    multi.tick(millis(),enqueue); service_tx();
    if(was_active&&!multi.active()) completed();
    if(running&&!multi.active()&&int32_t(millis()-next_start)>=0) {
        current=cohorts(seed,uint16_t(seed*301u),uint16_t(seed*601u));
        remote_before=multi.stats.remote; local_before=multi.stats.local;
        started_us=esp_timer_get_time();
        if(!multi.begin(current,100)) { running=false; Serial.println("batch start failed"); }
    }
    delay(1);
}
#endif
