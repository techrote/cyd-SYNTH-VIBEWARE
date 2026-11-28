# QA batch 011 — integrated owner audition collection and Ecophon independent native qualification (2026-11-28)

Draft/HOLD. Main bf08fb5a38a7fa692026af3bfb08943fb72506ef. QA parent 1481a84103299cdc33d5c43c8af1d34a3acecd99. Preserve all frozen independent author pins; no runtime changes, main merge, release or physical approval.

## Actual delivery

Recovered five exact integrated #17 author Windows archives (head 319664f449f2c4e77ced3ff80f5a743e4ad36b59, tree 5a4b413929afe0f420e80f7c2e71c42f1136ebce) and the Ecophon M3 preview. Verified 17/17 integrated author CI jobs succeeded (3 or 4 per workflow). All artifact metadata nonexpired at inspection. New assemble011.py verifies exact outer SHA256, available inner digest receipts (Chronomorph and Ecophon outer archives have only one inner ZIP), complete inner manifest coverage, source commit/tree receipts, safe relative names, expected executable PE magic, no installable firmware members. Original inner archives copied byte-for-byte. Two deterministic assemblies (normal Python and python -O) produced identical collection SHA256 7d44a3024a9aa0b695e747acfb2718b6cef731f855756233ad71d584eabc80e0. Mutated outer archive rejected under python -O. Tested assembler blob 07e83183310d4502c874e361dd57dd7a71490a36. The owner-only collection is delivered through the automation output; no binary committed to GitHub. PINS.json, SHA256SUMS.txt and README.md accompany the original archives. New cumulative executable replay is NOT RUN independently; prior separate exact-source/package independent evidence remains valid only for its frozen pins. No new qualification claim from checksums alone.

| Instrument | Author CI | Windows artifact | Complete manifest entries | Media |
|---|---:|---:|---:|---:|
| PHASESPACE | [46145279990](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279990) | [15263486904](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279990/artifacts/15263486904) | 29 | 23 |
| CHRONOGRID | [46145279922](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279922) | [15263715490](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279922/artifacts/15263715490) | 27 | 21 |
| Xenoresonator | [46145279932](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279932) | [15263133705](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279932/artifacts/15263133705) | 25 | 16 |
| Chronomorph | [46145279872](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279872) | [15263302248](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279872/artifacts/15263302248) | 22 | 8 |
| XENOBIO | [46145279876](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279876) | [15262659578](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46145279876/artifacts/15262659578) | 36 | 15 |
| Ecophon (separate preview) | [46143835690](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46143835690) | [15263235062](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46143835690/artifacts/15263235062) | 27 | 10 |

166/166 payload hashes, 93 actual media files. Reproduce by downloading the six outer ZIPs from pinned artifact IDs using the filenames in assemble011.py, then running:

    python qualification/assemble011.py <download-directory> CYD-VIBEWARE-Owner-Audition-011.zip

Integrated XENOBIO source directory and all 15 media are byte-identical to recovered independent batch010 author M4R archive; integrated inner ecda1297486bb1efa32ac814b76bc52e342ab2455d25b0bff9524960fe11e621. Batch010 independent Windows and target 2/2 success confirmed via jobs 140970066930 and 140970067163, map-only receipt 15261942720. M5 successor e9b8e8b47a1973d36453fd8f15e0309138da5361 author run 46146171930 Windows/target 2/2 PASS, not independently adopted. Integrated author five target map IDs 15263492012, 15263750307, 15263114325, 15263492001, 15263845042 metadata inspected; independent extraction/rebuild of these new target artifacts NOT RUN.

## Ecophon independent native

M2R source f3d22fbcd87e9aa9a3ffe408b56fdef09e7f3d77 / b067b8b634fae7c8eb29f9a63a943cb633697bb0, inner SHA256 30af668b1b8092874b08c3550076e0e9a5742e71069fe87de73fc810ab314e57. Recovered supplied inner archive: 27 manifest entries PASS, ten packaged source/docs Git blobs (CRLF normalized to LF) match connector at exact revision. M3 exact ba10122adbe2e431087d219f439815254f16eb0c / 21f8ad084c85da4976bf7eb6f2c79d1b80b53371 author Linux/Windows/ESP32 3/3 PASS. M3 recovered outer 8404b52d27aef95e0fce06e2276de8a7ee9ed4b7ab5c2ad400bfa265f9ba5b27, inner 5ca43d30a276ff67a8970751e123a89cf74a94f1118298111ea555e5193f9c66; 27 manifest entries PASS; all ten source/docs Git blobs match both M2R and M3. All ten M3 media byte-identical to M2R. Target gate exact Git blob 691d2aa4eca7163292aa389b6c73611ab87894c5 recovered through connector and locally hash verified.

