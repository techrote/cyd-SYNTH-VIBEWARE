// Independent visual-only snapshot stream, trusted isolated WPA2 LAN.
// No audio, no boot radio, no remote rendering commands, no credential persistence.
#if CLUSTER_SPATIAL
#include "../../spatial.hpp"
#include "../../lan.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_timer.h>
#if CLUSTER_SPATIAL_PANEL
#include "field_panel.hpp"
#endif
using namespace cluster;
namespace sf=cluster::spatial;
namespace {
constexpr uint16_t port=42002;
WiFiUDP udp;
lan::Peer peer{};
sf::Receiver rx(0);
sf::Field local;
bool joined=false,online=false,configured=false,enabled=false;
uint32_t last_step=0,tx=0,accepted=0,rejected=0,tx_fail=0,compute_max=0,io_max=0;
uint32_t remote_ticks=0,local_ticks=0;
char line[197]{};std::size_t used=0;bool overflow=false;
lan::Address address(IPAddress ip){return {ip[0],ip[1],ip[2],ip[3]};}
#if CLUSTER_SPATIAL_PANEL
field_panel::Screen screen;
SPIClass touch_bus(VSPI);
XPT2046_Touchscreen touch(33);
bool panel=false,contact=false,frame_remote=false;
uint32_t last_touch=0,last_frame=0,frames=0,render_max=0;
unsigned row_y=240;
sf::Pixels snapshot{};
std::array<uint16_t,320> rgb{};
std::array<lgfx::rgb565_t,320> line565{};
void panel_begin(){
    pinMode(21,OUTPUT);digitalWrite(21,LOW);
    panel=screen.init();
    if(panel){screen.setRotation(5);panel=screen.width()==320&&screen.height()==240;}
    if(panel){screen.fillScreen(uint16_t(0));digitalWrite(21,HIGH);
        touch_bus.begin(25,39,32,33);touch.begin(touch_bus);touch.setRotation(0);}
}
void panel_input(uint32_t now){
    if(panel&&uint32_t(now-last_touch)>=12){last_touch=now;contact=touch.getPoint().z>=400;}
}
void render(uint32_t now){
    if(!panel)return;
    const bool remote=enabled&&online&&!contact&&rx.fresh(now);
    // Drop unfinished remote frame when its lease expires or local touch overrides.
    if(row_y<240&&frame_remote&&!remote)row_y=240;
    if(row_y==240){
        if(uint32_t(now-last_frame)<100)return;
        last_frame=now;row_y=0;frame_remote=remote;
        snapshot=remote?rx.pixels():local.pixels();
        if(remote)++remote_ticks;else ++local_ticks;
    }
    const auto before=esp_timer_get_time();
    screen.startWrite();
    for(unsigned i=0;i<8&&row_y<240;++i,++row_y){
        sf::row(snapshot,row_y,rgb.data());
        for(unsigned x=0;x<320;++x)line565[x]=rgb[x];
        screen.pushImage(0,int(row_y),320,1,line565.data());
    }
    if(row_y==240){
        screen.fillRect(0,224,320,16,uint16_t(0));screen.setTextColor(uint16_t(0xffff));
        screen.setTextSize(1);screen.drawString(frame_remote?"REMOTE FIELD":"LOCAL FIELD",3,227);++frames;
    }
    screen.endWrite();
    render_max=std::max(render_max,uint32_t(esp_timer_get_time()-before));
}
#endif
void network_tick(){
    const bool connected=joined&&WiFi.status()==WL_CONNECTED;
    if(!connected&&online){udp.stop();online=false;rx.disconnect();}
    if(connected&&!online){online=udp.begin(port)==1;
        if(online)Serial.printf("LAN ready IP=%s port=%u\n",WiFi.localIP().toString().c_str(),unsigned(port));}
    if(!online)return;
    const auto before=esp_timer_get_time();
    for(unsigned i=0;i<4;++i){
        const int n=udp.parsePacket();if(n<=0)break;
        const auto sender=address(udp.remoteIP());const auto p=udp.remotePort();
        if(n!=int(sf::bytes)){udp.flush();++rejected;continue;}
        sf::Packet packet{};const int got=udp.read(packet.data(),packet.size());udp.flush();
#if CLUSTER_SPATIAL_PANEL
        if(configured&&enabled&&sender==peer.address&&p==port&&got==n&&rx.accept(packet.data(),packet.size(),millis()))++accepted;
        else ++rejected;
#else
        (void)sender;(void)p;(void)got;++rejected;
#endif
    }
    const uint32_t elapsed=uint32_t(esp_timer_get_time()-before);
    if(elapsed>io_max)io_max=elapsed;
}
void tick_field(uint32_t now){
    if(uint32_t(now-last_step)<50)return;
    last_step=now;
#if CLUSTER_SPATIAL_PANEL
    if(enabled&&online&&!contact&&rx.fresh(now))return; // actually defer local compute
#endif
    const auto before=esp_timer_get_time();
    if(!local.advance()){enabled=false;return;}
    const uint32_t elapsed=uint32_t(esp_timer_get_time()-before);
    if(elapsed>compute_max)compute_max=elapsed;
#if !CLUSTER_SPATIAL_PANEL
    if(online&&configured&&enabled){
        const auto p=sf::encode(peer.epoch,local);
        IPAddress ip(peer.address[0],peer.address[1],peer.address[2],peer.address[3]);
        const auto t=esp_timer_get_time();
        bool ok=false;
        if(udp.beginPacket(ip,port)){const auto n=udp.write(p.data(),p.size());ok=udp.endPacket()==1&&n==p.size();}
        if(ok)++tx;else ++tx_fail;
        const uint32_t io=uint32_t(esp_timer_get_time()-t);if(io>io_max)io_max=io;
    }
#endif
}
void status(){
    Serial.printf("spatial online=%u enabled=%u local_seq=%lu remote_seq=%lu tx=%lu rx=%lu rejected=%lu tx_fail=%lu compute_max_us=%lu io_max_us=%lu heap=%u min_heap=%u\n",
        unsigned(online),unsigned(enabled),static_cast<unsigned long>(local.sequence()),static_cast<unsigned long>(rx.sequence()),
        static_cast<unsigned long>(tx),static_cast<unsigned long>(accepted),static_cast<unsigned long>(rejected),
        static_cast<unsigned long>(tx_fail),static_cast<unsigned long>(compute_max),static_cast<unsigned long>(io_max),
        unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()));
#if CLUSTER_SPATIAL_PANEL
    Serial.printf("panel=%u frames=%lu remote_frames_started=%lu local_frames_started=%lu render_max_us=%lu\n",unsigned(panel),
        static_cast<unsigned long>(frames),static_cast<unsigned long>(remote_ticks),static_cast<unsigned long>(local_ticks),static_cast<unsigned long>(render_max));
#endif
}
void command(const char* s,std::size_t n){
    if(n>=5&&std::memcmp(s,"join ",5)==0){
        lan::Credentials c{};
        if(!joined&&lan::parse_join(s,n,c)){
            WiFi.persistent(false);WiFi.setAutoReconnect(false);
            if(WiFi.mode(WIFI_STA)){WiFi.begin(c.ssid.data(),c.pass.data());joined=true;Serial.println("LAN association requested");}
        }else Serial.println("join rejected; reboot to change credentials");
        wipe(&c,sizeof(c));return;
    }
    if(n>=5&&std::memcmp(s,"peer ",5)==0){
        lan::Peer p{};
        if(online&&!configured&&lan::parse_peer(s,n,p)&&p.address!=address(WiFi.localIP())){
            peer=p;rx=sf::Receiver(p.epoch);configured=true;Serial.println("peer configured; remote enables stream");
        }else Serial.println("peer rejected; connect first, reboot to change peer");
        return;
    }
    if(!std::strcmp(s,"status")){status();return;}
    if(!std::strcmp(s,"local")){enabled=false;rx.disconnect();return;}
    if(!std::strcmp(s,"remote")){enabled=configured;return;}
    if(!std::strcmp(s,"reconnect")&&joined&&!online){WiFi.reconnect();return;}
    Serial.println("unknown command");
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
    Serial.begin(115200);WiFi.persistent(false);WiFi.mode(WIFI_OFF);
#if CLUSTER_SPATIAL_PANEL
    panel_begin();
#endif
    Serial.println("Spatial visual-only; radio off; isolated trusted WPA2 LAN, port 42002");
    Serial.println("join <ssidhex> <pskhex>; peer <IPv4> <epoch8>; remote/local/status/reconnect");
}
void loop(){
    serial_poll();network_tick();const auto now=millis();
#if CLUSTER_SPATIAL_PANEL
    panel_input(now);
#endif
    tick_field(now);
#if CLUSTER_SPATIAL_PANEL
    render(now);
#endif
    delay(1);
}
#endif
