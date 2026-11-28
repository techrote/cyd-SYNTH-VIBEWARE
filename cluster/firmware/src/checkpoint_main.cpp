// M18 optional checkpoint-assisted visual-only experiment. Trusted WPA2 LAN.
// No audio, no boot radio, no flash or persisted credentials.
#if CLUSTER_CHECKPOINT_TARGET
#include "../../reaction_view.hpp"
#include "../../checkpoint_paced.hpp"
#include "../../lan.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_timer.h>
#if CLUSTER_CHECKPOINT_PANEL
#include "field_panel.hpp"
#include <optional>
#endif
using namespace cluster;
namespace sf=cluster::spatial;
namespace cp=sf::checkpoint;
namespace {
constexpr uint16_t port=42003;
constexpr uint32_t checkpoint_interval_ticks=10;
WiFiUDP udp;
lan::Peer peer{};
bool joined=false,online=false,configured=false,enabled=false;
uint32_t tx=0,rx=0,rejected=0,tx_fail=0;
uint64_t work_us=0,net_us=0,export_us=0,render_us=0;
uint32_t work_max=0,net_max=0,export_max=0,render_max=0;
char line[197]{};std::size_t used=0;bool overflow=false;
lan::Address address(IPAddress ip){return {ip[0],ip[1],ip[2],ip[3]};}
void record(uint64_t& sum,uint32_t& maximum,int64_t before){
    const auto elapsed=uint32_t(esp_timer_get_time()-before);
    sum+=elapsed;maximum=std::max(maximum,elapsed);
}
#if CLUSTER_CHECKPOINT_PANEL
std::optional<sf::ReactionView> viewer;
std::array<uint8_t,cp::prefix+cp::payload> packet{};
field_panel::Screen screen;
SPIClass touch_bus(VSPI);
XPT2046_Touchscreen touch(33);
bool panel=false,contact=false;
uint32_t last_touch=0,last_frame=0,frames=0;
unsigned row_y=240;
sf::Pixels snapshot{};
std::array<uint16_t,320> rgb{};
std::array<lgfx::rgb565_t,320> line565{};
const char* frame_label="LOCAL";
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
    if(row_y==240){
        if(uint32_t(now-last_frame)<100)return;
        last_frame=now;row_y=0;
        viewer->present(now);snapshot=viewer->image();
        frame_label=viewer->frozen()?"CATCH-UP":(viewer->follower().remote()&&viewer->remote_allowed()?"REMOTE":"LOCAL");
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
        screen.setTextSize(1);screen.drawString(frame_label,3,227);++frames;
    }
    screen.endWrite();record(render_us,render_max,before);
}
#else
sf::ReactionField field;
cp::PacedSender paced;
uint32_t last_step=0,checkpoint_starts=0,checkpoint_done=0,checkpoint_aborted=0;
template<class P> bool send(const P& p){
    IPAddress ip(peer.address[0],peer.address[1],peer.address[2],peer.address[3]);
    const auto before=esp_timer_get_time();
    bool ok=false;
    if(udp.beginPacket(ip,port)){const auto n=udp.write(p.data(),p.size());ok=udp.endPacket()==1&&n==p.size();}
    if(ok)++tx;else ++tx_fail;
    record(net_us,net_max,before);return ok;
}
void cancel_checkpoint(){if(paced.active()){paced.cancel();++checkpoint_aborted;}}
#endif
void network_state(){
    const bool connected=joined&&WiFi.status()==WL_CONNECTED;
    if(!connected&&online){
        udp.stop();online=false;
#if !CLUSTER_CHECKPOINT_PANEL
        cancel_checkpoint();
#endif
    }
    if(connected&&!online){
        online=udp.begin(port)==1;
        if(online)Serial.printf("checkpoint LAN IP=%s port=%u\n",WiFi.localIP().toString().c_str(),unsigned(port));
    }
}
#if CLUSTER_CHECKPOINT_PANEL
void receive(){
    if(!online)return;
    const auto before=esp_timer_get_time();
    for(unsigned i=0;i<4;++i){
        const int n=udp.parsePacket();if(n<=0)break;
        const auto sender=address(udp.remoteIP());const auto p=udp.remotePort();
        if(n!=int(sf::bytes)&&n!=int(packet.size())){udp.flush();++rejected;continue;}
        const int got=udp.read(packet.data(),std::size_t(n));udp.flush();
        if(configured&&enabled&&!contact&&sender==peer.address&&p==port&&got==n&&
           viewer->receive(packet.data(),std::size_t(n),millis()))++rx;
        else ++rejected;
    }
    record(net_us,net_max,before);
}
#else
void worker_step(uint32_t now){
    bool snapshot_sent=false;
    if(field.busy()||uint32_t(now-last_step)>=50){
        const auto before=esp_timer_get_time();
        if(!field.busy()){last_step=now;if(!field.begin())return;}
        const bool complete=field.slice();
        record(work_us,work_max,before);
        if(complete&&online&&configured&&enabled){
            const auto p=sf::encode(peer.epoch,field);
            send(p);snapshot_sent=true;
            if(field.sequence()%checkpoint_interval_ticks==0){
                const auto t=esp_timer_get_time();
                if(paced.start(peer.epoch,field,millis()))++checkpoint_starts;
                record(export_us,export_max,t);
            }
        }
    }
    if(!online||!configured||!enabled){cancel_checkpoint();return;}
    if(snapshot_sent)return; // presentation traffic takes priority
    const auto result=paced.poll(millis(),[](const cp::Packet& p){return send(p);});
    if(result==cp::PacedSender::Result::complete)++checkpoint_done;
    if(result==cp::PacedSender::Result::failed||result==cp::PacedSender::Result::expired)++checkpoint_aborted;
}
#endif
void meter(){
    Serial.printf("CHECKPOINT_METER_V1 uptime_us=%llu work_us=%llu work_max_us=%u net_us=%llu net_max_us=%u export_us=%llu export_max_us=%u render_us=%llu render_max_us=%u tx=%u rx=%u rejected=%u tx_fail=%u heap=%u min_heap=%u largest=%u online=%u enabled=%u\n",
        (unsigned long long)esp_timer_get_time(),(unsigned long long)work_us,unsigned(work_max),
        (unsigned long long)net_us,unsigned(net_max),(unsigned long long)export_us,unsigned(export_max),
        (unsigned long long)render_us,unsigned(render_max),unsigned(tx),unsigned(rx),unsigned(rejected),unsigned(tx_fail),
        unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),unsigned(ESP.getMaxAllocHeap()),unsigned(online),unsigned(enabled));
