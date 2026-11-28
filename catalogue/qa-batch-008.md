# QA batch 008 — HYBRID M16 checkpoint adversarial reproduction (2026-11-28)

Disposition: Draft/HOLD. No promotion of prior collection pins. Independent native checkpoint subsystem only; full Windows and target reproduction remain separate.

## Exact inputs

- Main: bf08fb5a38a7fa692026af3bfb08943fb72506ef.
- HYBRID #10 head: 2d9841b1a1faeed37cd8dd8062d5dab24659bcc1, tree 0012f99bebce7c8b7620030cab553059a355bb1f (author receipt and extracted archive).
- Recovered cluster/checkpoint.hpp blob 3089db8437d2d4d38a0d403d61dc930f700966b8 and cluster/reaction.hpp blob 5337c9f71e0d40d95ac264dec42c41100ff237e4. Git hash-object verified both exact byte identities before execution. Git checkout unavailable locally (DNS resolution of github.com failed); no source copied into QA branch.
- Independent new fixture qualification/checkpoint_adversary.cpp tested blob eb7f8623159a1359c6073f9dd412b5ecdd798751.

## Actual executed independent tests

From a workspace containing the recovered exact cluster headers and QA fixture:

    g++ -std=c++17 -O2 -Wall -Wextra -Werror -Icluster qualification/checkpoint_adversary.cpp -o qa-gcc
    ./qa-gcc
    clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Icluster qualification/checkpoint_adversary.cpp -o qa-san
    ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./qa-san

Both exit 0, identical stdout:

    PASS checks=123567 bit_flips=100608 invalid_concentrations=6144 restored=60 expired=20 continued=180

Tests cover every single-bit mutation of all twelve packets of one actual checkpoint; malformed lengths/null; resealed invalid header fields; valid packet hashes with false whole-state digest; invalid Q12 concentration at every U/V position; exact state/metadata preservation and suspended-tick continuation on rejection; shuffled fragment delivery and duplicate refusal; 20 deliberately expired assemblies; 60 successful restores and 180 subsequent whole-plane/projected-state comparisons against the same uninterrupted production core. Seed 0x435944. FNV corruption detection is not authentication; this fixture is not a proof against multi-bit errors or malicious authenticated senders. No independent socket/follower policy, graphical execution, audio or physical timing claim.

## Actual archive inspection

Author M16 native run https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46132166480: 3/3 successful jobs, including Windows 140939502084. Author target run https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46132166485: 21/21 successful jobs. Checkpoint experiment not adopted in embedded profiles.

Downloaded https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46132166480/artifacts/15258052146 (nonexpired, expiry 2027-02-26T09:48:12Z). Outer SHA256 independently measured b46fc6d22efa61c772e734c04bcb961800bc59e78921a1e5b5167f3acb542b88, exact metadata match. Inner cluster-native-windows.zip independently measured dd1b960057dae4e173d57445d04e98502b37e4326a9cf03d7dd748408ab0844d, exact sidecar match. Extracted to path with spaces; all 43 manifest-listed files hash-correct, exact coverage of 44 entries including manifest. 22 x64 PE executables inspected with objdump -p; only KERNEL32, WS2_32, WINMM, GDI32 and USER32 imports observed. No executable launched locally (Wine unavailable; no Windows runtime). Package has original media and replay scripts, no installable firmware.

Archive source-commit.txt is 1c788ffeb7dea79710561bc26bde518f7c845392 (PR synthetic merge commit), not branch head. source-tree.txt matches above. Verified current PR #10 merge ref and GitHub commit message "Merge 2d9841b1a1faeed37cd8dd8062d5dab24659bcc1 into bf08fb5a38a7fa692026af3bfb08943fb72506ef"; compare branch head to merge ref reports ahead 1, behind 0, zero changed files. Preserve both identities; do not silently rewrite provenance.

## Closure and remaining gates

Batch007 independent Windows job 140924829763 and independent target job 140924829969 both success in run https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46127366662. Nonexpired target receipt 15255964310 digest metadata 6a858320412fea07fb119baa1393e3a0536f4cb77e5bf08a1f4fa5dad998bfd8 (metadata only this run). Prior frozen standalone pins preserved. HYBRID M16 checkpoint native adversarial PASS; author native/target PASS; archive integrity and PE inspection PASS; independent full UDP/follower, Windows graphical replay and target rebuild NOT RUN; virtual target NOT RUN; physical NOT RUN; rights/owner acceptance HOLD. XENOBIO M2 author Windows pending at lane handoff; Ecophon M1 core only; no repins.

Next: exact-source independent M16 UDP/follower and extracted Windows graphical replay, including adversarial fixture, then refresh collection once qualified. No new CI fan-out in this batch, no main merge, no creative branch edits, no firmware distribution.
