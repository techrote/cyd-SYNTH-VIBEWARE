# Ecophon CYD M1 — four-habitat acoustic ecology

Draft/HOLD. Native single-threaded core milestone, not an interactive desktop application or CYD firmware yet.

## Provenance and compatibility

Original C++17 touch-native interpretation of FM-1-VIBEWARE [Ecophon #8](https://github.com/techrote/FM-1-VIBEWARE/pull/8), reviewed exact eaf276c015fc81185076647762ddf1fd97aa7960 (README and local semantic profile; author native Windows/Linux/browser [46126318152](https://github.com/techrote/FM-1-VIBEWARE/actions/runs/46126318152) green). Reference LICENSE lookup at that commit returned 404; no source copied, no upstream writes, no PCM identity or distribution permission asserted. Prior standalone virtual FM-1 evidence is reference-only, not evidence for this port.

Retained conceptual semantics: eight agents, four bounded resource patches, continuous agent voice/energy/species mapping, replenishment, migration, resource transfer, births/deaths, seed-bank recovery, fertility/sharing/pressure, three environmental presets, frozen interaction comparator and two environment memories. Independently chosen ecological rules, oscillator tuning/waveforms, amplitude smoothing and spatial touchscreen layout; no FM-1 panel, CPU, ABI, MIDI, browser, feedback controller or remote companion protocol. The 24 s Habitat Arc specimen uses abrupt drought/recovery changes, not the upstream ramped score. Memory recalls use a 4 s ramp and manual takeover; slots persist only in this Engine session, including reset.

This is distinct from TOUCH XENOBIO's spatial mobile-organism event notes: Ecophon renders sustained agent voices and changes the whole habitat's environmental intent. No shared or TOUCH code imported.

Reference repository head inventory: FM-1-VIBEWARE main 805a64b215c50b94634925dc25afe245338a6e1b; FM-1-CYDaux e65a0b880b5c13c9aa3ce734c6e3eaa4b11fc969; FM-1-MHAC 16c698d9e244559064a9a9a22f834d36912bf498; Diviner 7b1b37eec6ac126f334a0b008d91135071ea60fa. No code adoption from these refs.

## Controls (native scripted coordinates; labels pending)
- Top 24 rows: four fresh-touch scene buttons, Mutualism / Scarcity / Seasonal Bloom / Frozen interactions. Reset uses current RNG state; explicit Engine::reset(scene,seed) for deterministic replay.
- Rows 24–79: actual four resource levels and living agent positions within their patch, color indicates species, radius energy.
- Rows 80–111: sharing slider. Rows 112–191: environmental XY (right = higher fertility, up = higher pressure).
- Rows 192–215: capture A, capture B, recall A, recall B. Recall interpolates current intent to the captured target without resetting ecological state; manual controls cancel ramp.
- Bottom: freeze/unfreeze on fresh contact, red rightmost 40 pixels Panic (fresh or held). Panic silences digital PCM, inhibits held contact until release; fresh scene button restarts. Lost contact lease 150 ms, explicit release to rearm; environmental intent persists because these are autonomous drones, not gate notes.

## Local reproducible evidence

In this directory, with GCC and Clang:
```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror replay.cpp -o replay
./replay 'audition path with spaces'
clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined replay.cpp -o replay-san
./replay-san 'sanitized path with spaces'
diff -qr 'audition path with spaces' 'sanitized path with spaces'
```

Both PASS; ten WAV/PPM files identical; 2,304,000 partition-equivalent samples; 479,684 / 576,000 different samples in Habitat Arc vs steady (matched first 4 seconds). 12,000 varied-environment ticks (120 s simulated), population accounting and bounds, row guards, peak <=16384, memory interpolation/takeover, lease, drag-to-panic and digital silence/rearm all PASS. Host Engine 344 B. Fixed eight voices, 100 Hz ecology clock, 24 kHz mono, 320x240 RGB565 row renderer; no full framebuffer or dynamic memory in core. The test exporter uses host allocation/file I/O outside the engine.

| Scene | population | births | deaths | moves | gifts | PCM peak |
|---|---:|---:|---:|---:|---:|---:|
| Mutualism | 8 | 0 | 0 | 59 | 161 | 11381 |
| Scarcity | 4 | 16 | 20 | 471 | 2766 | 7387 |
| Seasonal Bloom | 8 | 2 | 2 | 33 | 2960 | 12485 |
| Frozen interactions | 8 | 0 | 0 | 0 | 0 | 5387 |

Seed 42, 24 s each. Frozen means interactions stop, not resource replenishment or sound. WAV SHA-256: scene0 eb4bad0d74729637487cdde75543b4dcd60e1bfa74d9795903ddfc7275a6978a; scene1 63a2c12ed8622f2040610e26dfa5a64ff93b458d737895e04a53e0a40da3db15; scene2 da38ac89a70dc3cf0169a92354422795e3c431a8752635ec63bdbab0bd21bbce; scene3 fde209a98c4c6047238bbfa72dde9ef73d74787d9c69fa89390d310a067ac4b4; habitat-arc b7aea8102291ed23348f3f78917950b795c000663793437da23a30dd7ad947e7. No listening claim.

Native local PASS; Windows interactive, ESP32 target, physical, independent reproduction NOT RUN. Single-threaded core only: do not call Touch or renderer concurrently with sample(); next add local bounded input queue, immutable render snapshot, labeled native evaluator, safe host and target audio lifecycle, target profile and timing characterization. Pulse-like waveform aliasing, DSP costs, DAC/physical behavior and deployment rights unqualified. No firmware artifact, merge, flash or release.
