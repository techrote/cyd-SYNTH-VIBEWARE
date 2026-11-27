# CYD SYNTH VIBEWARE

**An experimental touchscreen audiovisual instrument and firmware laboratory for the ESP32-2432S028R Cheap Yellow Display (CYD).**

Sister project to [FM-1-VIBEWARE](https://github.com/techrote/FM-1-VIBEWARE). Develop original, playable instruments, spatial composition systems, artificial acoustic ecologies, procedural visual synthesizers, and distributed performance tools. Treat graphics and touch as first-class parts of an instrument, not decoration on a conventional synth. Visual-only instruments are legitimate.

## Start here

- [Programme and implementation issues](https://github.com/techrote/cyd-SYNTH-VIBEWARE/issues)
- [Engineering worker protocol](AGENTS.md)
- [Architecture](docs/ARCHITECTURE.md) and [hardware contracts](docs/HARDWARE.md)
- [Diviner performance characterisation](docs/PERFORMANCE.md) and [wireless compute clusters](docs/CLUSTER.md)
- [Instrument catalogue](docs/CATALOGUE.md) and [development roadmap](docs/ROADMAP.md)
- [Worker roster](docs/WORKERS.md) and [schedule-ready mandates](docs/SCHEDULES.md)
- [Qualification and owner delivery](docs/QUALIFICATION.md)

## Initial instruments

| Instrument | Design direction |
|---|---|
| PHASESPACE | Coupled oscillators and phase geometry: touch the actual sound-generating dynamics. |
| CHRONOGRID | Spatial, interacting clocks, polyrhythms and touch-painted events. |
| XENOBIO | Bounded artificial-life simulation with causal acoustic ecology. |
| PHOSPHOR | Recursive visual feedback, expressive fullscreen gestures and optional sonification. |

Future directions include VECTORFIELD, PATCHSPACE, MEMBRANE, GLYPH, LITHOS, MORPHOGEN and SWARM.

## Reference lineage (read-only)

| Reference | Intended use |
|---|---|
| [cyd-diviner](https://github.com/techrote/cyd-diviner) | Actual CYD HAL and PlatformIO configuration, fullscreen visuals, touch, SDL2, GPIO26 audio and USB PCM/VLC. |
| [cyd-MHAC](https://github.com/techrote/cyd-MHAC) | [Audio #2](https://github.com/techrote/cyd-MHAC/pull/2) and [visual #4](https://github.com/techrote/cyd-MHAC/pull/4): bounded DSP, gesture exchange and adaptive rendering. These Draft/HOLD PRs are candidates, not automatically accepted dependencies. |
| [FM-1-VIBEWARE](https://github.com/techrote/FM-1-VIBEWARE) | Instrument lab structure, worker model and delivery contract. |
| [FM-1-CYDaux](https://github.com/techrote/FM-1-CYDaux) | Firmware profiles, MIDI, Wi-Fi transport and simulated interaction. |
| [FM-1-MHAC](https://github.com/techrote/FM-1-MHAC), [ZaaggenZ-FM1](https://github.com/techrote/ZaaggenZ-FM1) | Portable synthesis ideas and embedded DSP. |
| [MachineHumanAudioCybersemiotics](https://github.com/techrote/MachineHumanAudioCybersemiotics) | Research inspiration for cybernetic and semiotic instruments. |

The repositories remain independent. Inspect exact revisions, licenses and notices before adoption; access is not a reuse license. Prefer original code where rights or compatibility are unclear. No upstream writes.

## Delivery model

Develop portable C++ instrument cores, real CYD adapters and native Windows/Linux interactive evaluators. Deliver individual playable firmware candidates before attempting a collection launcher. Keep audio deadlines isolated from rendering, use bounded memory and queues, and design for one touch contact. Test the same core on host and target; host audio/screenshots are not hardware evidence.

**Status: programme bootstrap only.** No firmware, CI qualification, physical tests, or schedule activation is implied. No repository-wide license decision is asserted. Keep experimental implementation PRs Draft/HOLD pending independent qualification and owner approval. No automatic flashing or merges.

## Distributed architecture and performance

Owner's spare ESP32-S3/C3 modules and dual-CYD configurations are explicit targets for optional wireless compute offload. Local standalone instruments remain complete. Start with [Diviner physical characterisation #8](https://github.com/techrote/cyd-SYNTH-VIBEWARE/issues/8) and [heterogeneous cluster experiments #9](https://github.com/techrote/cyd-SYNTH-VIBEWARE/issues/9); compare real end-to-end performance and failure behavior before adoption.
