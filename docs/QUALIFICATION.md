# Qualification and owner delivery

Independent gates for each exact revision/profile. Use PASS, FAIL, BLOCKED, NOT RUN, NOT APPLICABLE with evidence and reasons. A queued/skipped run is not PASS. Native playback is not ESP32 execution; compile/link is not hardware performance; simulated touch is not physical touch.

| Gate | Required evidence |
|---|---|
| Concept | Distinct playable behavior, design choices, control mapping, concrete plan |
| Native core | Actual Linux/Windows build/run, deterministic fixtures, audio/visual state from real core |
| Native interactive | SDL2 controls, mouse/single-touch replay, actual framebuffer/audio, safe exit/mute; headless and graphical evidence clearly distinguished |
| CYD target build | Exact board/profile/toolchain, compile/link, map, partition/size evidence, source/rights ledger |
| Virtual target (optional) | Only if real supported emulator execution exists; pin emulator and explain actual modeled peripherals and limitations. Otherwise N/A or BLOCKED, no invented emulator |
| Package | Independent clean reproduction, extracted Windows ZIP execution (including spaces path), exact SHA/tree, artifact checksums and manifests |
| Physical owner | Actual board/recovery, authorized installation, audio/touch/display/timing/stability witnesses and owner acceptance |

## Test matrix

- DSP: reset, silence, max/min controls, block partition, numerical safety, finite bounded outputs, clipping/headroom/DC, smooth transitions, feedback limits, sustain/release/mute, long runs. Actual-core WAVs with sample rate/bit depth and presets; subjective listening claims only if actual listening occurred.
- Touch: calibration/rotation, bounds, contact bounce, down/move/up/cancel and lost release, rapid gestures, reachable escape/panic, no required multitouch; native input replay vs physical input distinguished.
- Simulation: deterministic seeds/timestep, bounded pools and event queues, invariants, intervention causality, catch-up limits, sustained edge cases; compare simplest useful baseline.
- Graphics: actual 320×240 output (or declared scaled simulation), clipping/strip edges, deterministic seeds and tolerances, visual snapshots with scripted controls, buffer ownership and DMA completion; no unverified target fps or subjective visual claims from hashes alone.
- Concurrency: render overload, maximum controls, input/event bursts, deadline pressure, safe audio/visual data exchange, no blocking I/O on audio thread, serial/radio/SD contention only on implemented profiles. Native sanitizer/TSan where compatible; distinguish host from device timing.
- Persistence: version/length/corruption, reset/fallback and interrupted saves; optional features N/A with reason.
- Target resources: exact map, flash fit, static/stack budget; runtime heap/largest block, stack high-water, measured callback worst case and underruns, frame/strip timings, touch latency under real workloads only when physically executable. Report methodology and device/profile.
- Packaging: Linux/Windows clean reproduction, pinned dependencies, self-contained native Windows executable + required SDL2 runtime if dynamic, instructions, paths with spaces, actual extracted launch/replay, checksums. Do not label Linux or headless run as graphical Windows execution.

## Owner package

For each qualified candidate: portable Windows evaluator ZIP, screenshots and/or actual rendered sequences, WAV gallery if sound exists, controls guide, presets/scenes and deterministic replay, source commit/tree and CI run/artifact links, checksums, exact toolchain/dependencies, licenses/notices, known limitations and profile. Target compiled output may be retained as clearly unapproved engineering evidence. Installable firmware for owner use requires exact board identification, supported image/partition format, recovery/backup procedure, rights and explicit authorization. No unattended flashing, erasure or owner device changes.

QA owns independent delivery; coordinate scoped repairs and obtain separate reproduction of authored fixes or mark provisional. Draft/HOLD until owner review, no merges without authorization. Keep a compact ledger per instrument with exact candidate and each gate, failing evidence and next action. Never weaken a gate to pass. Prefer one completed playable delivery to many partial reports.

## Optional cluster profile (#9)

Keep standalone and distributed results separate. Match workload, seed and output/quality across local and remote variants. Capture payload/goodput, serialization, queue and compute times, latency percentiles, loss/reorder/duplicate and local audio/frame/heap impact; use RTT unless one-way clock mapping qualified. Test 0/1/2 nodes, S3/C3, dual CYD as implemented; fault-inject disconnect, reset, rejoin, stale/late, clock drift, version mismatch and saturation, with no stuck audio or UI blocking. Actual hardware required for physical wireless claims. Owner-led #8 supplies physical baseline, not a substitute for qualification of each profile.
