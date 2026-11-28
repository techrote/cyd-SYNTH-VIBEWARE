#pragma once
#include "link.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
using Socket=SOCKET;
using AddrLen=int;
constexpr Socket invalid_socket=INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket=int;
using AddrLen=socklen_t;
constexpr Socket invalid_socket=-1;
#endif
using namespace cluster;
using Clock=std::chrono::steady_clock;
using namespace std::chrono_literals;
inline uint32_t ms() { return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count()); }
[[noreturn]] inline void fail(const char* why) { std::cerr<<why<<'\n'; std::exit(1); }
class Udp {
    Socket s_;
    sockaddr_in peer_{};
    static sockaddr_in address(unsigned port) {
        sockaddr_in a{}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(uint16_t(port)); return a;
    }
public:
    unsigned rejected=0;
    Udp(unsigned own,unsigned peer):s_(::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)),peer_(address(peer)) {
        if(s_==invalid_socket) fail("socket failed");
        auto a=address(own);
        if(::bind(s_,reinterpret_cast<const sockaddr*>(&a),sizeof(a))!=0) fail("bind failed");
#ifdef _WIN32
        u_long yes=1; if(ioctlsocket(s_,FIONBIO,&yes)!=0) fail("nonblocking failed");
#else
        if(fcntl(s_,F_SETFL,O_NONBLOCK)!=0) fail("nonblocking failed");
#endif
    }
    ~Udp() {
#ifdef _WIN32
        closesocket(s_);
#else
        close(s_);
#endif
    }
    bool send_bytes(const uint8_t* p,std::size_t n) {
        if(!p||!n||n>1200)return false;
        return sendto(s_,reinterpret_cast<const char*>(p),int(n),0,reinterpret_cast<const sockaddr*>(&peer_),sizeof(peer_))==int(n);
    }
    bool receive_bytes(uint8_t* p,std::size_t capacity,std::size_t& size) {
        size=0;if(!p||!capacity||capacity>1200)return false;
        std::array<char,1201> buf{};sockaddr_in src{};AddrLen len=sizeof(src);
        const auto n=recvfrom(s_,buf.data(),int(buf.size()),0,reinterpret_cast<sockaddr*>(&src),&len);
        if(n<0)return false;
        if(src.sin_addr.s_addr!=peer_.sin_addr.s_addr||src.sin_port!=peer_.sin_port||n==0||std::size_t(n)>capacity){++rejected;return false;}
        size=std::size_t(n);std::memcpy(p,buf.data(),size);return true;
    }
    bool send(const Frame& f) {
        if(!f.size||f.size>max_frame) return false;
        return sendto(s_,reinterpret_cast<const char*>(f.bytes.data()),int(f.size),0,reinterpret_cast<const sockaddr*>(&peer_),sizeof(peer_))==int(f.size);
    }
    bool receive(Frame& f) {
        std::array<char,max_frame+1> buf{}; sockaddr_in src{}; AddrLen nsrc=sizeof(src);
        auto n=recvfrom(s_,buf.data(),int(buf.size()),0,reinterpret_cast<sockaddr*>(&src),&nsrc);
        if(n<0) return false;
        if(src.sin_addr.s_addr!=peer_.sin_addr.s_addr||src.sin_port!=peer_.sin_port||n<int(header)||n>int(max_frame)) { ++rejected; return false; }
        f={}; f.size=std::size_t(n); std::memcpy(f.bytes.data(),buf.data(),f.size); return true;
    }
};
