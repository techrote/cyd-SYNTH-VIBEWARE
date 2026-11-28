# QA batch 010 — XENOBIO M4R independent recovery (2026-11-28)

Draft/HOLD. Main bf08fb5a38a7fa692026af3bfb08943fb72506ef. Exact #20 source 9a314395eac5bf577192bbdb3a58fb7cc628cf22, tree 6098ecf645cb898cab3b157d5a3daa5a9ce383f9. #15 parent 970772c6f1f5ea943bb3e87e0b16bceecd5052a7. No creative branch edits, previous pins unchanged.

## Independent local execution

Recovered author Windows artifact 15260959010 from run 46141001982. Outer SHA256 ef262de8e063948dca74650fa5284f5c6cb8d5a274b96650557c87e15cc2fdd2 matches GitHub metadata; inner SHA256 452079bd55b0f0923350efd44bbf127058b72458553e709c9a6e440d20f9136c matches enclosed receipt. Unexpired (2027-02-26). Exact allowlisted layout, 36/36 manifest hashes, 14/14 packaged source text files normalized CRLF to LF matched Git blob identities fetched at exact head. Empty source/firmware directory is not target source delivery. Extracted spaces path; objdump PE import inspection 3/3 (KERNEL32, WINMM, GDI32, USER32 only). New replay010.py verifies full pinned layout, source blobs, hashes and media, corruption rejection tested locally. Windows execution branch NOT RUN locally.

From extracted archive root on x86-64 Linux GCC 14.2 and Clang 17:

    cmake -S source -B 'gcc build' -DCMAKE_BUILD_TYPE=Release
    cmake --build 'gcc build' -j2
    ctest --test-dir 'gcc build' --output-on-failure
    mkdir 'clang media'
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer source/replay.cpp -o replay-san
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -pthread source/runtime_test.cpp -o runtime-san
    ./replay-san 'clang media'
    ./runtime-san 'clang media/runtime.ppm'
    g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isource qa_xenobio_drain.cpp -o qa-drain
    ./qa-drain
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Isource qa_xenobio_drain.cpp -o qa-drain-san
    ./qa-drain-san
    python replay010.py Xenobio-M4R-outer.zip --compare 'clang media'

GCC CTest 2/2 PASS, Clang ASan/UBSan replay and runtime PASS, all 15 archived WAV/PPM specimens byte-identical to each regenerated set. Existing 576000 paired PCM samples, 100000 FIFO events and publications, 12000 ecology ticks PASS. Additional independently authored fixture blob bc4a1cba2809afc06ef5866025306e5b75b41fd5 PASS both compilers: 22 reachable queue occupancy/panic boundary schedules, 1001 acknowledged/requested epochs, failed reset retry, fresh restart. Eight schedules injecting after final drain check or after write retained nonzero queued PCM until the next modeled service; all cleared at that next check. This independently closes the previous modeled permanent reset-loss defect, not instantaneous mute or actual driver timing. A modeled Device holds real PCM, not WinMM. Source repair is TOUCH-authored; QA tests are independent of author tests. New archive verifier blob 3635022f1b4908b7fe271b871e3b7b849a5893e5.

## Author hosted and evidence custody

M4R author run 46141001982 Windows 140966690225 PASS, target 140966690419 PASS. Independently recovered target-map-only artifact 15261688786 outer SHA256 8c6bf5997b9adfe585558a365353723748a300c33fe3ca5e6d0a7d2c75f5dd03 matches metadata; six files (build/toolchain logs, map, ELF digest, exact source/tree). No installable firmware included. Build log espressif32 6.10.0, esp32dev 240 MHz, Arduino 3.20017.241212+sha.dcc1105b, Xtensa 8.4.0+2021r2-patch5, LovyanGFX 1.2.32, XPT2046 d57f64c, RAM 30808/327680, flash 374497/3145728. This is author build plus independently verified archive custody, not independent target rebuild. Local git access blocked by DNS; Windows/Wine and PlatformIO absent. New narrowly scoped QA workflow prepares one Windows job (exact archive replay plus clean GCC core/GDI and independent fixture) and one clean ESP32 rebuild with map-only output and dependency notice hashes. Hosted results NOT RUN at publication; no promotion. Actual Windows driver playback, physical touch/DAC and all physical acceptance remain NOT RUN; distribution rights remain owner gate.

Ecophon M2R author run 46139757618 Linux and Windows both PASS; artifact 15261421931 unexpired SHA256 0bd9061577a652ba7d2fd115f32ed395fe5ceff2915e66126e3382729ef28493 metadata only in this batch. M17 author native run 46136503880 3/3 PASS, target 46136503881 21/21 PASS, Windows artifact 15259483353 unexpired metadata only; M18 active successor b977167cfb7661265ed771adf84a049afc231b07 not yet qualified. No independent execution or pin promotion for either in this batch.

Next: close batch010 exact Windows and target results; independent M17 controller/UDP and Ecophon M2R reproduction, then refresh collection. Physical and rights gates preserved. No automatic merge or flash.
