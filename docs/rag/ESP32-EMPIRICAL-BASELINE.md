---
id: ESP32-EMPIRICAL-BASELINE
version: 0.1.0
as_of: 2026-11-28
status: advisory-DRAFT-HOLD
source_repo: techrote/cyd-diviner
source_evidence_commit: 771064d419c4391f8252a31107606f4a801e8186
tags: [esp32, esp32-s3, esp32-c3, cyd, dsp, graphics, wifi, esp-now, sdr, usb-pcm, benchmarking]
---

# Empirical ESP32 / CYD / S3 performance baseline

## Retrieval and evidence scope

A compact evidence-led reference for instrument firmware, DSP, embedded graphics, heterogeneous computing and USB audio research. This is a synthesis of published owner-hardware reports, not a fresh execution or independent raw-archive recomputation. The source remains [Diviner PR #7](https://github.com/techrote/cyd-diviner/pull/7), Draft/HOLD, based on accepted main `7b1b37eec6ac126f334a0b008d91135071ea60fa`; [consumer handoff](https://github.com/techrote/cyd-SYNTH-VIBEWARE/issues/8#issuecomment-7231264154). All Diviner evidence links below use immutable publication commit `771064d419c4391f8252a31107606f4a801e8186`. [Index and raw archive receipts](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/README.md); [coverage audit](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/FINAL-COVERAGE-AUDIT.md).

Claim classes: **PHYSICAL-REPORTED** = published physical observations for the exact cohort, not independently reproduced here; **HOST-REPORTED** = host/native computation; **BUILD-REPORTED** = compilation/CI; **DERIVED** = transparent arithmetic from declared inputs; **PROPOSED** = design or experiment, not measured; **NOT-RUN** = required observation absent. Keep claim ID, class, context, units, denominator, caveat and source together when retrieving. Never pool failed campaigns, conditional decodes or heterogeneous run statuses as if they were identical trials.

## EMP-BOARD-001 — Configured systems and workload identity

**PHYSICAL-REPORTED.** Classic CYD ESP32-D0WD-V3 rev 3.1, 240 MHz, 4 MB flash at 40 MHz, no initialized PSRAM; ordinary Diviner audio/display infrastructure remains while frame loop pauses for kernel timing. S3 rev 0.2, 240 MHz, 4 MB flash at 80 MHz, 2 MiB quad PSRAM, standalone kernel/peer image without Diviner workers, link off. Both use unchanged portable prepare/step/inspect algorithms with distinct wrapper and source/image identities. CYD kernel source `a4549f87cef05d19d9acf96d59ae95a2435ebb67`; S3 source `174a79670d4b0da656ebd615f0cd1620aa0ddbb1`. C3 remains **BUILD-REPORTED / physical NOT-RUN**. No causal isolation of FPU, ISA, CPU architecture or flash/cache effects.
Source: [recommendations, evidence scope](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/RECOMMENDATIONS-DRAFT.md), [S3 comparison](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/S3-COMPARISON.md).

## EMP-DSP-001 — Scalar arithmetic and matched kernel costs

**PHYSICAL-REPORTED.** Eleven scalar kernels, two sizes, three fresh-boot repetitions, 256 exact timed steps each, seed 23063. CYD 66 cases / 16,896 steps; S3 internal and PSRAM 132 cases / 33,792 steps. Each comparable region/size cell n=768; exact nearest-rank p99; allocation, initialization, warmup and output inspection excluded; RTOS preemption included.

Larger matching cases, microseconds per timed step (median / p99):

| Kernel | CYD | S3 internal | S3 PSRAM |
|---|---:|---:|---:|
| Interpolated float oscillator, 512 samples | 318 / 319 | 265 / 265 | 265 / 265 |
| Fixed oscillator, 512 | 109 / 110 | 97 / 97 | 97 / 97 |
| Float one-pole filter, 512 | 297 / 306 | 248 / 248 | 248 / 248 |
| Fixed one-pole filter, 512 | 192 / 192 | 169 / 169 | 169 / 169 |
| Float modal, 32 voices × 512 | 1916 / 1924 | 1741 / 1741 | 1741 / 1741 |
| Fixed modal, 32 × 512 | 3713 / 3715 | 3346 / 3347 | 3346 / 3347 |
| Float FFT, 1024 complex points | 1405 / 1414 | 1350 / 1350 | 1350 / 1350 |
| Cellular, 128 × 96 cells | 4977 / 4979 | 4065 / 4067 | 4065 / 4067 |
| Particles, 512 states | 90 / 91 | 73 / 74 | 73 / 74 |
| Indexed feedback, 160 × 120 | 4489 / 4528 | 3974 / 3974 | 4021 / 4047 |
| RGB565 stripe, 320 × 16 | 322 / 331 | 301 / 301 | 301 / 301 |

Across 22 scalar cases CYD/S3 internal median ratio 1.041–1.233. CYD fixed interpolation 2.86–2.92× faster than float; fixed one-pole 1.50–1.55× faster; float modal 1.55–1.94× faster than fixed. Q30 state/coefficients and quantized Q15 results used where specified; do not generalize to different precision or algorithm. S3 PSRAM medians equal internal at microsecond resolution in 21/22 cases; larger indexed feedback ~1.18% slower. Warmed bounded active contexts do not establish streaming, cold-cache or DMA bandwidth.

**HOST-REPORTED.** Fresh-block numeric checks at most one Q15 LSB for tested DSP comparisons. Physical output fingerprints/min/max/sums/finite flags agree for all 22 cases; equality is not a per-sample error bound or long-term sonic equivalence. No optimized S3 SIMD comparison executed. One pre-existing USB telemetry failure counter and one logged host recovery query retained; prior incomplete runs remain failed.
Sources: [recommendations](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/RECOMMENDATIONS-DRAFT.md), [full comparison](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/S3-COMPARISON.md), [machine data JSON](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/S3-COMPARISON.json), [CSV](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/S3-COMPARISON.csv).

**PROPOSED.** Retain scalar reference; compare S3 vector/intrinsic/assembly and library kernels at identical precision, alignment, dimensions, output accuracy, long-run state and full application load. Measure speed, memory, energy and deadlines, not theoretical FLOPS. Do not extrapolate a voice count from a kernel in isolation.

## EMP-DISPLAY-001 — Local SPI, fullscreen and color

**PHYSICAL-REPORTED.** Classic CYD 320×240 RGB565 transfer: 153,600 bytes, 384 samples, median 32,281 µs, p99/max 32,293 µs, ~4.76 decimal MB/s including commands and completion. Menu backdrop 29.4–29.5 FPS under 33 ms cap; combined fullscreen Gate+Trails plus local drone/sequencer/beat ~22.7–22.8 FPS. Decorated landscape Trails RGB444 41,760 B history and ~25.0 FPS vs RGB565 55,680 B and ~28.5 FPS. Fullscreen Trails uses different geometric path; format labels do not imply full-screen history.

Later packed-color image 36/36 finite windows (12 settings ×3); one/two-effect native RGB565 Gate 25.00/22.33 FPS; RGB111 conversion 7.89/3.67 FPS; RGB444 and RGBA5551 each 7.00/3.00 FPS. Intermediates are eight-row packed buffers around RGB565 rasterization; total scratch for two effects 12,480/18,240/20,480 B respectively including two RGB565 DMA strips; all retain 64,800 B backing reservation and 16-bit LCD traffic. Not native low-depth renderer throughput or arbitrary layer limit.
Sources: [recommendations](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/RECOMMENDATIONS-DRAFT.md), [color findings](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/COLOR-FINDINGS.md).

**DERIVED.** A 30 FPS frame has 33.333 ms; median full-screen transfer alone consumes ~96.8%. At 20 FPS, 50 ms nominal frame period, leaving 17.719 ms only in an ideal non-overlapped separate-transfer model; actual scenes interleave operations, so this is not a measured CPU budget.
**PROPOSED.** Initial layered fullscreen target 20 FPS; use dirty regions/stripes, local procedural rendering, adaptive visual work. Do not claim reduced palette depth increases throughput without a matched native renderer.

## EMP-AUDIO-001 — Classic local audio scheduling and measurement limits

**PHYSICAL-REPORTED.** Diviner 128 samples / 24 kHz; block cadence 5.333333 ms (DERIVED). Across matching landscape/local cases n=5,625 callback observations each: exact maxima drone 523 µs, sequencer 759 µs, beat 325 µs. Callback excludes independent lock/mix/I2S/scheduling costs; do not add separate p99 or maxima as a measured block. Blocking I2S enqueue includes waiting for DMA space. Supplement recorded 124,687 TX_DONE notifications matching completed/drained blocks, no observed queue-full/DMA-fault notifications. These are not proof of no physical underruns or analog continuity.
**NOT-RUN.** Analog DAC continuity/fidelity/latency, physical contact-to-photon and total observer overhead. Existing Diviner USB PCM is a framed serial route, not class-compliant USB Audio Class enumeration, and this evidence does not establish S3 native USB audio latency.
Sources: [recommendations](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/RECOMMENDATIONS-DRAFT.md), [coverage audit](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/FINAL-COVERAGE-AUDIT.md), [I2S events](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/EVENTS-INTERPRETATION-DRAFT.md).

## EMP-MEM-001 — Allocation, instrumentation and contention

**PHYSICAL-REPORTED.** After radio churn ~75 KB free heap but largest block 31,732 B, insufficient for 40,016 B common kernel workspace. All 66 classic kernels succeeded fresh boot. Generic heap metrics can include non-byte-addressable regions; match capability flags and observe actual allocation. Profiling arrays reserve 24,008 B plus other observer costs. Do not infer ordinary firmware free heap simply by subtracting this allocation.
**PROPOSED.** Preallocate deadline-critical queues and DSP scratch; use heap capabilities, largest contiguous block and stack minima by lifecycle; separately characterize PSRAM/flash/cache misses and interference. Compare ordinary/minimally-counted/full profilers in otherwise identical firmware; histogram-only A/B is not total observer overhead.
Source: [recommendations](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/RECOMMENDATIONS-DRAFT.md), [coverage audit](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/FINAL-COVERAGE-AUDIT.md).

## EMP-RADIO-001 — Application-level echo RTT and useful goodput

**PHYSICAL-REPORTED.** 64-byte echo, conditional on replies in bounded tail, includes queues, packet creation, CRC and peer handling; not RF propagation. Three repetitions per RTT cell; histogram p99 is upper bound.

| Topology | Menu mean ms / p99 upper ms | Fullscreen Gate+drone mean ms / p99 upper ms |
|---|---:|---:|
| ESP-NOW | 18.338 / 65.536 | 12.273 / 65.536 |
| S3 SoftAP | 50.949 / 131.072 | 39.388 / 119.563 |
| Phone AP | 100.989 / 524.288 | 53.978 / 262.144 |

Best observed individual echo ~4.98 ms; max up to 568.765 ms across table. Goodput median decimal B/s, unique application payload excluding framing; ESP-NOW paced 200 B, UDP/TCP 1024 B; offered loads differ.

| Topology/transport | Menu B/s | Fullscreen B/s |
|---|---:|---:|
| ESP-NOW | 15,660 | 25,860 |
| S3 SoftAP UDP | 151,142 | 138,240 |
| Phone AP UDP | 99,635 | 131,994 |
| S3 SoftAP TCP | 55,603 | 79,053 |
| Phone AP TCP (2 valid repetitions per cell) | 35,584 | 56,627 |

All 22 workload types have at least one valid traffic/idle window, but overall capture plans FAILED; two TCP cells 2/3 valid. BLE directed-peer startup failed before arm acknowledgement (symbolicated Bluedroid null write); no usable GATT latency or goodput measurement. Fullscreen idle 22.2 FPS vs 17.5–19.0 FPS with traffic. The profiler is not a maximum-throughput implementation, and application deficits include queue, horizon and telemetry effects, not all RF loss. No causal inference that more graphics reduces RTT.
Sources: [wireless findings](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/WIRELESS-FINDINGS.md), [wireless JSON](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/WIRELESS-OVERVIEW.json), [BLE failure](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/BLE-STARTUP-FAILURE.md).

## EMP-OFFLOAD-001 — Compute acceleration vs expansion

**DERIVED/PROPOSED.** Tested S3 scalar kernel savings 17–912 µs, smaller than even the minimum observed baseline echo RTT. No measured matched job RPC or offload win. For synchronous substitution: compare end-to-end local computation with initiator-clock submission-to-validated-result latency, including packing, queue, transport, remote compute and consumer application. Do not add B/goodput twice when an RTT already includes the transfer. Independently measure local CPU freed, frame/audio deadlines and output quality for concurrent expansion; peer-resident state and small summaries can enable additional workloads without a per-job latency win. Keep local audio clock/touch/UI and failure fallback. No network blocking in callback. Remote event scheduling should be clock/drift-aware; do not infer sample sync from arrival.
Sources: [wireless findings](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/WIRELESS-FINDINGS.md), [offload worksheet](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/RECOMMENDATIONS-DRAFT.md), [HYBRID issue #9](https://github.com/techrote/cyd-SYNTH-VIBEWARE/issues/9).

## EMP-PCM-001 — Packetized audio transport

**PHYSICAL-REPORTED.** ESP-NOW broadcast PCM16, synthetic source, no app retransmission/unicast MAC ACK, bounded admission/assembly and two-block receiver reserve. 50 valid three-second windows / 195 attempted (8 closed attempt directories); S3 matched live metadata 22/48, deferred 25/48, both plans FAILED. Best observed 24 kHz stereo 32 frames/block requested 6 Mbps: nonconcealed coverage 87.87–88.84% live, 86.62–87.56% deferred, ~11–13% PLC; digital source/error SNR ~8.1–9.3 dB includes concealment. No analog I/O. Example 4,016 API-accepted TX packets, 31 receiver accepted blocks, 3,891 future-window rejects: not 99% RF loss. PHY rate API acceptance is not over-air rate verification.
**DERIVED.** 24 kHz stereo PCM16 96,000 B/s; 48 kHz stereo 192,000 B/s, one direction before overhead. Two blocks ×32 frames /24 kHz = 2.667 ms configured reserve, not measured one-way latency. Eight-frame stereo at 24 kHz needs 3,000 packets/s; 32 B PCM +20 B header yields 1.248 Mbps before MAC/PHY, already exceeding nominal 1 Mbps.
**PROPOSED.** Prefer asynchronous state/control, local synth or wired bridge for first qualified instruments. Wireless PCM requires real continuous recovery, sample-clock management and deadline/quality testing.
Source: [PCM findings](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/PCM-FINDINGS.md), [recommendations](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/RECOMMENDATIONS-DRAFT.md).

## EMP-SDR-001 — Finite raw RF experiments, separate from USB audio

**PHYSICAL-REPORTED.** Original low-rate 23 ksymbol/s, 2 kHz stereo OSR4: 15/16 source frames recovered in each of three runs; zero errors/3,840 compared payload bits per run, but missing leading sequence is not success. Coarse boxcar reconstruction SNR ~6.74/6.03 dB. Later isolated S3 source `e8b4c8a9f5073db86706f9cebbaefb0476549ef6` (ESP-IDF 5.5.5) closed 78 shared TX +18 receiver-only exposures, two receivers, 192 retained finite native buffers, 56 fast-clock receipts. Paced software plateau ~9.881 million writes/s (not RF goodput); alternating envelope periodicity through requested 8 million/s. At 4.416 Mbaud 48 kHz ×4 /OSR16: RX1 53/3072 (1.725%) and RX2 41/2816 (1.456%) conditional BER, headers 3/3 each; only 48/44 reconstructed source samples/channel. At 8.832 Mbaud no headers in 3/3 per RX; one TX window 50 target words unsent. Fan repeat one RX headers 3/3, 196/768 errors (25.5%), other no headers. Finite sparse host decoding is not continuous streaming, delivered rate, calibrated spectrum or whole-run BER. No continuous on-device audio receiver.
Sources: [fast findings](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/METAL-FAST-FINDINGS.md), [original proof](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/SDR-PROOF.md), [fan repeat](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/FAN-FRAMED-REPEAT.md), [WiDAC PR #1](https://github.com/techrote/WiDAC-ESP32S3-SDR/pull/1). Keep WiDAC independent; no production adoption or extrapolation.

## EMP-THERMAL-001 — Sustained load and sensor limits

**PHYSICAL-REPORTED.** Two separate 365 s classic CYD stress windows; 355/356 retained 1 Hz samples, weighted non-idle core0/core1 98.9947/99.9602% and 98.9871/99.9515%. Raw sensor codes 128 throughout first, 139–146 second; uncalibrated, not exact Celsius or arithmetic cycle usage. S3 fast SDR 576 outside-window readings; TX indication up to 98.9 °C, later fan repeat 29.9–55.9 °C. ESP-IDF driver initial 10–80 °C request can auto-range; not an absolute ceiling, but sensor accuracy, cached range-state interaction and physical temperature remain unverified. Movement, RF geometry, workload history and airflow co-varied. No causal claim of heat-induced errors.
**PROPOSED.** Use fixed geometry, controlled workload, airflow/ambient and external probe, stop thresholds based on verified board/component ratings, separate sensor calibration and repeat safely. Do not pursue sustained high-temperature operation as an optimization.
Sources: [thermal ledger](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/TEMPERATURE.md), [range clarification](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/TEMPERATURE-RANGE-NOTE.md), [coverage audit](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/FINAL-COVERAGE-AUDIT.md).

## EMP-GAPS-001 — Research backlog and consumption rule

**NOT-RUN:** total observer overhead, physical C3, physical contact-to-photon, analog DAC continuity/latency, calibrated thermal/RF measurements, matched job RPC and concurrent compute benefit, continuous on-device SDR audio/video, native USB UAC enumeration/streaming/full-duplex and USB DSP performance. New USB work must qualify its own board/firmware/host and transport; no carryover of Diviner results as USB-specific measurements.

**BUILD/HOST-REPORTED:** handoff reports 163 Python tests, portable model/low-level checks, all 19 CI jobs passed, archive/member integrity checks. These support reproducibility, not physical closure. No raw archives independently reprocessed by this synthesis.

**PROPOSED next work:** (1) reconstruct exact numerical baseline from raw data and independent parsers; (2) compare scalar/vector DSP accuracy and deadlines; (3) physical matched-job cluster and C3 measurement with local coexistence; (4) native USB device and full-duplex DSP as a separate qualified research programme; (5) instrument/FX integration and latency/clock drift; (6) optional continuous RF audio in WiDAC. Preserve exact source and firmware identity, raw and derived data, hardware/profile, measurements and failures. Never promote exploratory claims merely because a build passed.