From extracted M2R spaces path, both GCC release and Clang ASan/UBSan:

    g++ -std=c++17 -O2 -pthread -Wall -Wextra -Werror replay.cpp -o replay
    ./replay 'media gcc'
    g++ -std=c++17 -O2 -pthread -Wall -Wextra -Werror runtime_test.cpp -o runtime-test
    ./runtime-test
    g++ -std=c++17 -O2 -pthread -Wall -Wextra -Werror recovery_test.cpp -o recovery-test
    ./recovery-test

Repeat with clang++ -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer in place of g++ -O2. All PASS; 2,304,000 partition-equivalent PCM, 576,000 matched runtime PCM, 7,680,000 matched RGB565 pixels, 200,000 FIFO events and 200,000 publications, 12,000 stress ticks, 68 timer stages and 48 silent blocks. All ten regenerated WAV/PPM specimens byte-exact under both compilers. 479,684/576,000 intervention PCM samples differ causally. No listening claim.

New independent fixture qa_ecophon.cpp tested blob 7c54ccf30af9e114930f1a22d4e169a3d1f36952 exercises actual M3 target gate plus M2R/M3 runtime. Compile from exact M3 directory:

    g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. /path/to/qa_ecophon.cpp -o qa-ecophon
    ./qa-ecophon
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I. /path/to/qa_ecophon.cpp -o qa-ecophon-san
    ./qa-ecophon-san

Both PASS 2266 checks: every FIFO occupancy 0..32 at two time origins (66 schedules), 33 delayed BOOT/held-contact target arming schedules, rollover, same-pump stale-event/restart generation and exact 149/150ms expiry boundary. This is real engine PCM with deterministic input schedules, not actual Windows device draining or physical CYD timing.

M3 target receipt artifact [15262837442](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46143835690/artifacts/15262837442) recovered outer digest de41fbfeb3beca6567e99722aa558e2e55eff05f864b3ffb41616b6f832f0414 matches metadata; six files, source/tree exact, map nonempty, no installable payload. Author log: esp32dev 240 MHz, espressif32 7.1.3, Arduino 4.20017.260907+sha.dcc1105b, Xtensa 8.4.0+2021r2-patch5, LovyanGFX 1.2.32, XPT2046 d57f64c. RAM 28,704/327,680, flash 351,265/3,145,728. Notice paths present, full rights approval not asserted. Independent target compile and Windows execution NOT RUN (local PlatformIO, Xtensa, Windows/Wine unavailable; direct Git DNS failure). Virtual/physical NOT RUN. Actual DAC and WinMM latency unmeasured.

## HYBRID custody, not promotion

M19 author native 46145977892 3/3 and target 46145977926 24/24 PASS. Recovered artifact 15264330217, outer 86d92ad89492f154bd8be5d4feec8c4000210a14da79f4b5d48e48d1f58a3b7d, inner 45d5863743602b19ac958c923106c2471e97509e6b25c9a7e49ddb312c64109e; 43 manifest hashes PASS. Source receipt d5729a4bd4eb884a7a0ce3cd1d22269adcbd0e6d differs from author PR head 52cca8f1458916ae13f7d09462a60c0d00d8ecf6, tree 26a7dd7d757c3cbeaeb9c407b8a41a217cd1322e matches; compare reports ahead one commit, no changed files. Treat as distinct checked-out commit with equivalent tree, not exact head identity. Ask HYBRID to use explicit PR head checkout or record both. Independent controller/UDP, extracted Windows and target rebuild NOT RUN. Not included in standalone audition package; prior pins preserved.

## Disposition

No new hosted workflows or changes to existing gates. This local-tested, archive-only QA delta uses a skip-CI commit marker to avoid triggering six historical fixed-pin workflows through cumulative PR path filters; no qualification is inferred for the new QA commit. Keep Draft/HOLD, preserve previous exact run receipts; coordinate a narrowly scoped independent Windows and target batch before any promotion, and repair historical trigger structure with PLATFORM separately. Next: independently execute Ecophon extracted Windows and clean target, then exact M19 controller/UDP, and independent cumulative collection replay. Owner physical and rights gates HOLD.
