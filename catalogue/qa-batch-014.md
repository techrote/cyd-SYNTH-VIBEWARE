# QA batch 014 — frozen collection closure, Ecophon M4, and Xenoresonator strict-media repair

2026-11-28. #7 / #15. Draft/HOLD, independent of integration #17. Main bf08fb5a38a7fa692026af3bfb08943fb72506ef unchanged.

## Batch 013 closure (frozen 319664f4)
Exact source 319664f449f2c4e77ced3ff80f5a743e4ad36b59/tree 5a4b413929afe0f420e80f7c2e71c42f1136ebce. Independent [run 46157082622](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46157082622), QA workflow 5ee41c9d. Actual logs retrieved for jobs 141016002420 and 141016002186. Extracted Windows replay PASS 5/5, real PCM/PPM regeneration within Windows package (83 media total), GDI smoke 5/5 (not live audio-device audition). Clean MSVC core/GDI 3/3 (4 CTest tests); clean UCRT64 2/2 plus target-input fixture PASS. Fresh per-profile target compile/link 5/5 PASS, PIO 6.1.18 / IntelHex 2.3.0, Xtensa 8.4.0+2021r2-patch5. Downloaded [artifact 15268730498](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46157082622/artifacts/15268730498) SHA256 304e3a0dc4765103564a5fcd5859a469b2aa76f4fe63f64c76f8e6570927fc7e (3,150,745 bytes, expires 2027-02-26); all 40 nested SHA256 manifest entries and five source/tree pairs verified locally. No installable payload in archive.

| Profile | Static RAM B | Flash B |
|---|---:|---:|
| cyd-phasespace | 27636 | 382937 |
| cyd-chronogrid | 25392 | 374429 |
| cyd-port-xeno | 24876 | 380293 |
| cyd-port-chrono | 26064 | 349017 |
| cyd-xenobio | 30808 | 374705 |

Historical strict Linux/Windows 330 one-code PCM differences on Xeno remain FAIL at this frozen pin. Batch011 collection unchanged. Physical hardware, analog/audio/touch timing and redistribution approval HOLD; virtual target NOT RUN.

## Ecophon M4 independent actual-core replay
Source d353906d58b960fb987b18524d25dfe831078e60/tree 7f104198c8e911cea0c0d77abbdf3adfb586b999; author [run 46149077845](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46149077845) 3/3 success. Three downloaded outer SHA256s verified:
- Windows 15265001841: e1963f29cc1043625806e2756bd94a50aee665b80fa58815dd17eded0608bf7a; inner Ecophon.zip 24a7f8ec9b6891920d267eea18b6ff198d231a64fa2167b30ec259500fdc56e9; 27/27 manifest entries verified; source/tree match. Windows preview only, author GDI.
- Linux 15264254091: 59b7b090a965383818da04b3237d678faa05dda5bfd402e9e241023980356f2f; 10/10 media hashes verified.
- Target 15264962161: a45928bfb9efc27b1ddc82048212704b376fd9fe22c003ccbb0ebe3ca3070ade; maps/source/build logs and toolchain receipts only. Legacy 28704 RAM / 351269 flash; timer 29248 RAM / 341925 flash. Author compilation not independent target execution.

Source recovered in fresh spaces path from Windows archive, normalized CRLF only after original manifest verification; six source blobs checked against exact GitHub files: core 6c0fb89340a95c7caada3454de62699d4f420125, runtime 84ce9b40e2181aa3f8e78329d030c1b3c13cd183, replay a4721229dc6d5d989f68cf3b427c09f5c4e4876f, runtime_test 401e72c6bfea9fc845d7473a4a4c48b7745cad8b, recovery_test 0dd5f0db203a7e027e3209fa3e08d51a36a3a4c4, windows 3a90e99a1cddc152605260046c7f63eb2eaefb1d. Sample sink independently recovered from pinned source blob 42e6047fb19832c4aa290902c2a4b20ed68b8d84. Package does not include M4 sample_sink.hpp, sample_sink_test.cpp, target_gate.hpp, target_gate_test.cpp or M4 receipt; do not represent desktop archive as complete timer backend reproduction bundle. Request PORT include these source-only files for future packages. No firmware requested.

