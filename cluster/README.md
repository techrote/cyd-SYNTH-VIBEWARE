# CYD-CLUSTER-001: M0 portable vector-field prototype

Draft/HOLD, original source; no imported reference code. No firmware, audio, physical display, radio, or external transport in this initial slice. This is a bounded single-peer in-process experiment, not the final cluster architecture. It does not modify shared PLATFORM interfaces.

## Run

```sh
cmake -S cluster -B build/cluster -DCMAKE_BUILD_TYPE=Release
cmake --build build/cluster --config Release
ctest --test-dir build/cluster -C Release --output-on-failure -V
```

Alternatively GCC/Clang: `g++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic cluster/native.cpp -o cluster_native && ./cluster_native`. The executable runs deterministic fault cases, a wall-time host benchmark and ASCII density preview. No dependency besides standard C++17. No network or flash operations.

## Workload and wire contract

The job is an independent seeded integer vector-field particle cohort, up to 512 particles × 64 integration steps. The attractor coordinates represent a potential touch input. Result is an 8×8 occupancy grid of uint16 counts; sum must equal requested particle count. This is a coarse density field suitable for local visualization/control-rate mapping; no stateful XENOBIO or audio integration yet. The same kernel runs in local fallback and worker. Signed divisions truncate toward zero; positions wrap into uint16. The seed zero has a defined nonzero replacement. All protocol fields explicitly little-endian. One in-flight job, no heap use in core.

16-byte header: bytes 0–1 magic CV, 2 version (1), 3 type, 4–7 nonzero epoch, 8–11 sequence (zero only for hello/ready), 12–13 payload length, 14–15 reserved zero. Exact frame length required. Type 1 hello (0 payload); 2 ready (max particles and steps, 2×u16); 3 job (seed u32, particles, steps, x, y u16); 4 result (64×u16 row-major). Largest 144 bytes, beneath initial 250-byte cap. 28+144=172 application bytes per successful job excluding handshake, retransmissions and transport framing.

Coordinator owns monotonic deadline, uses wrap-safe uint32 elapsed with <=60s budget, and falls back to same local kernel at expiry or disconnect. Worker caches last request/result for idempotent identical retry, rejects older sequences and conflicting same-sequence payloads. Epochs are explicitly configured, nonzero and must be fresh across new sessions; create new objects on reset. Same-session disconnect/rejoin retains coordinator sequence. No distributed clock, automatic lease/rejoin, multi-peer orchestration, remote wall-clock deadline, integrity or authentication implemented; an actual transport must authenticate/authorize peer and supply correctly sized Frame. A result with internally consistent but false counts can pass validation; only trust authenticated workers. The codec is not a security boundary. Poll/compute must run on simulation/control loop, not audio callback; measured target worst-case fallback cost still required.

## Evidence and limits

The native test uses deterministic virtual deadlines and injected drop, delayed result, duplicate, old epoch/sequence, malformed lengths/headers, conflicting duplicate, saturation of single in-flight slot, disconnect/rejoin and millisecond wrap. It compares every completed grid to identical local computation. Benchmark reports actual host steady-clock percentiles of matched local computation and sequential in-process encoding/worker/decoding. This includes remote computation on the same CPU, not a real network RTT or proof of offload advantage. Host timings vary. Output digest is a repeatable witness, not cryptographic integrity. No physical timing claims.

Next: separate-process transport with explicit peer identity, bounded receive queue and loss/latency instrumentation; add target ESP-NOW profiles for classic ESP32/S3/C3 after reviewing exact Arduino/IDF callback versions. Expose time-varying touch-controlled heatmaps and local sound mapping with PLATFORM coordination. No target board flashing or deployment approval. Diviner measurements #8 not yet supplied.

## M1a: separate-process UDP experiment

The portable Link controller adds nonblocking send acceptance, periodic hello (100 ms), retry of the identical job (10 ms), application completion rather than MAC/socket acknowledgement, and automatic re-negotiation after an offload deadline triggers local fallback. No audio callback calls this code. Session epoch still configured out of band. The two real native processes bind only IPv4 loopback and filter the exact configured source IP/port: this is endpoint isolation, NOT cryptographic authentication or a LAN security model. No arbitrary network address or physical device access.

Build via CMake above (Python 3 required for the process test); run `python cluster/udp_test.py build/cluster/cluster_udp` (on Windows use the extracted EXE path). Two 100-job workloads: clean, then deterministic injected result loss (every 11th sequence all copies, every 7th first copy), duplicates (every 5th). Check every resulting grid against the same local kernel. A 60 ms coordinator deadline, 1 ms host polling sleep, and re-negotiation are included; timings are actual local UDP process wall times, not Wi-Fi, compute speedup or one-way latency. The test harness terminates the bounded worker after completion. The Windows console evaluator remains standalone; running its separate-process test requires Python.

Authored Linux GCC strict build, exact source blobs checked against commit: clean 100 remote / 0 fallback, p50/p95/p99/max 1078/1176/2240/3232 us. Faults 91 remote / 9 fallback, 58 retries, all exact matches; remote p50/p95/p99/max 1082/10830/12005/12005 us, fallback 60391/60597/60597/60597 us. Single run, 100 jobs per scenario; host scheduler included; no inferred device performance. New hosted qualification pending. Previous M0 hosted native/sanitizer/extracted Windows gates passed at 3a4f92e, run 46044967658.

## M1b: opt-in ESP-NOW serial compute diagnostic (target qualification pending)

