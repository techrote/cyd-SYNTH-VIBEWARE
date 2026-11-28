# Ecophon M2R — graphical delivery recovery and emergency drain

Draft/HOLD. Predecessor 3ee4f91c46b7e5c63f9bcd0514980976658b538d: [Windows job 140946830953](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46134551670/job/140946830953) compiled, regenerated and verified the extracted native package, but graphical smoke returned 5 without individual predicate diagnostics. No Windows artifact promoted. Exact failed predicate is not isolated. Replay now processes scripted Win32 input and actual portable PCM in deterministic GUI timer stages, explicitly paints each stage, requires a rendered panic frame and 48 silent blocks during the panic window, and prints individual predicates. Real interactive mode still uses the dedicated audio worker; native runtime concurrency tests remain in CI. No claim of real endpoint playback or target timing.

A separate actual-core defect reproduced: a stale contact followed by queued release and fresh scene in one pump can trigger panic and restart while final engine state is unmuted. Previous waveOut mute-edge condition misses the drain. Add audio-owner-only panic_epoch, compare after render and service pending fault even when all device buffers are in queue. Modeled device queue only; driver reset and audible latency NOT RUN. DSP and renderer unchanged.

Local commands (exact core/replay blobs 6c0fb89340a95c7caada3454de62699d4f420125 / a4721229dc6d5d989f68cf3b427c09f5c4e4876f):
- `g++ -std=c++17 -O2 -Wall -Wextra -Werror recovery_test.cpp -o recovery-test && ./recovery-test` PASS.
- `clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined recovery_test.cpp -o recovery-san && ./recovery-san` PASS.
- `g++ -std=c++17 -O2 -Wall -Wextra -Werror replay.cpp -o replay && ./replay 'recovered media'` PASS 2,304,000 partition samples, 12,000 stress ticks; all 10 WAV/PPM files byte-identical to M1 gallery.
- New fixture: 68 stages, 48 silent blocks (6,144 samples), actual PCM restart, stale-event same-block restart generation, full-queue emergency and lease expiry PASS both compilers. Tested runtime/fixture blobs f21c6ac897cb5d55f69c9dda9f1c3a7c6c48444d / 0dd5f0db203a7e027e3209fa3e08d51a36a3a4c4.

Exact-head Linux/Windows workflow must still run; local Win32 compiler/Wine unavailable, direct Git DNS unavailable. The GUI change is a proposed repair, not a Windows PASS. Actual Windows audio device, ESP32 compile/link, independent QA and physical results NOT RUN. Preserve M1 provenance and independently qualified predecessor instruments. No target firmware published.
