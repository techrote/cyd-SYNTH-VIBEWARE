# QA batch 009 — XENOBIO independent native and backend boundary (2026-11-28)

Draft/HOLD; no promotion or main merge. Main bf08fb5a38a7fa692026af3bfb08943fb72506ef. #20 M2 2eaccb0b01a429d987cf7fd9b57b4e5eceb4f58c / tree 9a6448a65e3c1f8fabef94d3845035a404c63fdb (package receipt); M3 1d3b497cb6a36c2d1cbf467f4d89ba81fe00675b retains exact core, runtime and Windows blobs. Prior QA pins unchanged.

## Independently executed

Recovered existing M2 Windows inner ZIP from prior author delivery, SHA256 3b873ef647fc7e1e3df7e2a01d3625699f66a40e7efaf327a0ff55620763569f. Author artifact 15258032091, run 46132156384, unexpired metadata digest sha256:c51491a0f36f866db7100146bdea1ff64a3fb2988d2c77e4c8605006b59f1a63. This run checked inner ZIP and all 32 manifest entries before source normalization; outer archive not independently recovered this run. Extracted into path containing spaces. Normalized CRLF to LF for source only; all 10 source files matched Git blob identities fetched at M2 ref, including core 6953826440779ef79a53adc76d63b2829079ed88, runtime de6d9f76bb2d1f735024c1d61386fcd2b0de09dc, labeled c35cae93e683db681f5ccd32e34fece8e919d3a0, replay bd315ebd1de6522aaea75fcd44d33e5b0a4c34f6, runtime_test 91b4a0e60ea160285caa6a63397642e7543c3f70 and windows b17a8ec47039b5b1cc1b53e0e685bee6404e0806.

Local x86-64 GCC 14.2.0 and Clang 17.0.0. From extracted archive root:

    sha256sum -c manifest.sha256
    # Normalize CRLF source files and check git hash-object against exact refs.
    cmake -S source -B 'gcc build' -DCMAKE_BUILD_TYPE=Release
    cmake --build 'gcc build' -j2
    ctest --test-dir 'gcc build' --output-on-failure
    mkdir -p 'clang media'
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer source/replay.cpp -o replay-san
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -pthread source/runtime_test.cpp -o runtime-san
    ./replay-san 'clang media'
    ./runtime-san 'clang media/runtime.ppm'

GCC CTest 2/2 PASS. Clang sanitizer author fixtures independently executed PASS. All 15 packaged media (8 WAV, 7 PPM) byte-identical to both regenerated sets. Runtime replay 576000 exact PCM, 100000 FIFO events, 100000 publications; 12000 ecological ticks. Independent additional fixture qualification/qa_xenobio.cpp (Git blob b46620064607da72767e8cf40da3e48142ef2f4f):

    g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isource qa_xenobio.cpp -o qa-gcc
    ./qa-gcc
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Isource qa_xenobio.cpp -o qa-san
    ./qa-san

Each PASS 800 panic pixels, 61 adjacent pixels, 32 queue occupancies. Real PCM witness: backend's one pre-loop pending check can miss an emergency arriving before Runtime::render; render acknowledges it, returns silence and clears pending before next loop. Previously queued nonzero PCM then has no corresponding waveOutReset in that ordering. This is a deterministic model of the exact source scheduling boundary, NOT actual driver playback, hardware latency or proof of a particular audible duration. Backend-drain gate FAIL for modeled interleaving; digital panic PASS. Request a durable emergency generation/ack observation or explicit render result, flush before requeue, regression at this boundary. Handoff #20 comment 7238106373. No creative branch edits or authored core repair.

## Other live evidence

M3 author run 46136480264 Windows job 140952807017 and target job 140952807149 both PASS; artifact 15259504683 Windows and 15260435422 non-installable map receipt unexpired metadata only. Independent Windows device/graphical execution and target rebuild NOT RUN, physical NOT RUN, no acceptance. Local git ls-remote failed DNS; no Wine or PlatformIO in local environment. M17 HYBRID native run 46136503880 had Ubuntu and sanitizers PASS, Windows in progress at inspection; target run 46136503881 14/21 completed successfully, remainder in progress at inspection; independent M17 controller/UDP/Windows NOT RUN. Ecophon run 46134551670 Windows job 140946830953 FAIL at graphical replay exit 5 after compilation, source and media replay passed; Linux PASS. No Windows package available in that run. Source inspection shows exit 5 aggregates five predicates; avoid guessing which failed.

Next: coordinate TOUCH backend drain repair and exact-head independent extracted Windows replay; then independently reproduce M17 controller/manual cancellation and actual UDP. Retain existing qualified predecessors and rights/physical holds. No new CI fan-out in this batch.
