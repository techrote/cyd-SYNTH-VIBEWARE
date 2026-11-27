#ifdef CLUSTER_MULTI
// Serial-only mixed-device characterisation candidate. No LCD, speaker, touch,
// default credentials or boot-time radio; does not replace the field instrument.
#include "../../multi.hpp"
#include "../../pairing.hpp"
#if CLUSTER_MULTI_FIELD
#include "../../multi_field.hpp"
#include "field_panel.hpp"
#endif
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
bool attempted=false, configured=false;
#if CLUSTER_MULTI_FIELD
av::MultiField field;
field_panel::Panel panel;
uint32_t control_max_us=0;
#ifdef CLUSTER_FIELD_BENCH
bench::Capture capture;
bench::Histogram loop_hist,control_hist;
field_panel::Panel::Evidence baseline{};
bool bench_wait=false,bench_run=false,bench_audio=false;
int64_t bench_begin=0,batch_begin=0;
uint32_t seen_batches=0,heap_min=0;
uint32_t tx_before=0,drop_before=0,reject_before=0;
std::array<LinkStats,2> link_before{};
void print_hist(const char* name,const bench::Histogram& hist) {
    Serial.printf("bench hist name=%s counts=",name);
    for(unsigned i=0;i<bench::bins;++i)Serial.printf("%s%lu",i?",":"",static_cast<unsigned long>(hist.counts[i]));
    Serial.println();
}
void finish_bench(const char* reason) {
    const auto elapsed=esp_timer_get_time()-bench_begin;
    const auto end=panel.evidence();
    bench_wait=false;bench_run=false;panel.mute();field.gate(false);
    for(unsigned i=0;i<capture.count;++i) {
        const auto& s=capture.rows[i];
        Serial.printf("bench sample seed=%lu us=%lu grid=%08lx remote=%u\n",static_cast<unsigned long>(s.seed),static_cast<unsigned long>(s.us),static_cast<unsigned long>(s.hash),s.remote);
    }
    print_hist("control",control_hist);print_hist("loop",loop_hist);
    for(unsigned i=0;i<configured_nodes;++i) {
        const auto& s=field.link_stats(i);const auto& b=link_before[i];
        Serial.printf("bench peer slot=%u sent=%lu retry=%lu remote=%lu local=%lu busy=%lu\n",i,static_cast<unsigned long>(s.sent-b.sent),static_cast<unsigned long>(s.retries-b.retries),static_cast<unsigned long>(s.remote-b.remote),static_cast<unsigned long>(s.local-b.local),static_cast<unsigned long>(s.busy-b.busy));
    }
    Serial.printf("bench end reason=%s count=%u elapsed_us=%lld frames=%lu audio_blocks=%lu audio_errors=%lu heap_min_sample=%lu tx_fail=%lu rx_drop=%lu rx_reject=%lu\n",reason,capture.count,static_cast<long long>(elapsed),static_cast<unsigned long>(end.frames-baseline.frames),static_cast<unsigned long>(end.blocks-baseline.blocks),static_cast<unsigned long>(end.errors-baseline.errors),static_cast<unsigned long>(heap_min),static_cast<unsigned long>(tx_fail.load()-tx_before),static_cast<unsigned long>(dropped.load()-drop_before),static_cast<unsigned long>(rejected.load()-reject_before));
    panel.status(); // lifetime maxima, explicitly separate from window counts
}
void start_bench() {
    if(!field.replay_reset())return;
    capture={};loop_hist={};control_hist={};seen_batches=field.stats().batches;
    Serial.printf("bench begin schema=1 nodes=%u remote=%u audio=%u count=%u steps=64 particles=1024\n",configured_nodes,unsigned(configured_nodes&&field.remote_enabled()),unsigned(bench_audio),bench::batches);
    Serial.flush();
    baseline=panel.evidence();heap_min=ESP.getFreeHeap();tx_before=tx_fail.load();drop_before=dropped.load();reject_before=rejected.load();
    for(unsigned i=0;i<configured_nodes;++i)link_before[i]=field.link_stats(i);
    bench_begin=esp_timer_get_time();bench_run=true;bench_wait=false;
}
bool collect_batch() {
    if(field.stats().batches==seen_batches)return true;
    seen_batches=field.stats().batches;
    const auto& j=field.shown();
    return capture.record({j.seed,uint32_t(esp_timer_get_time()-batch_begin),bench::digest(field.grid()),field.remote_cohorts()});
}
#endif
#else
bool running=false;
Multi multi(0,1,2);
Cohorts current{};
unsigned seed=1;
uint32_t next_start=0, remote_before=0, local_before=0;
int64_t started_us=0;
#endif
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
    #if !CLUSTER_MULTI_FIELD
    multi=Multi(nodes,nodes?staging[0].epoch:1,nodes==2?staging[1].epoch:2);
