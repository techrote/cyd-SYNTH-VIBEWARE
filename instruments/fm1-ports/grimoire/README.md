# Grimoire CYD — Orbit Gate M3 (Draft/HOLD)
PORT #13, original touch-native interpretation, not a binary/source port.
Reference FM-1-VIBEWARE #10 exact 803d4b577d081bc67f907a02a633672d4c17bb3c, author native Windows/Linux 2/2 PASS (46146398139). Reviewed instrument README, programme/catalogue and controller feature receipts. Reference LICENSE lookup returned 404; no upstream source or media reused. Reference semantic features: 24 kHz mono, 24 chromatic steps, three materials, Y phase/filter, three factory trajectories, eight-point sketch, direction and speed, manual takeover, release and panic. New independently written oscillator arithmetic, pitch smoothing, path shapes, interpolation and renderer; exact upstream PCM parity is not asserted. No browser, FM-1 panel, wire protocol, MIDI, sketch JSON exchange or persistence implemented. No CYDaux dependency. Other reference main pins for discovery only: FM-1-CYDaux e65a0b880b5c13c9aa3ce734c6e3eaa4b11fc969, FM-1-MHAC 16c698d9e244559064a9a9a22f834d36912bf498, cyd-diviner 7b1b37eec6ac126f334a0b008d91135071ea60fa. No code adopted from them.

## Play
Windows candidate: run Grimoire.exe. Amber / Iron / Abyss select material and mute; tap or drag in the large XY pad to sound a note, X selects pitch, Y changes phase modulation/filter. Orbit explicitly latches automatic motion and gate; tapping Orbit again or Panic stops it. Manual touch takes over and on release returns to the moving orbit (when explicitly enabled). R/C/P select Ribbon/Crown/Pendulum; V reverses, S cycles speeds 1/2/4; E silences and enters eight-tap sketch editing; tap eight points, then explicitly enable Orbit or play manually. Escape or Space panics; focus/capture loss panics. Manual contact expires in 250 ms without refresh; standalone manual release decays. No multitouch/pressure assumption. Visual path, cursor, phase satellites and envelope bar derive from actual core state. GDI labels are host overlays; portable RGB565 frames contain actual state without text labels.

## M3 touchscreen-only controls
The lower six controls replace the keyboard requirement: Ribbon, Crown, Pendulum, Sketch (eight individual pad taps; tap Sketch again to abandon), Reverse/Forward and Speed 1/2/4. The top material/orbit/panic controls remain. The XY pad is vertically rescaled to fit above the new toolbar; the actual engine path and oscillator geometry are resampled in the Windows renderer, with the envelope meter retained. Moving an active pad contact into Panic mutes and flushes queued audio. Capture loss, focus loss or a missed release detected by the host poll cancel the contact. Sketch mode is silent and blocks orbit start until completed or cancelled. No pressure/multitouch required. Keyboard shortcuts remain optional. M3 local portable control tests passed; graphical and board qualification pending.

## Local reproduction
From this directory:
    g++ -std=c++17 -O2 -Wall -Wextra -Werror replay.cpp -o replay
    ./replay "gcc output"
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all replay.cpp -o replay-san
    ./replay-san "clang output"
    diff -rq "gcc output" "clang output"

Executed GCC 14.2.0 release and Clang 17 ASan/UBSan PASS, 576000 partition-equivalent PCM samples per compiler, three eight-second WAVs and three RGB565-derived 320x240 PPMs identical across compilers. Peaks 7718/7452/7158; nonzero samples 152609/152714/152709. Verified initial silence, pitch monotonicity, touch clamp and lease release, reverse without cursor reset, invalid sketch rejection, cancel/stale movement, row bounds, immediate digital panic and distinct media. Engine 132 bytes on host. Test code uses dynamic media storage outside engine; sample() itself has fixed state, no allocation/locks/I/O. Single serialized owner only, no target concurrent handoff yet.
Windows source candidate uses Win32 message ownership, four fixed waveOut blocks, reset on panic, actual core RGB565/GDI, extracted --smoke replay without opening a physical device. Hosted CI must establish compile and actual graphical execution; local MinGW/Wine absent. Real audio-driver playback, queue timing and shutdown remain unmeasured. Visual redraw cadence target 33 ms, not measured. Rendering currently traverses paths per scanline and requires cost measurement/optimization before embedded adoption.
Windows compile candidate:
    g++ -std=c++17 -O2 -Wall -Wextra -Werror windows.cpp -o Grimoire.exe -lwinmm -lgdi32 -luser32 -static
    ./Grimoire.exe --smoke

## Qualification
Native local PASS; local sanitizers PASS. Hosted Linux/Windows and extracted graphical NOT RUN until actual CI completes. ESP32 compile/link, virtual target, independent QA, physical NOT RUN. Physical board, audio lifecycle, calibrated touch and timing remain HOLD. No firmware, no new shared runtime, no merge request. Next: close Windows CI, target adapter using independently reviewed audio lifecycle with PLATFORM, owner physical gate separately. Ecophon M4 existing author 3/3 PASS retained; P11 ISR/clock/startup/cleanup and independent physical gates tracked in #13/PR #21; no replacement of earlier pins.

## M4 target front-panel milestone (digital only)
The M3 exact author Windows/Linux qualification passed. M4 adds a common panel projection, a serialized board session and a source-only classic CYD front-panel compile candidate. No DAC/I2S/speaker audio output is enabled; digital PCM is generated and discarded. M4 is not playable on hardware yet. See M4.md for actual native results and the pending target/audio/physical gates. Prior M3 Windows audition remains valid for its exact source.
