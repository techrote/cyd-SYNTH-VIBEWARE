# QA batch 006 — target recovery and new desktop candidates (2026-11-28)

## Independent target recovery — PASS, frozen profiles only

QA run 46118660237, job 140898051626, exact QA head a50080a29c1fef7a69d3d0bc475c4c01934e94b2. Reviewed actual logs, all three compile/link and per-profile receipt checksum checks passed. Artifact 15252966833, GitHub metadata SHA256 927523a19d0b03850a635b73081570a70c17b810420f4ea6abfe2141de0eaad3; unexpired. Archive bytes not independently downloaded in this run. Earlier failure 15250873460 preserved.

| Profile | Frozen source | Static RAM bytes | Flash bytes | Build elapsed |
|---|---|---:|---:|---:|
| PHASESPACE M4 | 724d4d2251d9f190e1c412432594239ccd8325fd | 27636 | 382905 | 85.36 s |
| Xenoresonator M4 | 724d4d2251d9f190e1c412432594239ccd8325fd | 24876 | 380277 | 88.02 s |
| CHRONOGRID M1 | 153e6bbd563242c9bc8bbeda47d1e40158877d41 | 25296 | 373329 | 84.43 s |

These are linker/build figures, not runtime memory or physical timing. Independent workflow repair is provisional until separately reviewed. No target image delivery.

## M4 Grid independent native reproduction — PASS

Exact c0a866a71ecf0a6be958a04f9870b92d0a7fe069, core blob 1938c700c31b47188d571664594f1e9bfeb6e4bf, replay blob feb4c727c39c63f800da4c0a64578f6798882a52, matched owner conversation gallery via git hash-object. All gallery checksums passed. GCC -std=c++17 -O2 -Wall -Wextra -Werror replay.cpp; Clang -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer replay.cpp; execute each with separate media directory, ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1. 2,891,648 checks each; diff -qr 21/21 exact media; cmp gate-free.wav gate-coupled.wav gate-grid.ppm each vs archived corresponding file exact. 126377/192000 PCM samples differ, free events 39,35,74,49 vs coupled 37,13,65,30. Author run 46121977986 all 4 jobs PASS; Windows artifact 15253554975, target 15254162786; independent Windows and M4 target NOT RUN.

## Chronomorph M3

Exact a42f33ecd37628fc6db64f0f0ffafc333f82af6d author run 46120900337 all 3 jobs PASS, Windows artifact 15254076393, target 15254136294, unexpired. Independent current native/Windows/target NOT RUN. QA replay006.py pins both raw outer digests and exact source commits, compares source tree receipt to clean checkout, checks complete inner checksum coverage, rejects path traversal/duplicate case-folded names, regenerates media and executes extracted GUI smoke in spaces path. Local test of parser: original M1 archive from collection003 wrapped as test outer ZIP PASS, eight negative controls (outer tamper, wrong tree, five unsafe paths and case-folded collision) refused. Actual new archive downloads and Windows execution pending CI. Workflow independently rebuilds both candidates and compares clean native media; no physical audio endpoint test.

HYBRID M14 ee13d7622a43271b272ee9ae3d32e162bf83fad9 author 46122682336 Linux and sanitizers PASS, Windows in progress at check; no promotion. Integration #17 7138c7769e2c8c25ae519eca4485f15bd697aaf9 remains separate. Collection003 original desktop pins preserved. Rights/physical/owner HOLD. Next inspect independent Windows workflow and update catalogue only on exact PASS; then refresh independent target pins and qualify green HYBRID.
