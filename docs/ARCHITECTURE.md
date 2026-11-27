# Architecture and implementation routes

CYD-VIBEWARE is a collection of independently playable applications, not a mandatory monolithic firmware. Platform assumptions are hypotheses until verified on the actual target. Design around the CYD's touch/display/synthesis intersection rather than porting FM-1 front panels.

## Routes

- Standalone audiovisual instrument: portable C++17 core + thin CYD adapter and native evaluator; primary route for SYNTH, GEN, CYBER.
- Visual instrument: same touch/render lifecycle without compulsory audio. The VISUAL lane owns this route.
- Distributed performance application: standalone useful behavior with optional MIDI, OSC, peer or FM-1 companion. Capabilities verified per device/firmware.
- Heterogeneous compute cluster: optional wireless S3/C3 workers and dual CYD peers executing coarse simulation/DSP/event jobs. Keep local audio clock, touch, display and fallback. See CLUSTER.md (#9) and PERFORMANCE.md (#8); no round-trip inside audio callback. Compare measured end-to-end benefit before adoption.
- Desktop evaluator: native SDL2 (Windows/Linux) driving actual portable cores, not a parallel approximation. Headless scripted execution produces real PCM and RGB565 frames; interactive host maps mouse to single contact and clearly labels simulated hardware.
- Collection: only after independent applications mature and measured resource costs justify common launcher. Keep standalone builds.

## Minimal shared contracts (prove through one working consumer)

- Instrument lifecycle: initialize with explicit seed/configuration; reset; enter/exit; safe mute; snapshot/save/load; bounded update and rendering. No allocations in audio callback or unbounded catch-up.
- Audio: explicit negotiated rate, channels, fixed block or bounded block lengths, frame count, headroom and saturation. Initial candidate 24 kHz mono, based on Diviner and cyd-MHAC references; qualify actual timing. Use identical DSP in native and target except explicitly documented backend/precision differences.
- Event/control: monotonic timestamp/sample position, fixed-size queues, sequence/drop policy, down/move/up/cancel, parameter smoothing, gesture replay, transport/panic semantics. Release on lost contact. Overflow must not drop a safety mute permanently.
- State exchange: single-writer ownership and coherent immutable snapshots or proven bounded lock-free exchange. No unsynchronized shared mutable objects, callback locks or display access from audio thread. Visual→audio mappings pass bounded events/smoothed state; audio→visual passes decimated features/snapshots. Clearly specify units, ranges and version.
- Simulation: fixed timestep, bounded catch-up, explicit seed, fixed-capacity pools; visual presentation may interpolate but cannot modify authoritative state.
- Rendering: RGB565 strip/tile sink, clipping, dirty regions, bounded history, explicit palette, independent display update cadence; avoid per-pixel virtual dispatch in hot loops. Transfer buffer ownership extends through DMA completion. Adaptive quality may drop visual work, never audio deadlines.
- Persistence: versioned bounded formats, validation, atomic/recoverable saves where available, default fallback, no frequent flash writes from real-time paths; only user-initiated writes initially.
- Input: single-contact resistive touch and orientation/calibration transform; sufficiently large hit areas, fullscreen escape and reachable mute, no required multitouch. Native input supports the same event path.

Audio clock is master for event scheduling when sound is active. Simulated and rendered timelines must be distinguished; no long locks or I/O on audio deadlines. Evaluate task affinity/priorities against real peripheral constraints, not a blanket one-core-per-subsystem assumption. Prefer measured simple algorithms to speculative optimization.

## Intended layout

    platform/         target/host adapters, audio, graphics, touch, transport
    primitives/       reusable DSP, simulation, rendering (only proven reuse)
    instruments/<id>/ portable engine, input mapping, renderer, presets/scenes, tests
    desktop/          SDL2 evaluator and native host adapters
    qualification/    replay, independent checks and evidence
    catalogue/        evaluator manifests, media and owner guides
    tools/            build, package, benchmark, diagnostics
    launcher/         deferred collection integration
    docs/             concise engineering contracts and planning

PlatformIO candidate: ESP32 Arduino framework and pinned libraries/toolchain derived from reviewed Diviner config. Native C++17/SDL2; CMake or PlatformIO native route chosen by reproducible Windows/Linux results. Do not inherit unrelated network or UI code. CI initially actual native builds/tests, sanitizer jobs, CYD compile/link, map/size receipts and portable Windows package. Separate diagnostics builds if necessary.

## Reference and provenance

Inspect exact source revisions from README and pin reviewed dependencies. Diviner's HAL, USB PCM and native replay and cyd-MHAC's actual DSP/XY/rendering are high-value candidates, not approved source drops. Keep source/rights ledger: repo, commit/tree, path, license/notices, modifications, dependency and toolchain versions, distribution requirements. No unreviewed licensing assumptions. References remain independent.

One issue-scoped branch and Draft/HOLD PR per active instrument; main accepted reference. Local adapters unblock creative lanes while shared contract stabilizes. Integrate by exact dependency SHAs and review shared changes against real consumers.

## Performance-driven partitioning

Use Diviner's physical profile (#8) to identify bottlenecks, then compare local and remote implementations under matched output/quality, including serialization, radio, queue, latency and failure costs (#9). Keep portable local paths; no mandatory network dependency. Native profiling is complementary, not physical evidence.
