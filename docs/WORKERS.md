# Scheduled engineering roster

Six workers are recommended initially, following FM-1-VIBEWARE's structure with a dedicated visual lane. Every creative worker owns implementation, actual interaction, presets/scenes, native and target adaptation, tests and documentation for its current instrument. No recurring research-only or report-only tasks.

| Configuration | Workers | Roles |
|---|---:|---|
| Minimal | 3 | PLATFORM, CREATIVE (SYNTH+GEN+CYBER+VISUAL), QA |
| Optimal | 6 | PLATFORM, SYNTH, GEN, CYBER, VISUAL, QA |
| Expanded | 7 | Optimal + HYBRID |
| Maximum practical | 8 | Expanded + ACCEL |

## Cadence (proposed only)

| Role | Hourly start minute | Issue | Ownership |
|---|---:|---|---|
| PLATFORM | :05 | #2 | Firmware, shared runtime, HAL, native harness |
| SYNTH | :13 | #3 | PHASESPACE and later sound engines |
| GEN | :21 | #4 | CHRONOGRID and composition |
| CYBER | :29 | #5 | XENOBIO and artificial life |
| VISUAL | :37 | #6 | PHOSPHOR and optical systems |
| HYBRID (optional) | :45 | scoped issue on activation | Distributed instruments |
| ACCEL (optional) | :51 | assigned bottleneck | Measured optimization |
| QA | :57 | #7 | Independent qualification and mergedown |

Offsets are local-clock suggestions, not locks or promises of concurrency. Consider global runner contention and other programmes (including FM-1-VIBEWARE and MHAC); widen intervals to 2–3 hours or move offsets if WIP, active claims or CI queue grows. Do not launch duplicate work while a prior run is active. See AGENTS.md and SCHEDULES.md.

## Mandates

**PLATFORM:** reference touch→sound/graphics path on native and real target build; minimal versioned contracts, display strips, input, DAC/USB route, bounded queues, lifecycle, shared tools and timing. Inspect actual board/toolchain source and pin review. Unblock real downstream work. No abstract SDK before one working consumer.

**SYNTH:** original synthesis engines with tactile expression and causal graphics. Start PHASESPACE. Compare algorithms with actual PCM and resource cost, author presets and gesture demos, deliver working engine, UI, build and tests. Later VECTORFIELD/LITHOS/physical modelling when qualified.

**GEN:** start CHRONOGRID, alternative time and composition structures, spatial editing, reproducibility, bounded scheduling and standalone sound. Deliver playable authored compositions, transport and stress evidence. Later PALIMPSEST/GLYPH.

**CYBER:** start XENOBIO, artificial life and dynamical systems with causal audible/visible state and meaningful interventions. Compare baselines and demonstrate several regimes; finite pools, deterministic seeds, bounded work. Later MEMBRANE/MORPHOGEN/SEMIOSPHERE.

**VISUAL:** start PHOSPHOR, original graphics, feedback, palettes, optical effects and gesture performance; bounded memory and adaptive strip output, actual rendered witnesses. Audio optional and causal if used. Later DATAMOSH/PLASMATONE; coordinate shared renderer changes.

**QA:** independently reproduce exact source and artifacts, run Linux/Windows native tests, target compile, extracted package replay, audit source/rights and physical gate; fix real defects and coordinate integration. No self-certifying authored fixes; mark provisional absent independent reproduction. Prepare owner candidates without merge.

**HYBRID (optional):** SWARM standalone peer systems, optional FM-1, MIDI/OSC, touch performance and verified transport profiles. No assumed radio/USB capability. Use rate limits, disconnect/panic, safe offline fallback and replay. Coordinate shared APIs with PLATFORM. Create one scoped issue when activated, not before.

**ACCEL (optional):** measure actual downstream bottlenecks in DSP, simulation, memory, rendering, or transfer. Compare algorithm alternatives using same workloads, output equivalence/tolerance and real timing/footprint. Separate coordinated PRs, no speculative SIMD or device performance claims. Protect sound quality and responsiveness.

## Scaling

Begin with optimal six when capacity permits. Add HYBRID once an instrument merits connectivity and shared core is reliable; add ACCEL when timing/footprint evidence identifies concrete bottlenecks. Three-worker fallback preserves independent QA; combined creative lane should progress one instrument at a time. Slow creative starts if QA backlog or Actions contention rises. Every run must leave tested code, a repaired failure, or precise blocker plus a useful alternative, not a routine status report.
