# XENOBIO M1 — acoustic ecology core [Draft/HOLD]

Original standalone instrument prototype for #5, no upstream source copied. A finite 16×10 nutrient lattice, 24 organism slots and eight triangle voices. At each 20 Hz sample-clock tick organisms seek food, compete for occupied cells, consume energy, split or die. A capped subset of actual movements, births and deaths produces short notes: horizontal position, species and event type determine pitch; birth has a longer envelope. Rendering uses actual food, species, organism energy and touch radius, in 320×240 RGB565 rows. No dynamic allocation or I/O in the core, but the ecology update is synchronous in the sample loop; worst-case embedded deadlines are NOT RUN. The simplified comparator disables occupancy penalties and births, so it is not a one-factor ablation.

## Replay controls (adapter pending)

Header: six equal scene regions, left to right sparse, dense, balanced, alternating nutrient growth, disturbed, recovery. Main surface: food level is green intensity; orange/cyan/magenta dots show species and size shows energy. Footer: x 0–79 feed; 80–159 attract; 160–239 disturb (remove nutrients and repel); 240–279 reseed; 280–319 panic. Field moves renew a four-tick lease; release cancels immediately. Panic stops samples and simulation; fresh scene/reset input restarts. A future adapter must provide real down/move/up/cancel lifecycle and coherent cross-thread snapshots; M1 is a single-owner core, not a thread-safe interface. No hardware pressure or multitouch assumed. Header/footer are unlabeled color blocks in this initial renderer.

## Reproduce (from this directory)

```sh
mkdir -p media-gcc media-clang
g++ -std=c++17 -O2 -Wall -Wextra -Werror replay.cpp -o replay
./replay media-gcc
clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer replay.cpp -o replay-san
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./replay-san media-clang
diff -qr media-gcc media-clang
```

Both builds executed locally: PASS six 12-second scenes (288000 samples each), exact PCM across 128/37-sample partition loops; matched touch/no-touch and simple comparator differ; lease and held-drag panic, bounds, output limit, row guards, 12000 direct ecology ticks (600 simulated seconds, not long-run audio) PASS. Eight WAVs and six PPMs byte-identical across compilers. Peak 4208 PCM counts across recorded WAVs. Native sizeof Engine 696 B. Four-event cap per tick; eight voices. Scene final populations 4/24/21/24/13/24; birth counts 0/2/19/12/24/21 and deaths 0/0/10/0/27/0. Alternating resource growth is a configured forcing, not evidence of spontaneous oscillation. No subjective audition or learning claim.

Core blob 6953826440779ef79a53adc76d63b2829079ed88; replay blob bd315ebd1de6522aaea75fcd44d33e5b0a4c34f6. Reference inspiration read-only: Diviner README at 7b1b37eec6ac126f334a0b008d91135071ea60fa (CYD profile) and cyd-MHAC PR #2 description at 6db2b302034d4253d7f1f115de7e41b923560449 (bounded voices). No reuse license asserted.

Native author replay PASS; Windows native/interactive, CYD compile/link, independent QA, physical and distribution authorization NOT RUN. No firmware delivered. Next: instrument-local graphical/audio adapter and scripted contact lifecycle, labeled controls, sample-clock tick worst-case profiling and actual target build; coordinate with PLATFORM/QA before adoption of shared primitives.