#endif
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
    #if CLUSTER_MULTI_FIELD
    if(ok) field.configure(nodes,nodes?staging[0].epoch:1,nodes==2?staging[1].epoch:2);
#endif
    wipe(staging.data(),sizeof(staging)); staged={};
    if(!ok) {
        if(initialized) esp_now_deinit();
        WiFi.mode(WIFI_OFF); return false;
    }
    configured=true; return true;
}
void status() {
#if CLUSTER_MULTI_FIELD
    const auto& stats=field.stats();
    Serial.printf("configured=%u nodes=%u pending=%u remote_enabled=%u batches=%lu remote=%lu local=%lu heap=%u min_heap=%u control_max_us=%lu tx_fail=%lu rx_drop=%lu rx_reject=%lu\n",unsigned(configured),configured_nodes,unsigned(field.pending()),unsigned(field.remote_enabled()),static_cast<unsigned long>(stats.batches),static_cast<unsigned long>(stats.remote),static_cast<unsigned long>(stats.local),unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),static_cast<unsigned long>(control_max_us),static_cast<unsigned long>(tx_fail.load()),static_cast<unsigned long>(dropped.load()),static_cast<unsigned long>(rejected.load()));
    panel.status();
#else
    Serial.printf("configured=%u nodes=%u active=%u running=%u batches=%lu remote=%lu local=%lu heap=%u min_heap=%u tx_busy=%lu tx_fail=%lu rx_drop=%lu rx_reject=%lu\n",
        unsigned(configured),configured_nodes,unsigned(multi.active()),unsigned(running),
        static_cast<unsigned long>(multi.stats.batches),static_cast<unsigned long>(multi.stats.remote),static_cast<unsigned long>(multi.stats.local),
        unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),static_cast<unsigned long>(busy.load()),
        static_cast<unsigned long>(tx_fail.load()),static_cast<unsigned long>(dropped.load()),static_cast<unsigned long>(rejected.load()));