Commands in instrument source directory (copy independent qualification/ecophon_sink014.cpp into same directory, or provide -I to exact headers):
- g++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread <fixture>.cpp -o gcc-<fixture>
- clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined <fixture>.cpp -o clang-<fixture>
- Run replay with separate media directories, runtime_test, recovery_test, ecophon_sink014 under both compilers; diff -qr each media directory with extracted Windows and hosted Linux media.

Actual 8/8 executions PASS. Original 2,304,000 partition samples; 576,000 matched PCM; 7,680,000 pixels; 200,000 FIFO events/publications; 68 recovery stages and 48 silent blocks per compiler. 10/10 WAV/PPM byte-identical across all four outputs. New independent fixture blob af01109db0026c8be232204648a3e5fdc480ca18: 6,468,556 checks per compiler, 42,596 blocks, 6,254,663 ordered ticks (3,542,236 nonmidpoint), 4,233 full-ring refusals, 7,837 reset sequences, 10 lease/wrap/coalesced restart cases. Models sequential same-core ISR interleaving at API boundaries; not instruction-level, RTOS, or physical testing. Local exact-source native PASS, M4 independent Windows execution NOT RUN, independent target build NOT RUN, target ISR/disassembly/clock/analog timing and physical HOLD. No self-certification of fixture by separate third party claimed.

## Xeno repaired strict media gate
New #17 source 767f7ce5f66fb742a9530b426fd9513c5eb67fdb/tree 8771d94734efbd5af28f784a27c67ef498142a98. [Author run 46160428783](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46160428783) Windows, Linux, target and exact cross-environment media 4/4 PASS. Downloaded and verified SHA256s:
- Windows 15269132918: 19396af1add6a97b335a97da2353c36e81b67521af60eec94b9c3390a02eccb1; inner ZIP e384cb22763be96f2076302a82c7bfd46d5446665e7490d4b4d1311aa95f0154 agrees with sidecar; 25/25 payload hashes.
- Linux 15269760287: 032188fcef1ba3b3d59b6d65ccb718889499af9ca05f06bebb8c07ce957e7ef7.
- Target 15269680719: 1b48917a92c443fb1f9dccfd98fad69452dca8bfcdbbd015cf65bcf8e7cf99bc; 6/6 manifest checks, same source/tree, no installable payload.

Recovered exact source blobs after CRLF normalization: core ae695f6c6b18c7a82d21805bc31d04717c1acc40, exchange b69d81eedb2ba27ddae39d69b2cc4cb5e65e0258, replay d8cd008123c59da674d92cbc0e72be877ae6156c, windows 0275f435b9e53cd65897b453ff6858679f712faf. Rebuilt g++ -std=c++17 -O2 -Wall -Wextra replay.cpp and clang++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined replay.cpp; both PASS all replay checks including 2,560,000 stress samples, four material phrases and four rake phrases, panic/lease/FIFO. Independent strict comparison of 16 files across both rebuilt media directories, hosted Linux artifact and hosted Windows artifact PASS byte-identical; 768,000 PCM samples exact, no tolerance introduced. Prior frozen FAIL preserved. Git compare 319664f4...767f7ce5 shows only Xeno core source changed, two workflows and two integration scripts; four other instrument source profiles unchanged. New head independent Windows execution/target rebuild NOT RUN; predecessor evidence is not relabeled as fresh execution. New package is separately reviewable, not replacement of frozen collection without explicit QA pin refresh. No owner merge/physical approval.

Next: independently execute repaired Xeno and Ecophon M4 extracted Windows and clean target builds in one bounded exact-source batch with trigger isolation coordinated with PLATFORM; HYBRID after exact author delivery. Local Windows/PIO unavailable; direct git DNS fails. No new workflow or dispatch in this checkpoint; historical cumulative PR filters may retrigger. No integration or main edits.