`firmware/platformio.ini` isolates four build profiles: cyd-coordinator (esp32dev), s3-worker (esp32-s3-devkitc-1), c3-worker (esp32-c3-devkitm-1), cyd-worker (esp32dev). PlatformIO espressif32 6.10.0 / Arduino 2.0.17 / IDF 4.4.7 candidate, fixed channel 6, no AP or Internet required, no display/audio/touch drivers, no wiring. These are generic compile profiles, not board-specific flash approval. See [platform release](https://github.com/platformio/platform-espressif32/releases/tag/v6.10.0) and [IDF 4.4 ESP-NOW API](https://docs.espressif.com/projects/esp-idf/en/v4.4.5/esp32c3/api-reference/network/esp_now.html); original adapter, no copied reference code.

Build only: `pio run -d cluster/firmware -e cyd-coordinator` (repeat for other profiles). Target CI records ELF, map, binary, package versions, source/tree and SHA-256. No flash or upload target invoked. Firmware is an unapproved engineering diagnostic. Physical testing requires owner approval, exact board/flash profile and backup/recovery first. This candidate does not yet drive the CYD display or sound, and cyd-worker is a compute node, not dual-CYD performance synchronization.

Application does not start radio at boot. Serial 115200 exposes station MAC and role. Pair exactly once per boot via local serial only, with both nodes configured to the other's 12-hex-digit unicast MAC, the same fresh nonzero 8-hex-digit epoch, and owner-generated independent nonzero 32-hex-digit PMK and LMK (distinct, same pair on both ends). Syntax: `pair <peer-mac12> <epoch8> <pmk32> <lmk32>`. No default keys, persistent application storage, broadcast pairing, echo or credential logging; do not place keys in source, terminal captures or shared logs. Generate keys with OS CSPRNG. Reset/re-pair both nodes with new epoch/keys after reboot; automatic secure session establishment is not implemented. Fixed channel 6; check local radio conditions. Failed initialization requires reboot. Application configures encrypted peer, and callback filters MAC; negative physical tests (wrong key/plaintext/spoof/replay) remain mandatory before claiming authenticated operation. No physical security claims.

Coordinator commands: `xy 0..65535 0..65535` sets the vortex, `run` starts one job per 250 ms with seed starting at 1; `stop` stops scheduling (an in-flight result can finish), `local` forces fallback, `remote` enables paired offload, `status` shows counters. For matched comparison stop, wait for pending=0, keep XY constant and run again; local radio-off baseline is an unpaired boot, local after pairing retains radio initialization. Job receipt gives end-to-end elapsed microseconds, separate local verification compute time, exact match and grid digest. This diagnostic verifies each completion against actual local compute; verification is excluded from recorded completion latency but consumes coordinator resources. Each submitted job snapshots XY; edits during flight affect only the next job. Keep XY constant across comparison runs. Worker executes only bounded received requests. One transmit outstanding until Wi-Fi callback; queue depth 8, drain at most 4 per loop, no compute/log/blocking I/O in callbacks. Busy results are dropped and recovered through application retries; if send callback never arrives, radio transmission stays blocked safely until reboot. No audio or render-deadline qualification implied.

Local authored strict GCC pairing parser test: 285 checks PASS, including all truncated/malformed input positions, invalid MAC, zero epoch/key and equal keys. Physical encryption, target compilation, radio latency, memory/timing, display/audio/touch, dual-CYD event clocks, multi-peer allocation and UDP/BLE wireless comparison are NOT RUN until supported by evidence.


## M2a: VECTORFIELD audiovisual consumer (qualification pending)

Original portable field_av.hpp maps actual completed job density into a 320×240 RGB565 interpolated heatmap and four column-pair density bands into a bounded, smoothed 24 kHz mono triangle-oscillator chord. A cross marks the completed job's attractor, not an uncommitted touch position. Each 100 ms control update runs the same kernel locally or via Link; 60 ms remote timeout. Render rows require only 640 bytes of destination scratch; the Windows adapter uses a display buffer. Audio consumes one coherent 32-bit control word, has no network access, never waits for computation, and releases smoothly. This is a cluster demonstrator, not a full VECTORFIELD instrument or shared PLATFORM implementation.

Windows: run cluster_field.exe locally. Hold and drag left mouse button to move attractor and sound; release or M mutes, Esc exits, loss of focus/capture mutes. No audio until contact; if waveOut fails, visuals remain usable and status indicates audio unavailable. For optional actual loopback UDP, open two terminals: cluster_udp.exe serve 42001 42000 clean, then cluster_field.exe --udp 42000 42001. R toggles remote/local; status shows negotiation and completion mode. Stop worker with Ctrl+C; serve is bounded to 30 minutes. Restart worker before restarting evaluator because this development-only transport uses fixed epoch 91. Loopback only, no claim of authenticated LAN or physical radio performance. Windows audio buffering is four 256-sample blocks, no measured physical latency.

cluster_field_replay.exe [output-prefix] exercises 300 virtual control ticks, local and fault-injected worker path, exact snapshot/render/audio-control comparison after deadline, block partition invariance, bounded PCM and eventual mute; optional prefix writes actual-core three-second WAV and final PPM. No hardware or subjective listening claim. Windows CI includes extracted GUI GDI smoke (synthetic input and actual window paint, no audio endpoint) and replay; Linux/sanitizers exercise portable core. Physical CYD display/audio/touch adapter and real radio integration with this consumer remain next work. Prior M1b exact-head a8a057c target and native CI all PASS (46052105061/46052104997); no physical test.


## M2b: classic CYD physical-panel candidate (not physically qualified)

Fifth isolated profile cyd-field runs the same Field/Voice/row core and existing ESP-NOW transport. Build only: pio run -d cluster/firmware -e cyd-field. Generic esp32dev 4 MB / huge_app candidate; actual board, flash, partition, backup, pinout and restore method must be checked by owner before approved installation. Four serial-only profiles remain available. Exact hardware configuration facts inspected in cyd-diviner 7b1b37e src/hal_cyd.cpp: ILI9341_2 SPI2 GPIO14/13/12/2/15, backlight 21, landscape rotation 5; XPT2046 VSPI GPIO25/39/32/33, provisional calibration 200–3900; GPIO26 internal DAC left channel, GPIO0 BOOT input. No reference application code imported.

Panel boots with radio/audio off; touch moves attractor, serial pair enables encrypted-unicast request/remote jobs as before; local/remote commands toggle transport. Serial audio on explicitly initializes DAC and arms touch-gated sound; mute or BOOT disarms, touch release mutes smoothly, stale control heartbeat (>120 ms) mutes if UI stalls, audio write error latches fault until reboot. Audio renders on its own core-0 task using coherent 32-bit controls; no radio/compute/graphics inside it. Display uses 8-row bounded synchronous slices, coherent density snapshot per frame, two 640-byte scratch rows, no full framebuffer. Serial status reports frame count, max slice/touch/PCM compute duration, successful I2S writes and write errors (not directly measured DMA underruns). No per-result local verification in interactive mode; serial diagnostic retains it. Native evidence does not establish CYD timing, panel color/orientation, physical touch, audio quality, radio coexistence or encryption security.

Dependencies only for this profile: LovyanGFX 1.2.32 and PaulStoffregen XPT2046 v1.4 as pinned in read-only Diviner reference. LovyanGFX declares multiple component licenses (BSD/MIT and font notices); XPT2046 has source notices. CI captures installed license/notice files and package listing; exact-version redistribution review remains required before approved deployment. No flash, no merge, no physical claims.

M2 safety/qualification follow-up: Windows audio also has a 120 ms control lease; CYD rechecks contact age after compute before publishing. Local-only Field disables hello attempts, so no artificial transport busy counts. Extracted Windows package additionally exercises actual GDI input messages and a separate fault-injected UDP worker (`field_udp_smoke.py`), including local/remote toggling, without claiming audio endpoint or physical radio tests.

## M3 — dual-CYD timed pulse experiment (candidate)

New `cyd-dual` profile retains standalone field graphics and uses an independent complementary rhythmic voice instead of density-controlled sustain. Hold touch to hear local pulses after explicit `audio on`; release, BOOT or `mute` disarms/gates as before. Pair two identically built CYDs using the existing encrypted-unicast serial provisioning, fresh shared epoch and matching peer LMK (non-default PMK, kept distinct from LMK); lexicographically lower MAC is leader. Both devices synthesize locally; the follower estimates clock offset/drift from bounded ping/reply and queues future 250 ms beat events sent 180 ms ahead. Complementary note mappings, 85 ms gates; no PCM transfer. Remote pulse reception expires after 600 ms, clock samples after 3 s; follower returns to local cadence, and `local` explicitly disconnects (radio stays initialized); `remote` resumes. Reboot both with a fresh epoch to reset sequence ownership. Radio delivery, audible timing and physical touchscreen are NOT RUN; this is not sample synchronization. Field graphics remain local and do not visualize remote clock phase yet. `status` exposes clock sample count, bounded drift estimate, queued/played/local pulse counts, rejection and max control-loop lateness.

Native: `cluster_dual_test` covers offset, drift, wrap, duplicate, outage and recovery; `python dual_udp_test.py ./cluster_dual_udp` exercises two actual loopback processes with deliberately distinct clock epochs. No radio measurements are implied. Separate authored tests from independent QA. No deploy authorization.

M3 touch X rotates oscillator register and Y controls accent, with the current role/sync and pulse marker displayed in the panel footer. `cluster_dual_test dual-demo.wav` emits 14 seconds of actual-core stereo PCM (leader left, follower right), including a scripted four-second transport outage and recovery; audio is synthesized separately by both voices, not transmitted. This is a host-generated demonstration, not a recording of physical boards.

The two-process UDP harness now runs clean and injected three-second leader transmission blackout scenarios. It asserts actual follower local pulses during blackout and at least four remote pulses after recovery; this is localhost fault injection, not a radio measurement.

## M4a — matched two-cohort scheduling (native candidate)

`multi.hpp` executes two independent 512-particle cohorts at identical seed/attractor/step settings across 0/1/2 peers, sums the two 8x8 histograms only on complete batch, and preserves exact 1024-particle totals. Each cohort is an existing 28-byte v1 request / 144-byte result, unchanged worker firmware; 0 peers execute both locally, 1 peer executes both sequentially, 2 peers execute one each concurrently. Each peer has its own Link, sequence/cache and transport identity; no scheduler calls from audio. One batch outstanding, 2 cohorts, per-cohort deadline (not whole-batch deadline), local timeout fallback and reconnection. A local result may use the control loop for bounded compute. This is a native transport prototype, not yet wired into CYD firmware or graphical field rendering (which currently uses a 512-particle job). All host peers use the same test epoch 91 but isolated loopback source ports; no claim of cryptographic authentication on UDP.

Build via existing CMake instructions. Run `cluster_multi_test` for deterministic exact-match/wrap/duplicate/outage/recovery checks. Run `python multi_udp_test.py ./cluster_multi_udp ./cluster_udp` (use .exe on Windows) for actual 0/1/2-worker process comparisons and loss on the second peer only. 60 batches each, 1024 particles x 64 integration steps per batch; verify every grid against local computation, outside batch timing. Output p50/p95/p99/max in microseconds for batch latency and local reference compute, with separate discovery timing (finite 150 ms before each trial) and peer completion/retry/accepted-TX counts. One-ms control polling is included in batch latency; physical radio throughput and ESP32 CPU benefits remain unknown. Do not infer speedup or call these separate processes S3/C3 hardware. Target firmware adapter, actual peer authentication, graphical combined-grid consumer and physical mixed-device comparison are follow-on gates.

M3 closure: exact predecessor 3bcf4542 native and all six target builds completed successfully ([native 46066705316](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46066705316), [target 46066705277](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46066705277)). [Exact M3 Windows artifact 15231556008](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46066705316/artifacts/15231556008), outer SHA-256 0846ab5b2afaa885376d2f371f128fd19c467fe60a2b295ffa60242a683f4735. Physical, independent QA and owner acceptance remain open.

### M4b — CYD mixed-peer serial diagnostic candidate

The isolated `cyd-multi` build uses the same Multi cohort scheduler and unchanged existing `s3-worker`, `c3-worker`, `cyd-worker` firmware wire protocol. Compile only (`pio run -d cluster/firmware -e cyd-multi`); physical installation still requires board verification, backup/restore and owner approval. This is a serial-only diagnostic, not the touchscreen/audio instrument. No application radio starts at boot. Initial status exposes the station MAC; provision both sides via serial without logging keys. Coordinator `pair0` and `pair1` have the existing pair grammar with a slot suffix. Use the same PMK at the coordinator and workers, distinct nonzero session epochs and distinct LMKs for each link, with matching LMK and epoch on the corresponding worker; worker's ordinary `pair` command points at coordinator MAC. No bundled keys. Staging does not start radio; `run 1` or `run 2` explicitly initializes encrypted peer configuration on channel 6. `run 0` uses no radio and no keys. Reboot all involved devices with fresh epochs before changing profile or peer identities; repeated run on same boot/profile preserves sequence counters. Test radio/security physically before claiming encryption or peer reliability.

Each run performs 60 matched 1024-particle x 64-step batches (same two seeds/attractor trajectory as host harness), reports batch elapsed and reference local compute time separately, per-batch remote/local cohort counts, match and grid digest, then cumulative peer stats/heap/min heap. `stop` prevents new jobs but lets outstanding work finish; `status` reports counters. One pending transmit per peer and round-robin serialized ESP-NOW sends; callback queues bounded frame+slot with exact MAC filtering. No callback compute, no dynamic job queue, no secret echo. Radio failure/timeout uses local computation; one worker failure should not monopolize transmit arbitration. No device timing/compute speedup claim until captured. Compare run 0, 1 S3, 1 C3, 2 mixed nodes and 2 with one worker powered down after initial successful batches. Record raw serial logs, actual board/SDK, exact firmware identity, measured p50/p95/p99/max and remote/local counts; verify all matches, compare net batch completion and local reference timing, and note discovery and logging gaps. This is not an audio/display coexistence witness; a multi-cohort interactive panel is a separate follow-up.


## M5a — interactive heterogeneous VECTORFIELD

New `cyd-multi-field` candidate combines the two-cohort scheduler with the existing actual-core touch, display and GPIO26 audio panel. Boot locally with 1024 particles and no application radio; `audio on` arms touch-gated sound; `mute`/BOOT disarms. Stage `pair0`/ `pair1` as in M4, then `enable 1` or `enable 2` once per boot to initialize peers. `local` forces same-cohort fallback and suppresses new sends, `remote` resumes the negotiated profile; reboot all nodes and rekey for a changed profile. Footer identifies completed snapshot LOCAL 0/2, MIXED 1/2 or REMOTE 2/2. Both cohorts share touch position and step count; display and audio publish only the complete 1024-particle aggregate. New jobs at 250 ms minimum interval; each cohort has a 60 ms timeout, so one peer can take two budgets plus local fallback compute. No real-time whole-batch guarantee. Audio still has independent task and stale-control mute, never calls the network; local compute is on control loop and must be timed physically. `status` reports cumulative per-peer jobs/retries, heap/min heap, maximum control-loop processing time (RX handling and simulation tick, not input/display), panel frame/slice/touch and audio write/compute counters. Existing worker firmware unchanged. No verification recompute in the interactive path; run the serial benchmark separately to validate physical workers. Physical installation remains unapproved.

`cluster_multi_field_test` drives 0/1/2-node deterministic virtual-time gestures across timer wrap, drops all second-peer traffic, checks exact merged grids, RGB565 rows, controls and actual PCM against standalone, verifies local toggling emits no packets and gates release. Native in-process test only; separate process UDP and hardware build gates remain. New target build and extracted Windows replay are included in CI.

### Windows interactive matched-cohort evaluator

`cluster_multi_field.exe` runs the same MultiField aggregate core locally (mouse hold/drag, M mute, R toggle offload, Esc exit). Optional one or two actual IPv4 loopback worker processes (development epoch 91, no LAN/physical authentication claim):

```powershell
# terminal 1
.\cluster_udp.exe serve 42001 42000 clean
# terminal 2
.\cluster_udp.exe serve 42003 42002 faults
# terminal 3
.\cluster_multi_field.exe --udp 42000 42001 42002 42003
```

Use only the first port pair for one worker; no arguments for zero. Terminate workers after play and restart for a fresh development session. All three modes execute the same 1024-particle batch; the footer displays per-batch remote contribution. Extracted package CI runs local GDI smoke and actual separate-process two-peer smoke with a fault-injected peer, checks both workers contributed and local completions occurred during toggling. No audio endpoint assertion in smoke; portable PCM equivalence tested separately. This is an engineering evaluator, not physical CYD performance evidence.

## M6a — matched interactive profiling candidate

Exact M5 source eef3c209 passed native/Windows extracted and all eight target builds: [native 46078785065](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46078785065), [target 46078785058](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46078785058). [M5 Windows evaluator](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46078785065/artifacts/15234939755), outer SHA-256 63e4348c46895844911a41259d2b16e1376ae78257bc54181bb20abe331cb966.

New opt-in `cyd-multi-field-bench` profile: compile only; no flash authorization. Once owner has verified board, backup and recovery, the existing explicit pairing/configuration permits 0/1/2 peers. `bench visual` requires display initialized and audio never started since boot. `audio on` followed by `bench audio` explicitly arms a deterministic synthetic touch gate; use conservative speaker volume and BOOT/mute/bench stop to abort. Real touch is still polled but the scripted XY overrides it during replay. The replay drains old work then resets scene seed without resetting peer sequence ownership. It submits 64 identical 1024-particle/64-step batches on the existing cadence, records exact per-batch seed, grid digest, observed completion latency and remote cohort count, then prints results outside the timed window. Control and loop-work histograms use powers-of-two upper bounds (1 to 4194304 us plus overflow); loop-work excludes delay(1) and serial polling. Counts for rendered frames, successful audio writes/errors, sampled minimum free heap and radio/peer counters are window deltas; panel maxima printed after end are lifetime metrics. No DMA underrun measurement. Profile-specific instrumentation overhead is not yet measured. Reboot for a fresh radio-off run and changed peer profile; use separate runs with same audio mode, repeat at least three times. A disconnected peer must preserve digest equality. Peer readiness and initial negotiation may affect early batches; do not discard these silently. Capture raw serial logs at 115200 and preserve exact firmware digest, board and setup; never capture provisioning keys. Physical execution NOT RUN.

Offline capture utility (no serial access or device writes): `python bench_capture.py radio-off.log s3.log c3.log mixed.log --firmware-sha256 <64-hex-digest> --board "<actual CYD revision>" --output results.json`. Run separate comparisons per firmware and board, record node identities in log filenames/setup notes. This outputs JSON and CSV with exact batch latency percentiles (nearest rank), histogram upper-bound percentiles, observed frame rate, cohort/peer counters, and digest matches against a complete radio-off reference of the same audio mode. A missing reference, incomplete capture or digest disagreement returns exit 2 after writing evidence; malformed/truncated streams are rejected. Digests are noncryptographic equality witnesses, not independent full-grid verification. Logs are operator-supplied and do not prove provenance. The unit tests exercise synthetic log fixtures only; actual physical logs remain absent.

M6c timing refinements (serial schema 2): window histograms also cover physical touch-read duration, combined render/transfer slice, frame completion period, audio render/mix computation, I2S write duration and audio-task start period. Histograms are accumulated only in the opt-in diagnostic build; audio bins are lock-free 32-bit atomic counters, snapshotted without resetting the running task. Audio boundary attribution is approximate (cross-task counters are sampled, not a synchronized event log); frame/period samples may straddle start/end. I2S write duration includes blocking/backpressure and is not a DMA underrun measure. Loop-work includes input, compute, render and transport service, not deliberate delay or serial polling; audio period includes scheduling and I2S pacing. No sample-level audio sync claim. Compare only matched runs from the same diagnostic firmware. Older schema-1 logs are deliberately rejected by the schema-2 parser. Timing instrumentation overhead and actual device evidence remain unmeasured.

## M7: trusted-LAN UDP comparison (engineering candidate)

Dedicated serial-only profiles `cyd-lan`, `s3-lan-worker`, `c3-lan-worker` use WiFiUDP and exactly the M2 512-particle/64-step seeded job and 60 ms budget, same 28-byte request/144-byte result and local verification. No panel/audio, no change to existing ESP-NOW firmware. This is a small-payload matched transport comparison, NOT a large-payload/fragmentation benchmark. Build with `pio run -d cluster/firmware -e <profile>`. No boot connection; credentials and peer are explicitly provisioned after boot, no flash persistence by application. Only use an isolated trusted WPA2 personal test LAN with no Internet, guest clients or port forwarding. UDP source IP/port and epoch filtering is NOT cryptographic authentication; do not use untrusted networks. No automatic flashing.

At 115200 baud, enter `join <ssidhex> <pskhex>` on each board (hex encodes printable ASCII SSID 1–32 bytes and WPA2 passphrase 8–63 bytes; use private local terminal, not logs or shared commands). Wait for LAN ready IP. Set each peer to the other board's reported address: `peer <IPv4> <epoch8>` with same fresh nonzero 8-digit hex epoch on both; reboot both to change peer/epoch. UDP port 42001 both ends. Enter `remote` on worker then coordinator; inspect `status` for ready=1, then `run` on coordinator; `stop` ends launches. `local` disables send/receive application processing without turning off radio; `remote` enables again. `reconnect` requests association after link loss; same in-memory epoch/sequence persists unless rebooted. For radio-off reference reboot coordinator and run without join. For radio-on local reference join but leave local. Use the same XY and number of consecutive seeds in each mode; capture job lines privately (omit provisioning commands) and before/after status. Compare elapsed_us vs verify_local_us, p50/p95/p99/max, local/remote count, retry and useful application bytes, heap; note verify and logging occur outside timed completion but affect cadence. No physical data yet. Link-layer bytes/airtime and one-way latency are not measured. WiFiUDP library allocates/buffers datagrams; this profile is intentionally not an audio-task transport. Record AP/channel/security/board/clock and repeat steady runs before comparing ESP-NOW.

Native `cluster_lan_test` exercises exact endpoint/epoch, length/version gates, credential parsing boundaries and matched compute through the admission gate. Native socket tests from M2 remain separate. M6 exact-head native/extracted Windows and nine target profiles green; [Windows evaluator](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46084243894/artifacts/15237898534), [benchmark firmware UNAPPROVED](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46084243924/artifacts/15238421797). BLE, larger UDP payloads and physical tests remain open.

Offline matched serial capture: extract exactly the first N consecutive job lines from a single run starting seed 1, after stopping; retain original private logs and metadata separately. For example, `python transport_capture.py --count 60 --warmup 10 --out comparison.json local=local.txt espnow=espnow.txt udp=udp.txt` (all captures must have same XY and workload). First capture must be all local. Rejects wrong counts, failed local verification, repeated or missing seeds, malformed rows, mismatched digests. Nearest-rank p50/p95/p99/max and remote/local split are reported after warmup. The result payload count is application data, not link-layer goodput; no rate inferred without elapsed capture window. Test: `python transport_capture_test.py`. No hardware or serial access by this utility.

## M8: opt-in BLE compact modulation (experimental)

`cyd-ble-field` uses the real standalone VECTORFIELD engine and existing touch/DAC/display adapter. BLE GATT writes contain a 16-byte little-endian versioned epoch/sequence/XY message (no gate, patch, flash or audio arm); application read acknowledgement is 12 bytes. One latest-value queue, 250 ms receive-time lease, monotonically increasing nonwrapping sequence per boot epoch. Remote XY is averaged with local XY; stale, disabled or disconnected state returns to local. Audio still needs local serial arming and physical touch, with existing BOOT/mute. No Wi-Fi/ESP-NOW coexistence. The BLE library owns internal memory and may block or fail initialization: no hard real-time claims.

**Physical NOT RUN; do not flash without owner authorization.** Generic peripheral init is intentionally separate from the directed-peer Diviner experiment that failed on physical Bluedroid. Public MAC allowlist and epoch are not cryptographic authentication; isolated laboratory only. Do not use this as a secure pairing mechanism. Unauthorized or second connection latches BLE modulation blocked until reboot. No radio init at boot.

1. On S3 or C3 serial client (115200), send `ble`; record printed public address (remove colons for provisioning).
2. On CYD, send `ble 1234abcd CLIENTMAC12` using a fresh nonzero eight-hex epoch and exact client public address; record CYD address. Send `remote` to permit modulation. On auxiliary send `connect CYDMAC12`. No automatic connect/advertise restart. Confirm both status.
3. Client `run` sends 32 deterministic XY values sequentially, reads application ACK until applied or a nominal 1s elapsed limit, then pauses 100ms; reports measured write-to-modulator-acceptance-read elapsed time, not RF RTT, render latency, or physical contact latency; tracks timely vs late acknowledgements separately. Arduino client GATT APIs may block beyond nominal deadline; client is serial-only, no audio task. Keep logs of failures and actual negotiated MTU. Compare CYD status/heap and local touch/render/audio with radio off and active; no measured results yet. `local`, `mute`, BOOT, client `disconnect` and disconnect/lease expiry are explicit safety checks. To rejoin, CYD `advertise` then reconnect, preserving sequence watermark. Reboot both for new epoch. No actual target runtime or radio success inferred from compilation.
4. Native `cluster_ble_test` exercises malformed frames, ack lengths, epochs, generations, sequence monotonicity, queue age/lease, wraparound and matched XY blending. Also tests the actual VECTORFIELD core with blended XY, local fallback and independent gate. No BLE stack emulation.

Advisory evidence: [Diviner owner baseline](https://github.com/techrote/cyd-diviner/blob/771064d419c4391f8252a31107606f4a801e8186/evidence/WIRELESS-FINDINGS.md) and [consumer RAG PR #12](https://github.com/techrote/cyd-SYNTH-VIBEWARE/pull/12) report ESP-NOW application echo mean 12.273–18.338 ms (p99 histogram upper bound 65.536 ms), S3 SoftAP UDP goodput ~138–151 kB/s for the tested 1024-byte traffic, and BLE startup failure with no usable GATT measurement. Scalar S3 savings from the tested Diviner kernels do not by themselves justify synchronous offload; favour persistent asynchronous workloads and local rendering. These are separate physical-reported Diviner observations, not HYBRID qualification. Keep failed runs and scope caveats.

### M9 asynchronous spatial snapshot experiment (native)
Independent 32x24 8-bit persistent advection/diffusion field; 788-byte self-contained UDP datagrams (20-byte header, 768 cells), no per-frame RPC or retransmission. Receiver rejects old, duplicate, malformed or foreign epochs, detects corruption (FNV is not authentication), expires after 200 ms, and preserves sequence across disconnect. Local simulation runs independently. The native receiver compares accepted states with a deterministic reference and exports the last remote snapshot through the actual RGB565 row renderer as PPM. Native loopback only, not wireless latency, physical display, or compute benefit evidence. New format is NOT admitted by ESP-NOW or existing job codec. Reboot both with a fresh nonzero epoch before sequence reset. Trusted isolated LAN only for future target adapter.

Build with existing CMake. Run cluster_spatial with no args for deterministic tests; run spatial_udp_test.py with the executable path for separate-process clean and injected blackout/duplicate/reorder experiments. Manually use receive 42000 42001 clean field.ppm, then send 42001 42000 clean unused.ppm in a second process within the receiver's finite test window. No audio changes or shared platform dependencies.

M9b target profiles: cyd-spatial (visual only), s3-spatial-worker, c3-spatial-worker. Same pinned framework as earlier LAN profiles. Serial setup as LAN diagnostic but port 42002; provision both endpoints with same fresh epoch and explicit peer IP, use remote to enable, local to stop, status for counters and measured maxima, reconnect for manual reassociation. Worker advances persistent field every 50 ms; sends latest full snapshot with no ACK. CYD renders 8 rows per slice, local field compute suspended during fresh remote operation, resumes from its own frozen state on expiry or touch override (not a phase-continuous state handoff). Hold touchscreen to select local field. Partial remote frame can be abandoned when local override begins; next complete frame replaces it. 20 Hz offered snapshot rate = 15,760 application bytes/s before network overhead; not observed goodput. No audio in this separate experiment; all earlier audio profiles unchanged. Endpoint + epoch and checksum are not authentication. No Internet or untrusted network. Reboot both before changing epoch. Target compile and physical tests separately gated, no flash authorization.

## M10 — matched spatial kernel and delivery repair
Run `cluster_spatial_bench.exe` (or native `cluster_spatial_bench`) to time 512 identical
local and worker field steps, worker encoding, receiver validation/copy, and 256
full 320x240 RGB565 rasters per arm. Every snapshot and every pixel must match;
render ordering alternates. Timings are steady-clock host nanoseconds, not
socket or target measurements. No delay or radio is included. The native CI
build, sanitizers and extracted evaluator run this benchmark; the Windows
package requests static MSVC CRT, refuses external MSVC runtime imports,
records PE dependencies/source commit/tree and verifies extracted file hashes.
Python is still needed for automated multi-process replay.

For physical **visual-only** comparison, on the existing spatial profiles use
serial `meter` before and after each steady capture window (e.g. 30 seconds,
three repetitions). Reboot between radio-off and radio-on arms; run radio-off
standalone, radio-on local, fresh remote from S3, fresh remote from C3, and an
intentional outage/rejoin. Do not change configuration or touch during a steady
window; test touch override separately. Record both endpoint serial logs,
board/clock/source/artifact and wall interval. Subtract cumulative 64-bit
compute/IO/encode/render counters between samples; divide by elapsed
`uptime_us` for measured loop duty fractions. `steps` counts executed local
field advances, `deferred` counts skipped local steps while remote is fresh,
`io_calls` includes empty socket polls, and frame counters count completed
(not merely started) LCD frames. Accepted payload bytes = RX delta × 788.
Report aborted frames separately if needed from existing status counters.
Heap/minimum/largest are snapshots or lifetime minima, not interval minima.
The `meter` output itself incurs serial overhead at boundaries. These windows
compare actual work rates, not identical states; use the native matched
benchmark to verify exact algorithm/render equivalence. No audio or one-way
source-age measurement in this visual profile; no physical results claimed.

## M11 — persistent reaction/diffusion comparison

Opt-in `cyd-reaction`, `s3-reaction-worker`, `c3-reaction-worker` reuse the
visual-only LAN adapter and its serial provisioning, meter, touch-to-local and
200 ms arrival-freshness lease. The original spatial builds are unchanged in
model. A 64x48 two-species Q12 lattice advances four substeps per 50 ms requested
tick (12,288 cell updates), with periodic deterministic seeding and 2x2
projection into the same 32x24 indexed snapshot. This is a different visual
model, not a claim of faster execution or equivalence to the light field.
The wire header byte 3 now identifies model 0 (old trail field) or 1 (reaction);
each receiver explicitly admits only its compiled model. Payload remains
788 bytes; old receivers reject variant 1. No automatic negotiation, no
cross-model fallback, no cryptographic authentication. Fresh epoch required
on reset; isolated trusted LAN only.

`cluster_reaction.exe` runs deterministic replay, or the same
`send|receive own-port peer-port clean|faults output.ppm` syntax as spatial.
`cluster_reaction_bench.exe` compares exact local/remote states and raster,
timing compute, encoding, decoding and raster separately. Use the existing
spatial UDP harness with this executable. Native fixed trajectory fixture
covers the first 512 snapshots (FNV-1a 1979313773), bounds, model rejection,
duplicates and lease recovery. Local simulation stays dormant while remote is
fresh, so switching is not phase-continuous. The heavier fixed tick may
affect target input latency; physical timing and memory margin NOT RUN.

Measure separate matched radio-off, radio-on local, S3, C3, and outage arms
using the existing meter. Compare within variant 1 at the same attempted
rate and frame configuration; do not compare unlike visual quality as a
speedup. Report executed/deferred steps, actual completed frame rates, heap,
socket costs, worst-case compute/IO durations and touch behavior. No audio in
this profile, no flash authorization. Original code, no imported reference
implementation. M10 package pin remains available for independent QA.

### Cooperative reaction scheduling (M12)

Reaction targets service at most one eight-row lattice slice (512 cell updates) or one 768-cell projection per main-loop iteration, 25 slices per completed tick. The bank switch is constant-time. Touch, UDP receive and display strips continue between slices; the existing loop delay remains. The requested 50 ms period is not guaranteed, and there is no catch-up burst. The CYD pauses pending local computation while fresh remote snapshots are selected, resumes on local override/expiry, and never publishes partially calculated local pixels or sequence numbers. Local/remote phase continuity is still not promised. The worker sends only at complete-tick publication. Lightweight spatial profiles retain synchronous advancement. `SPATIAL_SLICE_V1` (reaction profiles only) adds cumulative slice count, maximum individual compute slice duration, maximum summed compute duration per completed tick (excludes pauses), pending flag and accumulated pending compute duration. Existing `compute_us` includes partial-tick work; `steps` counts only complete ticks. A matched capture should begin/end with pending state accounted for, and distinguish work duration from elapsed tick cadence. Bounds are algorithmic, not physical latency guarantees. No audio on these profiles.

### M13 — offline boundary-aware CYD panel capture

Use the **existing M12 firmware**. For each arm, record precisely two complete
`meter` groups on the CYD, with a steady interval between them. Do not include
provisioning secrets. Repeat the full comparison with fresh output names. Example:

    python spatial_capture.py --model reaction --firmware-sha256 <actual-image-digest> --board "<actual CYD revision>" --out repetition1.json radio-off=off.log radio-local=local.log s3=s3.log c3=c3.log outage=outage.log

A radio-off reference is required; other arms can be omitted if not run. Use
`--model light` for the original spatial model (no slice row). Record exact
auxiliary firmware, board, clock, AP and outage event timestamps separately.
The tool checks schema/order, counter monotonicity, status consistency, slice
phase and incomplete capture. No output file is overwritten. Log SHA-256 is
retained, not raw log contents. Radio and node labels remain operator-reported;
boundary flags do not prove continuous state. Outage is a label, not a proven
fault event. Do not compare different models or firmware in one invocation.

Window compute duty uses actual cumulative work delta; completed-tick mean uses
compute delta + starting pending compute - ending pending compute, so a partial
tick is neither double counted nor attributed to the wrong completed tick. A
zero-completion window has no mean. Slice maxima and heap minimum are lifetime,
not interval statistics. Frame cadence counts completed frames; accepted bytes
are application payload only. Serial boundary skew remains; no interval p95/p99,
RF loss, one-way age, audio, or actual speedup claim. Native matched-state tests
remain distinct. Synthetic parser tests: `python spatial_capture_test.py`.

### M14 — reaction-diffusion desktop audition

Windows actual-core visual-only viewer: run `cluster_reaction_view.exe` for
standalone, or run `cluster_reaction_view.exe --udp 42000 42001` and then
`cluster_reaction.exe send 42001 42000 stream unused.ppm` in another terminal.
The worker sends 2400 snapshots at a requested 50 ms interval (about two
minutes); Ctrl+C stops it. Both endpoints are localhost only, fixed test epoch
91; restart both to reset sequence. The dummy output path is unused by send.
Hold the left mouse button to select local, release to resume remote, R to
toggle remote, Esc to exit. Loss triggers local computation immediately;
a 300 ms presentation-only blend on autonomous source changes avoids a
one-frame jump, including reversals, but is not simulation phase continuity.
Explicit local overrides are immediate. No audio or device radio in this
viewer. The embedded profiles and M13 measurement schema are unchanged.

The previous M13 Windows workflow omitted the spatial capture scripts despite
running their source tests; the new archive includes and exercises both. New
extracted GDI smoke and actual separate-process blackout/recovery smoke are
required, along with the portable handoff CTest, ASan/UBSan and PE checks.

## M15 native checkpoint experiment (not a deployed transport)

`cluster_checkpoint` proves exact completed-tick restoration of the 64x48 Q12 U/V planes and subsequent trajectory. The full state is 12,288 bytes, in twelve 1,048-byte datagrams (24-byte header plus 1,024-byte payload). Explicit little-endian format; separate RC v1 namespace from SF snapshots. Reassembly retains one sequence, rejects foreign epoch, corrupt/truncated/duplicate/older fragments and mixed digests, expires incomplete assembly 200 ms after first fragment, and cannot restore until complete. Full concentration validation precedes mutation; a suspended local tick may be discarded. Caller must authorize endpoint and decide whether completed checkpoint is sufficiently recent. FNV detects accidental corruption, not malicious modification. No clock alignment or automatic failover admission yet. No use over ESP-NOW.

Run `cluster_checkpoint.exe` in the Windows package (or CTest `cluster_checkpoint`); this is a native in-process codec and trajectory proof, not a socket/radio test. It checks 60 restores and 1,140 future ticks against independent uninterrupted reference, plus invalid/expired assembly. The timing counter named host_200_roundtrip_us measures only encode/reassembly/restore in one process, not RTT; export excluded, whole-state hash currently repeated per fragment. Native receiver sizeof is 12,308 bytes on tested host, not target peak heap. Offered bytes if one checkpoint per second: 12,576 B/s plus existing 15,760 B/s projection stream; every 20 Hz tick would require 251,520 B/s plus projection, before UDP/IP/radio overhead. Prefer occasional checkpoints and bounded catch-up, compare against presentation-only handoff. Next: actual UDP loopback transport, freshness policy, loss/recovery and bounded integration with viewer; only then consider opt-in embedded measurement. M14 viewer and existing target profiles unchanged. No flash or owner physical evidence.

### M16 — native checkpoint transfer and bounded continuation
Run `python cluster/checkpoint_udp_test.py build/cluster/cluster_checkpoint_udp`
(or use the extracted Windows exe and script). Separate localhost processes
exchange existing SF presentation frames and RC checkpoint fragments; the
worker sends reverse-order fragments, with a prepared whole-state digest.
Fault mode duplicates fragments, leaves checkpoint 18 incomplete and omits
snapshots 20–50. Receiver immediately admits complete fresh checkpoints into
the suspended local engine (no rollback), retaining the last admitted state
across incomplete transfers. Admission requires a fresh presentation sequence,
a checkpoint no more than 20 ticks behind it and a completion lease of 200 ms.
Loss catch-up uses at most one lattice slice per loop, only toward the last
observed snapshot and only when the saved state is at most 600 ms old and
20 ticks behind. Otherwise continue ordinary local simulation. Native harness
compares complete local states against an independent reference; no radio,
hardware, absolute clock/phase synchronization or speedup claim. This is a
headless experiment; graphical and embedded integration remain separate.

### M17 — checkpoint-aware desktop audition

Start the viewer, then the localhost worker in another terminal:
```
cluster_reaction_view.exe --udp 42000 42001
cluster_checkpoint_udp.exe send 42001 42000 stream
```
The worker sends 2400 snapshots at a requested 50 ms interval, full checkpoint
every five ticks; Ctrl+C stops it. This high checkpoint rate is a desktop
experiment, not an RF bandwidth recommendation. Original snapshot-only worker
remains supported. Restart both to reset test epoch/sequence. Hold left button
or R to select local immediately; release/re-enable requires a new snapshot
above the retained sequence high-water. During manual override drain but ignore
incoming packets and invalidate partial checkpoint assembly. Autonomous loss
freezes the last presented image during bounded exact catch-up, then resumes
local with visual blend; otherwise normal standalone fallback. Footer shows
CATCH-UP and saved checkpoint sequence. The Windows timer executes up to 32
individual lattice/projection steps per dispatch, stopping after a soft 2 ms
compute budget; checkpoint restore, network and painting are separate. No
hard real-time claim. Headless policy fixture validates frozen presentation,
exact continuation, manual override and fresh-only rejoin across timer wrap.
Extracted Windows smoke tests both snapshot-only and checkpoint streams,
the latter with synthetic hold/release and actual UDP blackout. Visual only;
no physical RF, board, audio or display timing claim.

### M18 opt-in embedded checkpoint audition
Build only (no flashing): `pio run -d cluster/firmware -e cyd-reaction-checkpoint -e s3-reaction-checkpoint -e c3-reaction-checkpoint`. Previous reaction profiles unchanged. Use two explicitly owner-authorized devices on a trusted isolated WPA2 LAN; the new profiles use UDP port 42003 on both ends. Radio off at boot. On each serial console at 115200, `join <ssidhex> <pskhex>`, then after connection `peer <other-device-IPv4> <same-nonzero-epoch8>`, then `remote`. Do not publish credentials. Peer selection explicitly resets the CYD local simulation into the new session. `local` or hold touch selects local; release/re-enable requires fresh snapshots. Worker `local` cancels transmission; restart with `remote`. `meter` reports cumulative timing maxima/sums, heap, checkpoint and recovery counters. Power loss/reboot requires a new shared epoch; endpoint filtering and FNV are not cryptographic authorization.
Worker exports one immutable checkpoint every ten completed ticks, sends at most one fragment per poll with 5 ms minimum spacing and 150 ms deadline, aborts on send error/disconnect, prioritizes presentation snapshots. No retransmission; 12 x 1048 B every nominal 500 ms plus 788 B x 20/s = 40,912 B/s offered application load, excluding radio overhead. This is a configured ceiling at nominal update rate, not measured goodput. CYD processes at most four datagrams and one cooperative step per loop, renders eight rows per draw, reuses the portable controller and keeps the last complete admitted checkpoint. Full export, checksum and restore remain bounded but not sliced. Measure their worst-case cost before acceptance. Display can retain an in-progress strip frame until next full refresh. No audio in this experiment. Target build does not establish RAM availability under Wi-Fi, checkpoint recovery latency, or physical benefit.
