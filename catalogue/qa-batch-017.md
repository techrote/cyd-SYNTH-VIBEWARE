# QA batch 017 — batch016 closure and six-instrument desktop custody (Draft/HOLD)
2026-11-28. #7 / #15; INTEGRATION #17 source 260098ea439ccd0f34fef26d454e89f31de0720e, tree 2274b606cfd0e6cf8c503adeeff681e8eaed6602. No integration branch edits or promotion.

## Independent batch016 closure
Run https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46172807310 : both jobs completed SUCCESS. Reviewed actual logs of Windows job 141064777171 and target job 141064776796. Frozen Xenoresonator 767f7ce5f66fb742a9530b426fd9513c5eb67fdb and Ecophon d353906d58b960fb987b18524d25dfe831078e60: clean Windows compilation, extracted graphical replay in spaces path, all 26 media exact, 52 original package manifest checks. Ecophon independent sample-sink randomized stress 6,468,556 checks, 6,254,663 ordered ticks, 4,233 full refusals, 7,837 resets. Clean target compile/link 3/3: Xeno 24,876 RAM / 385,421 flash B; Ecophon legacy 28,704 / 351,477 B; timer 29,248 / 342,181 B. PIO 6.1.18; static estimates, not runtime heap. Recovered artifact 15274562553 outer hash 174a7a839d93b7578432d8f5542907f227dea403007b6f02a62bd2b782ff13f1, 24 manifest checks, three exact source/tree identities. No installable images. Physical/virtual execution BLOCKED/NOT RUN.

## Integration custody
Exact integration head: seven author workflows completed SUCCESS (46176139311, 46176139263, 46176139251, 46176139281, 46176139261, 46176139309, 46176139249). Recovered all six Windows original archives and integrated seven-profile board evidence artifact 15275992757; eight outer digests checked in total including independent target evidence. Source/tree and manifest coverage checked. Original inner archive digest and count:
| App | Artifact | SHA256 | Manifest |
|---|---:|---|---:|
| Chronogrid | 15275269110 | c5255daf0a5968fc9184bfd472887ecedafd50c5e8700ab298dfff0b2bf3703e | 27 |
| Chronomorph | 15275568149 | 697a15655332c5e118a465244fed4c47fd39909b07c8e80443f6ea56a6dfa1ea | 22 |
| Xenoresonator | 15275591688 | 9ee191de44a85df438be2e66d7e2b3c192746fdaa911b160c70d21c8f5a724dd | 25 |
| Ecophon | 15275681368 | de89add39d89c5f27e17f620692e2393a4e040797df192bc00730f676b8d6f80 | 33 |
| Xenobio | 15275736208 | 90863f1bbeede52fb280e6defabc14e8adb68278bbf7eb08e0a4f5c46e5e7809 | 36 |
| PHASESPACE | 15276005706 | 5eacf649b4857541bcfdd92cdfc3fa6449cb0fd93df30954cdb7bbb631303352 | 29 |

Fresh verification: 172 desktop manifest entries, 89 target manifest checks (65 integrated, 24 independent, counts include overlapping parent/child entries), five intentional refusals (outer corruption, altered core, path traversal, firmware member, altered map). Original inner archives preserved, deterministic six-app wrapper built twice byte-identical: SHA256 88d868a96f9574506fbf1379413edbb6e9672ae21d4c640a980ee0e06972244b. Reproduce using python qualification/replay017.py DIRECTORY OUTPUT.zip with downloaded outer archives named qa-ARTIFACTID.zip. Tested local script Git blob d2d7d95e8d8cbe39cd67486245feaf21e9c9b00f. Wrapper is desktop-only owner audition, not a release or physical approval.

## Actual independent integrated Ecophon native replay
Extracted original Windows archive to /tmp/qa017 Ecophon. Nine source files compared to exact repository blobs after CRLF-to-LF normalization (core.hpp, runtime.hpp, replay.cpp, runtime_test.cpp, recovery_test.cpp, target_gate_test.cpp, target_gate.hpp, sample_sink_test.cpp, sample_sink.hpp). All match. Executed g++ -std=c++17 -O2 -Wall -Wextra -pthread for replay, runtime_test, recovery_test, target_gate_test, sample_sink_test; ran each. All five PASS; diff -rq media 'new media' exit 0 (10 WAV/PPM identical). 2,304,000 partition samples, 576,000 matched PCM, 7,680,000 matched pixels, 200,000 FIFO events, 256,000 exact DAC codes, 513 panic occupancies. Native only, not embedded timing or analog output.

Integrated fresh independent Windows graphical execution NOT RUN; integrated target fresh independent rebuild NOT RUN (author builds plus independently verified receipts; batch016 independent predecessor builds separate). Other integrated fresh independent native replay NOT RUN this batch. Local git clone blocked DNS; Windows and PIO unavailable locally. No physical/virtual execution, firmware distribution, branch merges or acceptance. Retain all previous exact pins. Next: finite independent integrated extracted Windows replay and target build comparison if needed; coordinate audio backend with PLATFORM. Batch016 automatic PR trigger retired after success, manual dispatch and every job/test unchanged.
