# PLATFORM P1: optional strip-rendered PHASESPACE reference (Draft/HOLD)

Consumer: exact same-repository PHASESPACE M3 0139740e3db44496a779d6629b424397f9f53dfb, core blob 50b8bb909b0c54480bf6a1180d830083ba4cbdaa. The original engine and all DSP/input mappings remain unchanged. This is an instrument-specific adapter and a bounded clipped RGB565 sink; not a shared ABI or required runtime. New code original; no third-party source copied. No redistribution license determination. Keep existing local renderer as baseline.

Compared PORT M2 96e1ddd9 (8-row cadence, 640B line, ordered events, espressif32 7.1.3) and HYBRID M9 6956bdbe (8-row snapshots, separate lease/transport, instrumentation) to TOUCH M3 (4-row cadence, same display pin family, espressif32 6.10.0). Do not change audio, touch calibration, SDK or transport; verify target board and rights before physical deployment. Diviner owner baseline #8, publication 771064d419c4391f8252a31107606f4a801e8186, reports full RGB565 transfer about 32.28ms on classic CYD; raster improvement does not predict total display speed. cyd-MHAC dbbb9eec uses espressif32 7.1.3 and explicit -ffp-contract=off; no source imported.

Local executed commands in recovered exact-core directory:
    g++ -std=c++17 -O2 strip_test.cpp -o strip_test
    ./strip_test bench
    clang++ -std=c++17 -O1 -g -fsanitize=address,undefined strip_test.cpp -o strip_san
    ./strip_san

Both PASS 27,340,800 exact RGB565 pixel comparisons, strip guard checks, invalid coordinates, ring wrap, six patches and 55,296 PCM samples identical with/without actual visualization points. Trace object 520B both versions. Four-row candidate caller scratch 2560B vs old row 640B (+1920B); separate existing typed transfer row unchanged. Eight rows 5120B. Also transient empty Trace object 520B (compiler stack layout not measured). Caller allocates width*rows; synchronous strip ownership only; no new DMA assumption.

Five repetitions of 400 full frames, same actual held Flux trace and horizontal stress. Median microseconds/frame, host only:
| Workload | M3 row | 4 rows | 8 rows |
|---|---:|---:|---:|
| Flux |236.62|158.25|144.84|
| Horizontal |245.28|166.05|140.18|

One 4-row Flux repetition was 341.13 us/frame. No target timing/guarantee. No SPI, simultaneous audio, OS scheduler isolation or physical latency included. Four rows selected to preserve existing input-service cadence and modest memory cost.

prepare.py modifies ONLY a disposable exact M3 checkout, not the reference branch. It applies opt-in renderer to existing native GDI/waveOut and target display adapter, leaving DSP and I2S unchanged. Windows CI tries static MSVC CRT to address QA #15 runtime prerequisite (no DLL copied); verify imports independently before claiming self-contained. Existing package retains explicit runtime prerequisite. Hosted Linux/Windows/target qualification pending at author checkpoint. Do not flash. Physical, independent QA and distribution rights HOLD. No universal adoption without a second proven consumer.

To reproduce: checkout this PLATFORM ref and separately exact M3 at consumer; run python platform/reference-strip/prepare.py consumer; build consumer/instruments/phasespace with CMake and ctest, or PlatformIO pinned profile as in workflow. The resulting candidate has two explicit source identities; preserve both. 