#if CLUSTER_CHECKPOINT_PANEL
    const auto& f=viewer->follower();
    Serial.printf("CHECKPOINT_PANEL_V1 bytes=%u local_seq=%u remote_seq=%u saved=%u target=%u admitted=%u refused=%u caught_up=%u cold=%u slices=%u frames=%u contact=%u\n",
        unsigned(sizeof(sf::ReactionView)),unsigned(f.local().sequence()),unsigned(f.view().sequence()),unsigned(f.saved_sequence()),
        unsigned(f.catchup_target()),unsigned(f.admitted),unsigned(f.refused),unsigned(f.caught_up),unsigned(f.cold_losses),unsigned(f.slices),unsigned(frames),unsigned(contact));
#else
    Serial.printf("CHECKPOINT_WORKER_V1 bytes=%u seq=%u starts=%u complete=%u aborted=%u pending=%u\n",
        unsigned(sizeof(field)+sizeof(paced)),unsigned(field.sequence()),unsigned(checkpoint_starts),unsigned(checkpoint_done),unsigned(checkpoint_aborted),unsigned(paced.active()));
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
            peer=p;configured=true;
#if CLUSTER_CHECKPOINT_PANEL
            viewer.emplace(p.epoch); // explicit new session; no large temporary on task stack
            row_y=240;
#endif
            Serial.println("peer configured; remote enables stream");
        }else Serial.println("peer rejected; connect first, reboot to change peer");
        return;
    }
    if(!std::strcmp(s,"status")||!std::strcmp(s,"meter")){meter();return;}
    if(!std::strcmp(s,"local")){enabled=false;return;}
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
#if CLUSTER_CHECKPOINT_PANEL
    viewer.emplace(0);panel_begin();
#endif
    Serial.println("M18 checkpoint visual-only, radio off, trusted WPA2 LAN port 42003");
    Serial.println("join <ssidhex> <pskhex>; peer <IPv4> <epoch8>; remote/local/status/meter/reconnect");
}
void loop(){
    serial_poll();network_state();const auto now=millis();
#if CLUSTER_CHECKPOINT_PANEL
    panel_input(now);
    // Keep allow true across network outage, so the controller performs autonomous recovery.
    viewer->allow_remote(enabled&&!contact);
    receive();
    const auto before=esp_timer_get_time();viewer->step(millis());record(work_us,work_max,before);
    render(millis());
#else
    worker_step(now);
#endif
    delay(1);
}
#endif
