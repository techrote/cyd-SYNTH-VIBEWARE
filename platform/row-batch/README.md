# P3 — synchronous row batching, two actual consumers (Draft/HOLD)

Both CHRONOGRID M2 and Xenoresonator M4 currently submit one LCD pushImage per row (respectively four and eight rows per loop iteration). Keep the same snapshot and cooperative iteration cadence, but assemble contiguous rows and submit once per iteration. This changes submission count, not raster arithmetic or transfer byte count. No async DMA, framebuffer, new allocation, audio callback changes or universal SDK. Header is original implementation; instrument code remains at its own source.

Exact consumers:
- CHRONOGRID c56e129d174dd942dd2aa50f0ae1808083e4c857 instruments/chronogrid/core.hpp, blob 43c17b843c7366e194a221dda7c6eed8da7ac647.
- Xenoresonator 7620dacd6e6d68adeb65bf032d3d06ab9bb32948 instruments/fm1-ports/xenoresonator/core.hpp, blob 26eb7ae004f5c5fe69535a7b143848b9fb622305.

Recover these headers into a temporary directory as grid.hpp and xeno.hpp, verify with git hash-object; copy the two files from this directory. Local copies recovered from M2 gallery and collection 003-qualified (Xeno Windows CRLF normalized, then exact blob match). Commands:
```
g++ -std=c++17 -O2 -Wall -Wextra -Werror row_batch_test.cpp -o test && ./test
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer row_batch_test.cpp -o test-san && ./test-san
```
Both actual local executions:
```
PASS frames=168 pixels=12902400 baseline_calls=40320 strip_calls=7000 scratch4=2560 scratch8=5120
```
56 real audio-driven snapshots across six scenes and four materials/two modes, each compared at 4, 8 and 7 rows, including nondivisible last strip, zero budget and final row. Core pixel output only, excludes Xenoresonator's separate target LCD text overlay. Scratch 2560/5120 B total, versus row buffer 640 B (or CHRONOGRID two 640 B buffers); potential memory delta depends on final typed driver adapter. For a 240-row frame, 4 rows gives 60 sink calls, 8 gives 30, 7 gives 35 versus 240. Bytes remain 153600 per frame. These are counted synchronous mock sink calls, not measured SPI timing. Diviner #8 owner evidence (771064d419c4391f8252a31107606f4a801e8186) gives ~32.28 ms full RGB565 transfer, not a VIBEWARE performance prediction. No actual target timing or DMA completion guarantee. Windows and target execution for P3 NOT RUN; independent QA pending. P2 unchanged.

Next: owner lanes coordinate an isolated exact-pin target adapter, verify LovyanGFX overload/byte ordering and synchronous lifetime (do not reinterpret rgb565_t blindly); preserve Xenoresonator overlay, touch polling, panic, and immutable frame snapshot. Build/map and measure physical submission/cadence only when available; do not adopt if audio slack, touch responsiveness or memory worsens. No creative branch or integration pin change.