#endif
    for(unsigned i=0;i<configured_nodes;++i) {
#if CLUSTER_MULTI_FIELD
        const auto& s=field.link_stats(i);
        const bool ready=field.ready(i);
#else
        const auto& s=multi.link_stats(i);
        const bool ready=multi.ready(i);
#endif
        Serial.printf("slot=%u ready=%u remote=%lu local=%lu retry=%lu enqueue=%lu busy=%lu\n",i,unsigned(ready),
            static_cast<unsigned long>(s.remote),static_cast<unsigned long>(s.local),static_cast<unsigned long>(s.retries),
            static_cast<unsigned long>(s.sent),static_cast<unsigned long>(s.busy));
    }
}
void command(const char* text,std::size_t n) {
#ifdef CLUSTER_FIELD_BENCH
    if(bench_run||bench_wait) {
        if(!std::strcmp(text,"bench stop")||!std::strcmp(text,"mute")) {
            if(bench_run)finish_bench("stopped");else {bench_wait=false;panel.mute();}
        }
        return;
    }
    if(!std::strcmp(text,"bench visual")||!std::strcmp(text,"bench audio")) {
        bench_audio=!std::strcmp(text,"bench audio");
        if(!panel.display_ready()||(bench_audio&&!panel.armed())||(!bench_audio&&panel.audio_started())) {
            Serial.println("bench rejected: panel/audio not ready (visual requires no prior audio arm)");return;
        }
        if(!bench_audio)panel.mute();
        bench_wait=true;return;
    }
#endif
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
#if CLUSTER_MULTI_FIELD
    if(!std::strcmp(text,"audio on")) { Serial.println(panel.arm()?"audio armed":"audio arm failed"); return; }
    if(!std::strcmp(text,"mute")) { panel.mute(); return; }
    if(!std::strcmp(text,"local")) { field.remote(false); outgoing={}; return; }
    if(!std::strcmp(text,"remote")) { field.remote(configured&&configured_nodes>0); return; }
    if(n==8&&!std::memcmp(text,"enable ",7)&&(text[7]=='1'||text[7]=='2')) {
        const unsigned nodes=unsigned(text[7]-'0');
        Serial.println(configure(nodes)?"peers configured":"enable rejected; check pairing or reboot");
        return;
    }
#else
    if(!std::strcmp(text,"stop")) { running=false; return; }
    if(n==5&&!std::memcmp(text,"run ",4)&&text[4]>='0'&&text[4]<='2') {
        const unsigned nodes=unsigned(text[4]-'0');
        if(running||multi.active()|| (configured&&nodes!=configured_nodes)||(!configured&&!configure(nodes))) {
            Serial.println("run rejected; check staging, reboot for changed profile/fresh epochs"); return;
        }
        seed=1; running=true; next_start=millis()+200u;
        Serial.println("60 matched batches; reference compute measured outside batch; stop ends new batches"); return;
    }
    #endif
    Serial.println("unknown command");
}
void serial_poll() {
    for(unsigned i=0;i<64&&Serial.available()>0;++i) {
        const int c=Serial.read();
        if(c=='\n') {
            if(!overflow) { line[used]=0; command(line,used); }
            else {
#ifdef CLUSTER_FIELD_BENCH
                if(!bench_run&&!bench_wait)
#endif
                Serial.println("line too long");
            }
            wipe(line,sizeof(line)); used=0; overflow=false;
        } else if(c!='\r') {
            if(c<32||c>126||used>=sizeof(line)-1) overflow=true;
            else if(!overflow) line[used++]=char(c);
        }
    }
}
#if !CLUSTER_MULTI_FIELD
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
 #endif
} // namespace
void setup() {
    Serial.begin(115200);
    rx=xQueueCreateStatic(rx_depth,sizeof(Packet),queue_storage,&queue_state);
    if(esp_read_mac(own.data(),ESP_MAC_WIFI_STA)!=ESP_OK) attempted=true;
    Serial.printf("cluster multi coordinator mac=%02x%02x%02x%02x%02x%02x channel=%u queue=%u; no boot radio\n",
        unsigned(own[0]),unsigned(own[1]),unsigned(own[2]),unsigned(own[3]),unsigned(own[4]),unsigned(own[5]),unsigned(channel),unsigned(rx!=nullptr));
#if CLUSTER_MULTI_FIELD
    Serial.println(panel.begin()?"multi field ready; audio off":"panel init failed");
    Serial.println("pair0/pair1 <peer-mac12> <epoch8> <pmk32> <lmk32>; enable 1|2; local; remote; audio on; mute; status");
#else
    Serial.println("pair0/pair1 <peer-mac12> <epoch8> <pmk32> <lmk32>; run 0|1|2; stop; status");
#endif
}
void loop() {
    serial_poll();
#if CLUSTER_MULTI_FIELD
    const uint32_t now=millis();
#ifdef CLUSTER_FIELD_BENCH
    auto loop_start=esp_timer_get_time();
#endif
    panel.input(field,now);
#ifdef CLUSTER_FIELD_BENCH
    if(bench_wait&&!field.pending()){start_bench();loop_start=esp_timer_get_time();}
    if(bench_run&&(digitalRead(0)==LOW||(bench_audio&&!panel.armed())))finish_bench("panic");
    if(bench_run)panel.scripted(field,bench::gesture(field.pending()?field.next_seed()-1u:field.next_seed()),bench_audio);
#endif
    const int64_t before=esp_timer_get_time();
    for(unsigned i=0;i<4;++i) {
        Packet p{}; if(!rx||xQueueReceive(rx,&p,0)!=pdTRUE) break;
        field.receive(p.slot,p.frame,millis());
    }
#ifdef CLUSTER_FIELD_BENCH
    if(bench_run&&!collect_batch())finish_bench("sequence_error");
    if(bench_run&&capture.count==bench::batches)finish_bench("complete");
    if(bench_run&&!field.pending())panel.scripted(field,bench::gesture(field.next_seed()),bench_audio);
    const auto previous_seed=field.next_seed();
    const auto tick_start=esp_timer_get_time();
    field.tick(millis(),enqueue,!bench_wait);
    if(bench_run&&field.next_seed()!=previous_seed)batch_begin=tick_start;
    if(bench_run&&!collect_batch())finish_bench("sequence_error");
#else
    field.tick(millis(),enqueue);
#endif
    if(configured&&field.remote_enabled()) service_tx();
    control_max_us=std::max(control_max_us,uint32_t(esp_timer_get_time()-before));
#ifdef CLUSTER_FIELD_BENCH
    if(bench_run)control_hist.add(uint32_t(esp_timer_get_time()-before));
#endif
    panel.output(field,millis(),false,0,false,false,field.mode());
#ifdef CLUSTER_FIELD_BENCH
    if(bench_run) {
        loop_hist.add(uint32_t(esp_timer_get_time()-loop_start));
        heap_min=std::min(heap_min,uint32_t(ESP.getFreeHeap()));
        if(capture.count==bench::batches)finish_bench("complete");
        else if(esp_timer_get_time()-bench_begin>45000000)finish_bench("timeout");
    }
#endif
#else
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
    #endif
    delay(1);
}
#endif
