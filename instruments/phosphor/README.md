# PHOSPHOR M1 — gesture-painted optical resonance

Original TOUCH instrument for #6. Six feedback regimes and harmonic spectra: Ember, Glacial, Orchid, Verdure, Brass, Afterimage (scene order 1–6). The 80×50 indexed feedback plane is warped, diffused and decayed at 30 sample-clock ticks/s. Each quadrant's measured intensity governs one smoothed triangle voice; unpainted planes produce silence. Gesture speed changes the injection footprint. Two 4000-byte planes, 8100-byte host Engine; no full RGB565 buffer. Actual row rendering at 320×240.

Controls: top 20 pixels choose one of six scenes (also deliberate restart after Panic); center 200 pixels paint/drag, release stops injection; bottom left 120 pixels freeze/thaw feedback, next 120 clear, right 80 panic. Drag into panic also mutes. Native contact lease expires within five simulation ticks (nominal 166.7 ms, tick quantized). No multitouch or pressure assumed. The single-owner reference intentionally executes field updates inline with the sample clock for deterministic replay; it is NOT a qualified hard-real-time callback. M2 must separate bounded field stepping and immutable visual/feature exchange from DSP deadlines; verify audio endpoint, touch capture and target adapter before claiming playable interactive delivery.

## Reproduce
    g++ -std=c++17 -O2 -Wall -Wextra -Werror replay.cpp -o replay
    ./replay gcc
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer replay.cpp -o replay-san
    ./replay-san clang
    diff -qr gcc clang

Actual GCC 14.2 and Clang 17 ASan/UBSan PASS, six 8-second 24 kHz mono WAVs and six 320x240 PPMs byte-identical. 1,152,000 paired samples per compiler plus equally long blank-field ablation. Peak counts 7363,8192,8192,6474,7285,8192; all scenes have >191000 nonzero samples. 240 ticks per scene, freeze/clear/contact lease/panic/restart and row guards PASS. Original code only; read-only concept references Diviner main 7b1b37eec6ac126f334a0b008d91135071ea60fa and cyd-MHAC main 6549f280672a5eb5cf9478e32f002454599d0c40 README; no source/assets imported, no license assumed. Base bf08fb5a38a7fa692026af3bfb08943fb72506ef.

Native Linux reference PASS; Windows interactive, target build, physical and independent NOT RUN. Local git clone blocked by DNS, pio absent. M1 does not supersede independently qualified XENOBIO M4R or M5 author target pin; P10 embedded audio lifecycle remains HOLD pending PORT/PLATFORM coordination. No firmware, no shared changes, no merge. Next: real interactive Windows evaluator, split simulation/audio ownership, then CYD target adapter and qualification.
