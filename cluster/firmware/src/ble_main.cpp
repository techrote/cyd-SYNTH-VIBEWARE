// Opt-in GATT experiment, no Wi-Fi coexistence or directed advertising.
// Public-address filter is NOT authentication. Isolated lab only; no remote audio gate.
#if CLUSTER_BLE
#include "../../ble_control.hpp"
#include "../../pairing.hpp"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <esp_timer.h>
#include <atomic>
#include <cstdio>
#include <cstring>
using namespace cluster;
#if CLUSTER_BLE_PANEL
#include "field_panel.hpp"
#endif
namespace {
char line[48]{};std::size_t used=0;bool overflow=false,initialized=false;
bool mac(const char* s,std::array<uint8_t,6>& out){
    return std::strlen(s)==12&&hex_bytes(s,out)&&nonzero(out);
}
}
#if CLUSTER_BLE_PANEL
namespace {
field_panel::Panel panel;
av::Field field;
ble::Modulator mod;
struct Input {
    ble::XY local{};
    void xy(uint16_t x,uint16_t y){local={x,y};}
    void gate(bool on){field.gate(on);}
} input;
StaticQueue_t queue_state;
alignas(ble::Arrival) uint8_t storage[sizeof(ble::Arrival)];
QueueHandle_t queue=nullptr;
std::array<uint8_t,6> allowed{};
uint32_t epoch=0;
std::atomic<uint32_t> generation{0},applied{0},rejected{0};
std::atomic<bool> connected{false},enabled{false},blocked{false};
class ServerEvents:public BLEServerCallbacks{
    void onConnect(BLEServer*,esp_ble_gatts_cb_param_t* p)override{
        if(!p||std::memcmp(p->connect.remote_bda,allowed.data(),6)||connected.load()){
            blocked.store(true);connected.store(false);
        }else if(!blocked.load())connected.store(true);
        generation.fetch_add(1);
    }
    void onDisconnect(BLEServer*,esp_ble_gatts_cb_param_t*)override{
        connected.store(false);generation.fetch_add(1);
    }
} server_events;
class Writes:public BLECharacteristicCallbacks{
    void onWrite(BLECharacteristic* ch)override{
        const auto gen=generation.load();
        if(!connected.load()||!enabled.load()||blocked.load()){rejected.fetch_add(1);return;}
        // Arduino BLE owns its attribute storage; no claim that stack allocation is bounded.
        const auto raw=ch->getValue();ble::Control c{};
        if(!ble::decode(reinterpret_cast<const uint8_t*>(raw.data()),raw.size(),c)||c.epoch!=epoch){
            rejected.fetch_add(1);return;
        }
        const ble::Arrival a{c,millis(),gen};
        xQueueOverwrite(queue,&a); // freshest wins, no callback-side compute or logging
    }
} writes;
class Reads:public BLECharacteristicCallbacks{
    void onRead(BLECharacteristic* ch)override{
        auto b=ble::ack(connected.load()&&!blocked.load()?epoch:0,applied.load());
        ch->setValue(b.data(),b.size());
    }
} reads;
bool send_nothing(const Frame&){return false;}
void command(const char* s){
    if(!initialized&&std::strlen(s)==25&&!std::memcmp(s,"ble ",4)&&s[12]==' '){
        std::array<uint8_t,4> bytes{};std::array<uint8_t,6> peer{};
        if(!hex_bytes(s+4,bytes)||!mac(s+13,peer)){Serial.println("invalid epoch/address");return;}
        epoch=0;for(auto v:bytes)epoch=(epoch<<8)|v;
        if(!epoch){Serial.println("zero epoch");return;}
        if(!queue){Serial.println("queue unavailable");return;}
        allowed=peer;mod=ble::Modulator(epoch);panel.mute();
        // Generic peripheral initialization: no directed-peer startup, no BLE on boot.
        initialized=true;BLEDevice::init("CYD-VIBEWARE-MOD");
        auto* server=BLEDevice::createServer();server->setCallbacks(&server_events);
        auto* service=server->createService(ble::service);
        service->createCharacteristic(ble::control,BLECharacteristic::PROPERTY_WRITE)->setCallbacks(&writes);
        service->createCharacteristic(ble::acknowledgement,BLECharacteristic::PROPERTY_READ)->setCallbacks(&reads);
        service->start();auto* adv=BLEDevice::getAdvertising();adv->addServiceUUID(ble::service);
        adv->start();
        Serial.printf("BLE advertising address=%s; local mode; use remote to admit controls\n",BLEDevice::getAddress().toString().c_str());return;
    }
    if(!std::strcmp(s,"advertise")&&initialized&&!connected.load()&&!blocked.load()){
        BLEDevice::getAdvertising()->start();return;
    }
    if(!std::strcmp(s,"remote")&&initialized&&!blocked.load()){
        generation.fetch_add(1);enabled.store(true);return;
    }
    if(!std::strcmp(s,"local")){enabled.store(false);generation.fetch_add(1);return;}
    if(!std::strcmp(s,"audio on")){Serial.println(panel.arm()?"armed; local touch only":"arm failed");return;}
    if(!std::strcmp(s,"mute")){panel.mute();return;}
    if(!std::strcmp(s,"status")){
        Serial.printf("BLE connected=%u enabled=%u blocked=%u applied=%lu rejected=%lu heap=%u min_heap=%u\n",
            unsigned(connected.load()),unsigned(enabled.load()),unsigned(blocked.load()),static_cast<unsigned long>(applied.load()),
            static_cast<unsigned long>(rejected.load()),unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()));
        panel.status();return;
    }
    Serial.println("unknown/busy; reboot after unauthorized connection");
}
}
#else
namespace {
BLEClient* client=nullptr;
BLERemoteCharacteristic* wr=nullptr;BLERemoteCharacteristic* rd=nullptr;
uint32_t epoch=0,sequence=0,attempts=0,matched=0;
bool running=false,pending=false;
int64_t sent=0;uint32_t next_job=0;
bool read_remote(uint32_t& e,uint32_t& seq){
    if(!client||!client->isConnected()||!rd)return false;
    const auto v=rd->readValue();
    return ble::read_ack(reinterpret_cast<const uint8_t*>(v.data()),v.size(),e,seq);
}
void command(const char* s){
    if(!std::strcmp(s,"ble")&&!initialized){
        initialized=true;BLEDevice::init("VIBEWARE-MOD-CLIENT");client=BLEDevice::createClient();
        Serial.printf("BLE client public address=%s\n",BLEDevice::getAddress().toString().c_str());return;
    }
    if(initialized&&!client->isConnected()&&std::strlen(s)==20&&!std::memcmp(s,"connect ",8)){
        std::array<uint8_t,6> addr{};if(!mac(s+8,addr)){Serial.println("bad address");return;}
        wr=nullptr;rd=nullptr;running=pending=false;
        // Serial diagnostic only: Arduino BLE discovery and operations may block.
        if(!client->connect(BLEAddress(addr.data()),BLE_ADDR_TYPE_PUBLIC)){Serial.println("connect failed");return;}
        auto* service=client->getService(ble::service);
        if(service){wr=service->getCharacteristic(ble::control);rd=service->getCharacteristic(ble::acknowledgement);}
        if(!wr||!rd||!read_remote(epoch,sequence)){client->disconnect();wr=rd=nullptr;Serial.println("service/epoch unavailable");return;}
        Serial.printf("BLE connected epoch=%08lx seq=%lu mtu=%u\n",static_cast<unsigned long>(epoch),static_cast<unsigned long>(sequence),unsigned(client->getMTU()));return;
    }
    if(!std::strcmp(s,"run")&&client&&client->isConnected()&&wr&&rd&&!running){
        uint32_t e=0,q=0;if(!read_remote(e,q)||e!=epoch||q>sequence){Serial.println("epoch/watermark changed; reconnect");return;}
        attempts=matched=0;running=true;pending=false;next_job=millis();return;
    }
    if(!std::strcmp(s,"stop")){running=pending=false;return;}
    if(!std::strcmp(s,"disconnect")&&client){running=pending=false;if(client->isConnected())client->disconnect();wr=rd=nullptr;return;}
    if(!std::strcmp(s,"status")){Serial.printf("BLE running=%u attempts=%lu matched=%lu heap=%u min_heap=%u\n",unsigned(running),
        static_cast<unsigned long>(attempts),static_cast<unsigned long>(matched),unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()));return;}
    Serial.println("unknown/busy");
}
void client_tick(){
    if(!running)return;
    if(!client->isConnected()){running=pending=false;Serial.println("BLE disconnected");return;}
    if(pending){
        uint32_t e=0,q=0;const bool ok=read_remote(e,q)&&e==epoch&&q==sequence;
        const auto elapsed=esp_timer_get_time()-sent;
        if(ok||elapsed>=1000000){
            if(ok)++matched;
            Serial.printf("ble sample seq=%lu applied=%u elapsed_us=%lld\n",static_cast<unsigned long>(sequence),unsigned(ok),static_cast<long long>(elapsed));
            pending=false;next_job=millis()+100;
        }
        return;
    }
    if(attempts==32||sequence==0xffffffffu){
        running=false;Serial.printf("ble end attempts=%lu matched=%lu\n",static_cast<unsigned long>(attempts),static_cast<unsigned long>(matched));return;
    }
    if(int32_t(millis()-next_job)<0)return;
    ++sequence;++attempts;auto b=ble::encode({epoch,sequence,uint16_t(attempts*1999),uint16_t(65535-attempts*1900)});
    sent=esp_timer_get_time();pending=true;wr->writeValue(b.data(),b.size(),true);
}
}
#endif
namespace {
void serial_poll(){
    for(unsigned i=0;i<64&&Serial.available()>0;++i){
        const int c=Serial.read();
        if(c=='\n'){if(!overflow){line[used]=0;command(line);}else Serial.println("line too long");used=0;overflow=false;std::memset(line,0,sizeof(line));}
        else if(c!='\r'){if(c<32||c>126||used>=sizeof(line)-1)overflow=true;else if(!overflow)line[used++]=char(c);}
    }
}
}
void setup(){
    Serial.begin(115200);
#if CLUSTER_BLE_PANEL
    queue=xQueueCreateStatic(1,sizeof(ble::Arrival),storage,&queue_state);
    panel.begin();
    Serial.println("BLE local field; ble <epoch8> <clientMAC12>; remote/local/advertise/audio on/mute/status");
#else
    Serial.println("BLE serial client; ble; connect <serverMAC12>; run/stop/disconnect/status");
#endif
}
void loop(){
    serial_poll();
#if CLUSTER_BLE_PANEL
    const auto gen=generation.load();
    mod.mode(enabled.load()&&connected.load()&&!blocked.load(),gen);
    ble::Arrival a{};
    if(queue&&xQueueReceive(queue,&a,0)==pdTRUE){
        if(gen==generation.load()&&mod.receive(a,millis()))applied.store(mod.sequence());
        else rejected.fetch_add(1);
    }
    if(gen!=generation.load())mod.mode(false,generation.load());
    const auto now=millis();panel.input(input,now);
    const auto xy=mod.effective(input.local,now);field.xy(xy.x,xy.y);
    auto route=send_nothing;field.tick(now,route);
    panel.output(field,now,false,0,false,false,mod.fresh(now)?"BLE MOD":"LOCAL");
#else
    client_tick();
#endif
    delay(1);
}
#endif
