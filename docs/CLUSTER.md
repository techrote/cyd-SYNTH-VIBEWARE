# Heterogeneous compute clusters and dual-CYD instruments

Tracking #9, with baseline #8 and platform #2. Wireless first; owner has spare ESP32-S3 and ESP32-C3 boards. Treat compute nodes as execution participants, not merely remote control endpoints. Keep standalone CYD behavior and local control/audio/display deadlines; distributed profiles optional. No generic cloud-style scheduler before an actual consumer.

## Profiles

| Profile | Intended allocation |
|---|---|
| CYD alone | Reference, complete local instrument, no radio required |
| CYD + S3 | DSP/control-rate spectral analysis, simulations, procedural generation; S3 vector/floating point optimization only when measured |
| CYD + C3 | Fixed-point/integer events, sequencers, finite state machines, compact sensors, network/control roles |
| CYD + S3 + C3 | Heterogeneous coarse jobs, simulation + event generation; compare one/two nodes and radio contention |
| Two CYDs | Complementary touchscreen panels, shared events, independent voices/simulations, local audio on each; peer autonomy or explicit coordinator |
| 4+ nodes | Scale only if measurements justify; finite topology and queues |

S3 is dual-core Xtensa LX7 with single-precision FPU; C3 is single-core RISC-V, no hardware FPU. Exact clock, flash/PSRAM, module and available memory measured/recorded; no assumption of arbitrary spare RAM. See [Espressif chip comparison](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/hw-reference/chip-series-comparison.html). Remote audio hardware needs its own verified output profile; don't assume C3/S3 has classic ESP32 internal DAC.

## Transport experiments

- ESP-NOW first: compact control/events, small summaries; no infrastructure AP required. Initial total application message <=250 bytes for mixed v1/v2 compatibility, including headers; negotiate larger v2 frames only after actual SDK/peer evidence. Verify same-channel behavior, peer configuration, delivery under interference, optional encrypted unicast/pairing. MAC send success is not application acknowledgement; use application ACK/idempotent retry only for necessary reliable commands. Use freshest-state-wins for continuous updates. [Espressif ESP-NOW guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html).
- Wi-Fi UDP over dedicated SoftAP or LAN: larger state/tiles, model transfer, bulk data and timestamped snapshots; cap fragments, validate message lengths, rate-limit, explicit loss policy and session reset. TCP optional for non-real-time configuration, never inside audio deadline. Measure actual goodput and jitter, not PHY rates.
- BLE GATT: controller/telemetry/discovery alternative; test negotiated MTU, intervals, latency, packet budget and Wi-Fi coexistence. Do not presume Bluetooth Classic on S3/C3. Start with one radio transport active at a time.
- Wired fallback only if necessary or explicitly evaluated: UART initially, SPI or suitable differential link if throughput requires it. Verify CYD free GPIO/bus conflicts, voltage, shared ground, power supply and connector pinout. No arbitrary power-sharing/backfeeding or rewiring without owner approval.

Wireless is best effort. All transports use same logical portable contract and fault injection suite, not assumed comparable latency. No hard-real-time network dependency.

## Job and state architecture

CYD owns local touch, display scanout and safe local audio clock. Offload stateful/coarse work with compact inputs/results: XENOBIO organisms/events, VECTORFIELD agents, CHRONOGRID future events, FFT features, slow modulation, preset search or reduced indexed visual grids. Send seed/parameters/deltas rather than raw frames when possible; worker nodes can maintain simulation state locally. Dispatch asynchronously into finite queues, consume latest coherent result if within age/deadline budget; interpolate/crossfade or degrade locally when stale. Keep instrument usable without peer. Never block audio or touch on network.

Protocol: negotiated version/precision/kernel and transport capabilities, unique session epoch (reset/rejoin), node/job IDs, sequence, timestamp/deadline and payload length with validation; explicit state ownership, bounded fragmentation/reassembly, duplicate/out-of-order handling, backpressure, leases, heartbeat, authorized peer pairing, stale result rejection and reset. Use explicit encoding/endianness; no raw ABI structs on wire. Callbacks enqueue bounded work only. Expose visible connection status and manual disconnect. No automatic peer firmware updates.

Compute-to-transfer comparison: offload latency = encode + queue + transfer/request + remote execution + result transfer + decode + application scheduling. For async pipeline also measure saved local CPU time, quality/throughput and radio/serialization cost; offload wins only for matched workload/quality and beneficial end-to-end performance. Compare measured percentiles and deadline misses, not just peak throughput.

Illustrative payloads, excluding framing: RGB565 320x240 at 30 fps = 4.608 MB/s (36.864 Mb/s); indexed 160x120 at 15 fps = 288 kB/s; 32 agents x 8 bytes x 20 Hz = 5.12 kB/s; 24 kHz 16-bit mono PCM = 48 kB/s. These are requirements, not measured wireless capacity. Favor compact state, local rendering, distributed computation. Remote PCM only experimental over measured transport with jitter buffer, clock drift correction, deadlines, underrun concealment/mute and explicit latency tradeoff. Do not claim sample sync on packet arrival; estimate clock offsets/drift, timestamp events ahead and measure synchronization. Dual CYD should first play independent local voices synchronized by musical events, not stream synchronized samples.

## Founding experiments

A. XENOBIO/VECTORFIELD: local vs S3 stateful simulation with compact state/event updates and deterministic same-seed comparisons, then C3 or mixed nodes; compare local frame/audio, simulation scale and net latency. B. CHRONOGRID/dual CYD: two local autonomous engines sharing timestamped tempo/events; one editing rhythm, other controlling synth/timbre or complementary voice. Handle clock drift, disconnect/rejoin, duplicate/late events and transport panic. Optional C: PHOSPHOR low-resolution indexed feedback or compact parameter maps; actual net benefit before expansion. Optional D: S3 spectral/control-rate modulation with actual audio causal response. All must retain standalone fallback.

## Qualification and ordering

1. Portable native multi-process/loopback kernel and protocol, deterministic transport fault injection, no generic framework.
2. Real build profiles for classic CYD, S3, C3; exact toolchain/board/pins; actual ESP-NOW and UDP/BLE profiles tested when physical access permits.
3. Measure isolated transport with payload size/rate sweeps (latency/RTT p50/p95/p99/max, useful goodput, loss, duplication, reorder, queue, retries, CPU and heap); compare local radio-off and simultaneous graphics/audio. One-way timestamps only with qualified clock mapping; RTT otherwise.
4. Actual end-to-end matched workload vs local baseline, 0/1/2 auxiliary nodes and dual CYD; record remote timing, CYD saved compute and deadline margin, interaction and audio/frame behavior; verify loss/reset/rejoin/version/clock drift/saturation. No physical claims from native or target compile.
5. Independent QA, exact manifests/Windows evaluator and device package/recovery gate. Owner physical test only after explicit authorization; no automated flashing or merging.

HYBRID owns #9 when activated and coordinates platform contracts; PLATFORM can design a minimal portable interface while working #2, creative lanes can continue without nodes. #8 characterisation informs choices but does not block useful prototype work. Optional ACCEL owns measured bottlenecks only. See WORKERS.md and QUALIFICATION.md.
