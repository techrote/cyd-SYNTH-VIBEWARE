# PHOSPHOR M3R — Windows delivery gate repair

Draft/HOLD #22 / #6. Parent ce44e91360e0ff02fb7d10f5acd429f80c8407cb (tree 1fbd4afb00d0b682bf2c71e1795caa102419377f). No C++ source or musical behavior changes.

## Actual predecessor execution

[Author run 46161445190, job 141029445557](https://github.com/techrote/cyd-SYNTH-VIBEWARE/actions/runs/46161445190/job/141029445557), 28 November 2026: exact checkout, UCRT64 GCC builds of all four executables (including SDL2 application), archive and extraction into a path containing spaces, 34 SHA-256 entries, extracted desktop session/split/core replay all PASS in the job log. The original six scenes regenerated; desktop session 120 ticks, 95,966 audible samples, split 1,152,000 paired PCM samples, 110,592,000 RGB565 comparisons and 200,000 concurrent publications. The recursive original-vs-regenerated media comparison failed to execute: `diff: command not found`, exit 127. Graphical smoke and artifact upload were skipped. No executable audition package was delivered by this run.

## Repair

Explicitly install MSYS2 `diffutils` alongside existing UCRT64 GCC/SDL2 packages; check `command -v diff` before build. Keep the existing exact recursive comparison, all replay/hash/rights steps, graphical smoke without compiler runtime PATH, and upload only after successful qualification. No new workflow or explicit rerun.

Local comparator negative controls (GNU diffutils 3.10, synthetic binary WAV/PPM-named fixtures, directory with spaces): `diff -qr media "reproduced media"` exit 0 for identical files, exit 1 for modified binary, exit 1 for missing file. PASS. This is a comparator fixture, not Windows or PHOSPHOR graphical execution. New exact-head hosted result pending; physical HOLD unchanged. Target adaptation, actual audio endpoint execution and independent QA still outstanding. No shared code adoption or other branch edits.
