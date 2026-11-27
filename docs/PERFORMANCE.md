# Physical characterisation and workload baselines

Tracking #8; supports #2, #7 and #9. Owner executes Codex on available physical hardware; scheduled VIBEWARE workers do not have assumed device access. Diviner is an independent reference, not a VIBEWARE dependency. Do not modify it from ordinary workers.

## Baseline

Inspected [Diviner main 7b1b37e](https://github.com/techrote/cyd-diviner/tree/7b1b37eec6ac126f334a0b008d91135071ea60fa). Resolve fresh head before work and pin it. Existing platformio.ini profiles cyd, cyd-diagnostics, cyd-pcm and native; src/hal_cyd.cpp diagnostics accept tap X Y, touch X Y, release, status and shot (binary RGB565 physical readback). Existing status includes heap/largest, present_us, draw_us, audio_blocks and signal_blocks. Direct rendering interleaves rendering/transfer. Audio task pinned core 0 with callback mutex and blocking I2S write; USB PCM producer queues to task on core 1, drops full-queue blocks, receiver detects gaps. Existing status/shot suppression during PCM is intentional. Separate quiet capture windows or explicitly versioned framed telemetry; don't inject text into a binary stream.

## Codex mission — CYD-DIVINER-PERF-CHARACTERISATION-001

Repository https://github.com/techrote/cyd-diviner. Consumer https://github.com/techrote/cyd-SYNTH-VIBEWARE/issues/8. Work from current accepted main in an isolated branch; inspect current work and tests. Preserve all ordinary firmware behavior, original application profiles and source rights. Build a dedicated opt-in profiling firmware and repeatable Windows capture/analysis harness; perform actual physical tests only on explicitly owner-authorized boards, after backup/recovery and confirmed port/board/flash profile. Never automatically flash, erase, alter network credentials or probe unrelated devices. If board not available, deliver exact build and commands for owner, not invented readings.

1. Inventory toolchain, core clocks, actual hardware revision, flash/PSRAM, display/touch, SPI, serial, task priorities, DMA and heap. Rebuild normal and diagnostic firmware; record size/maps. Measure instrumentation overhead with profiling on/off.
2. Extend existing diagnostics with bounded counters/histograms and timestamped run IDs, warmup, fixed-duration runs, reset/sample controls and machine-readable summaries. Measure frame intervals and update/raster/transfer (separate only when instrumentable), touch processing vs physical latency, task scheduling/utilization, heap free/min/largest, stack high-water. Audio callback/lock wait/mix/I2S enqueue duration, sample period, PCM queue occupancy/drop, sequence/CRC, detectable DMA underruns. Explicitly separate inferred from directly detected underruns and host timings from target timings.
3. Exercise representative actual workloads: idle/menu with backdrop variations, Visions each effect and layered combinations, fullscreen vs UI visible, portrait vs landscape, feedback trail formats; drone/seq/beat and combined graphics/audio; scripted touch/rapid transitions; local DAC, USB PCM and coexistence; repeat after sustained running. Use deterministic seeds/gesture replays where possible. Read back physical display outside measurement windows. Capture true frame counts and elapsed time, not loop iterations. Include radio-off baseline and Wi-Fi/BLE/ESP-NOW overhead only where safe and supported. Capture at least three steady repetitions where practical.
4. Add focused reusable benchmark kernels, isolated from normal UI: float32 vs fixed-point oscillators/filter/modal processing, FFT, cellular grid, particles, indexed feedback/warp and RGB565 strip transfer. Record data size, operation/precision/output checks, warmup, per-kernel timing and peak memory. Build equivalent kernel binaries for S3 and C3 if available, pin profiles; no assumption that all modules have PSRAM or FPU. Characterize realistic loads before testing max throughput.
5. Produce capture script, raw logs and versioned JSON/CSV (source SHA, firmware digest, board/profile, clock, workload, seed, settings, duration, samples, warmup, instrumentation settings, metrics/units, median/p95/p99/max, misses/drops, limitations). Include exact replay commands, build and toolchain receipts, sanity checks, known blockers. Generate comparative summary and proposed VIBEWARE performance budgets with evidence. Separate physical measured results, target compile, and host replay. Measure enough samples to support percentile claims; don't extrapolate missing values.
6. Deliver a focused Draft/HOLD PR in Diviner, profiling binaries as unapproved evidence and a portable Windows capture package/instructions. Report results and artifact links to VIBEWARE #8 with source/reproduction metadata, not copied upstream code. Do not merge without owner authorization. No unrelated optimization or architecture rewrite; small fixes needed for truthful diagnostics only, separately documented.

## Measurement matrix and interpretation

| Workload | Main question |
|---|---|
| Menu/backdrop, simple fullscreen, layered Visions, trail histories | CPU vs SPI bandwidth, framebuffer/strip costs, adaptive complexity |
| Drone/seq/beat, rapid gestures, local DAC | Actual audio deadline margin and touch behavior |
| USB PCM 24 kHz 16-bit + visuals | UART queue/CRC/loss, UI delay, audio coexistence |
| Radio off/on and traffic | Incremental CPU/heap costs, p95/p99 jitter, audio/display degradation |
| Portable kernels on CYD/S3/C3 | Compute throughput per result byte and portable numeric equivalence |
| Sustained stress and scene switching | Memory high-water, fragmentation, leaks, stability |

Measure p50/p95/p99/max, run count and units; mark missing data NOT RUN. Use realistic fixed seeds and repeated captures, not a single instantaneous status reading. A candidate 128-sample/24 kHz block is 5.33 ms; compare callback and enqueue scheduling to actual cadence, not just mean compute. No hard guarantee from average. Keep measurement overhead visible. Feed local bounds and candidate offload break-even into #9. Baseline is advisory, not a gate that prevents portable creative cores.
