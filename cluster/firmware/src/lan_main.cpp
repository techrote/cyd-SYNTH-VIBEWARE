// Dedicated serial-only LAN comparison. No audio/display, no default radio.
// IP/port filtering is not cryptographic peer authentication: isolated trusted
// WPA2 LAN only, no Internet or forwarded ports. ESP-NOW remains preferred.
#if CLUSTER_LAN
#include "../../lan.hpp"
#include "../../link.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_timer.h>
#include <cstdio>
using namespace cluster;
namespace {
WiFiUDP udp;
lan::Peer peer{};
Link link_core(1);
Worker worker(1);
bool joined=false, online=false, configured=false, enabled=false,running=false;
uint32_t last_job=0,next_seed=1,tx=0,rx=0,tx_fail=0,reject=0,tx_bytes=0,rx_bytes=0;
int64_t started_us=0;
Job active{};
char line[197]{};std::size_t used=0;bool overflow=false;
lan::Address address(IPAddress ip){return {ip[0],ip[1],ip[2],ip[3]};}
bool send_frame(const Frame& f){
    if(!online||!configured||!enabled||!f.size||f.size>max_frame)return false;
    IPAddress ip(peer.address[0],peer.address[1],peer.address[2],peer.address[3]);
    if(!udp.beginPacket(ip,lan::port)){++tx_fail;return false;}
    const auto n=udp.write(f.bytes.data(),f.size);
    const bool ok=udp.endPacket()==1&&n==f.size;
    if(ok){++tx;tx_bytes+=uint32_t(n);}else ++tx_fail;
    return ok;
}
#if CLUSTER_COORDINATOR
void completed(Completion c){
    if(c==Completion::none)return;
    const auto elapsed=esp_timer_get_time()-started_us;
    const auto before=esp_timer_get_time();
    const auto expected=compute(active);
    const auto verify=esp_timer_get_time()-before;
    const bool match=expected==link_core.grid();
    uint32_t digest=2166136261u;
    for(auto x:link_core.grid()){digest^=x;digest*=16777619u;}
    Serial.printf("job seed=%lu mode=%s elapsed_us=%lld verify_local_us=%lld match=%u grid=%08lx\n",
        static_cast<unsigned long>(active.seed),c==Completion::remote?"remote":"local",
        static_cast<long long>(elapsed),static_cast<long long>(verify),unsigned(match),static_cast<unsigned long>(digest));
    if(!match){running=false;enabled=false;link_core.disconnect();Serial.println("MISMATCH: stopped");}
}
#endif
void network_tick(){
    const bool connected=joined&&WiFi.status()==WL_CONNECTED;
    if(!connected&&online){udp.stop();online=false;link_core.disconnect();Serial.println("LAN disconnected; local fallback");}
    if(connected&&!online){
        online=udp.begin(lan::port)==1;
        if(online)Serial.printf("LAN ready IP=%s port=%u\n",WiFi.localIP().toString().c_str(),unsigned(lan::port));
    }
    if(!online)return;
    // Bound datagrams serviced per loop. WiFiUDP owns its receive allocation;
    // this diagnostic does not claim real-time or allocation-free radio I/O.
    for(unsigned i=0;i<4;++i){
        const int n=udp.parsePacket();if(n<=0)break;
        const auto sender=address(udp.remoteIP());const auto port=udp.remotePort();
        if(n<int(header)||n>int(max_frame)){udp.flush();++reject;continue;}
        Frame raw{},f{};const int got=udp.read(raw.bytes.data(),std::size_t(n));udp.flush();
        if(!configured||!enabled||got!=n||!lan::admit(sender,port,peer,raw.bytes.data(),std::size_t(n),f)){
            ++reject;continue;
        }
        ++rx;rx_bytes+=uint32_t(n);
#if CLUSTER_COORDINATOR
        completed(link_core.receive(f,millis()));
#else
        const auto response=worker.receive(f);if(response.size)send_frame(response);
#endif
    }
}
void status(){
    Serial.printf("LAN connected=%u socket=%u peer=%u enabled=%u tx=%lu rx=%lu tx_bytes=%lu rx_bytes=%lu tx_fail=%lu rejected=%lu heap=%u min_heap=%u\n",
        unsigned(joined&&WiFi.status()==WL_CONNECTED),unsigned(online),unsigned(configured),unsigned(enabled),
        static_cast<unsigned long>(tx),static_cast<unsigned long>(rx),static_cast<unsigned long>(tx_bytes),static_cast<unsigned long>(rx_bytes),
        static_cast<unsigned long>(tx_fail),static_cast<unsigned long>(reject),unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()));
#if CLUSTER_COORDINATOR
    Serial.printf("ready=%u pending=%u remote=%lu local=%lu retries=%lu\n",unsigned(link_core.ready()),unsigned(link_core.pending()),
        static_cast<unsigned long>(link_core.stats.remote),static_cast<unsigned long>(link_core.stats.local),static_cast<unsigned long>(link_core.stats.retries));
#endif
}
void command(const char* s,std::size_t n){
    if(n>=5&&std::memcmp(s,"join ",5)==0){
        lan::Credentials c{};
        if(!joined&&lan::parse_join(s,n,c)){
            WiFi.persistent(false);WiFi.setAutoReconnect(false);
            if(WiFi.mode(WIFI_STA)){WiFi.begin(c.ssid.data(),c.pass.data());joined=true;Serial.println("LAN association requested");}
            else Serial.println("LAN mode failed");
        }else Serial.println("join rejected; reboot to change credentials");
        wipe(&c,sizeof(c));return;
    }
    if(n>=5&&std::memcmp(s,"peer ",5)==0){
        lan::Peer p{};
        if(online&&!configured&&!running&&!link_core.pending()&&lan::parse_peer(s,n,p)&&p.address!=address(WiFi.localIP())){
            peer=p;link_core=Link(p.epoch);worker=Worker(p.epoch);configured=true;Serial.println("peer configured; use remote to enable");
        }else Serial.println("peer rejected; connect first, stop local run, reboot to change peer");
        return;
    }
    if(!std::strcmp(s,"status")){status();return;}
    if(!std::strcmp(s,"local")){enabled=false;link_core.disconnect();return;}
    if(!std::strcmp(s,"remote")){enabled=configured;return;}
    if(!std::strcmp(s,"reconnect")&&joined&&!online){WiFi.reconnect();return;}
#if CLUSTER_COORDINATOR
    if(!std::strcmp(s,"run")&&!running&&!link_core.pending()){next_seed=1;running=true;last_job=millis()-250u;return;}
    if(!std::strcmp(s,"stop")){running=false;return;}
    unsigned x=0,y=0;char extra=0;
    if(std::sscanf(s,"xy %u %u %c",&x,&y,&extra)==2&&x<=65535&&y<=65535&&!link_core.pending()){active.x=uint16_t(x);active.y=uint16_t(y);return;}
#endif
    Serial.println("unknown or busy command");
}
void serial_poll(){
    for(unsigned i=0;i<64&&Serial.available()>0;++i){
        const int c=Serial.read();
        if(c=='\n'){
            if(!overflow){line[used]=0;command(line,used);}else Serial.println("line too long");
            wipe(line,sizeof(line));used=0;overflow=false;
        }else if(c!='\r'){
            if(c<32||c>126||used>=sizeof(line)-1)overflow=true;
            else if(!overflow)line[used++]=char(c);
        }
    }
}
}
void setup(){
    Serial.begin(115200);
    WiFi.persistent(false);WiFi.mode(WIFI_OFF);
    Serial.println("cluster LAN diagnostic; radio off; trusted isolated WPA2 LAN only");
    Serial.println("join <ssidhex> <pskhex>; peer <IPv4> <epoch8>; remote/local/status/reconnect; coordinator run/stop/xy X Y");
}
void loop(){
    serial_poll();network_tick();
#if CLUSTER_COORDINATOR
    completed(link_core.tick(millis(),send_frame,enabled&&online));
    if(running&&!link_core.pending()&&uint32_t(millis()-last_job)>=250){
        last_job=millis();active.seed=next_seed++;active.particles=512;active.steps=64;
        started_us=esp_timer_get_time();
        if(!link_core.start(active,last_job,60,send_frame)){running=false;Serial.println("start failed");}
    }
#endif
    delay(1);
}
#endif
