#pragma once
#include "dirty_scan.hpp"
#include <type_traits>

namespace cyd::platform {
// Exact M4R display cadence and buffer geometry. Compile-time selection keeps
// full-redraw modes free of the selective scheduler's second snapshot.
enum class XenobioScanMode { Original, Span, Selective };
enum class ScanStep { Idle, Submitted, Complete, Failed };
struct XenobioFullScan {
    xenobio::Engine frame{};
    int row=240;
    int next_row() const { return row==240 ? -1 : row; }
    void begin(const xenobio::Engine& e) { frame=e; row=0; }
    template<class Row> void for_each_next_row(Row&& write) const {
        for(int dy=0;dy<4;++dy) write(frame,row+dy,dy);
    }
    void commit(int) { row+=4; }
    void invalidate() { row=240; }
};

// Single display owner. Submit must synchronously consume all 1280 typed pixels
// (or wait for DMA) before returning. False means display contents are unknown.
// No interaction with the audio thread except the caller's snapshot accessor.
template<XenobioScanMode Mode, class Pixel>
class XenobioScanAdapter {
    static constexpr bool selective=Mode==XenobioScanMode::Selective;
    std::conditional_t<selective,XenobioDirtyScan,XenobioFullScan> scan_{};
    uint32_t frameAt_=0;
    std::array<uint16_t,320> row_{};
    std::array<Pixel,320*4> transfer_{};
public:
    bool pending() const { return scan_.next_row()>=0; }
    void invalidate() { scan_.invalidate(); }
    template<class Snapshot,class Submit>
    ScanStep step(uint32_t now,Snapshot&& snapshot,Submit&& submit) {
        if(!pending()) {
            if(uint32_t(now-frameAt_)<50) return ScanStep::Idle;
            scan_.begin(snapshot()); frameAt_=now;
            if(!pending()) return ScanStep::Complete; // unchanged selective frame
        }
        const int y=scan_.next_row();
        scan_.for_each_next_row([&](const xenobio::Engine& e,int row,int dy) {
            if constexpr(Mode==XenobioScanMode::Original) xenobio::labeled_row(e,row,row_.data());
            else xenobio_span_row(e,row,row_.data());
            for(unsigned x=0;x<320;++x) transfer_[dy*320+x]=row_[x];
        });
        if(!submit(y,transfer_.data())) { invalidate(); return ScanStep::Failed; }
        scan_.commit(y);
        return pending()?ScanStep::Submitted:ScanStep::Complete;
    }
};
}
