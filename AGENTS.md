# CYD SYNTH VIBEWARE — worker protocol

## Active deployment — owner-authorized 28 November 2026

The [programme activation](https://github.com/techrote/cyd-SYNTH-VIBEWARE/issues/1#issuecomment-7232382620) supersedes the historical proposed schedule below. Five active lanes: PLATFORM/PERFORMANCE (#2, :09), PORT (#13, :18), TOUCH (#3–6, :38), HYBRID (#9, :45), QA/MERGEDOWN (#7, :49). PORT owns standalone FM-1 interpretations in instruments/fm1-ports/; TOUCH owns original instruments and rotates creative mandates; HYBRID owns cluster/ and distributed transport; PLATFORM owns optional shared runtime and measured optimization; QA independently qualifies, packages, and coordinates integration. Local adapters remain independently buildable. Draft/HOLD, no unsolicited main merge or physical deployment. Historical six/eight-lane plans below are not active rosters.

Repository: techrote/cyd-SYNTH-VIBEWARE only. Programme #1. Read README.md, docs/ARCHITECTURE.md, docs/HARDWARE.md, docs/WORKERS.md, docs/QUALIFICATION.md and the live issue/PR for your mandate. Read the catalogue and roadmap when choosing new work. References are read-only and adoption is not automatic.

## Every invocation

1. Discover current main, live issue/PR heads, open reviews, CI/artifacts, predecessor work, and active claims. Resume the most valuable unfinished implementation in your lane; resolve real failures before opening another instrument. Check the actual repository, not a cached report.
2. Claim a bounded task with an issue comment recording role, run identifier, base SHA, intended paths and expiry/checkpoint. Comments are advisory, not atomic locks. Never concurrently write another run's branch; use a fresh isolated branch when overlapping or uncertain. Never force-push. Ref updates must use expected-head checks where supported. Refresh and reconcile changes before publishing.
3. Do substantive engineering: implement, build, execute, benchmark, compare alternatives, correct defects, deliver playable controls and evidence. Do not simulate a successful test with invented logs, produce reports instead of code, or manufacture issues as a substitute for implementation. If blocked, record exact blocker and do another useful scoped task.
4. Update the existing issue-scoped Draft/HOLD PR or open one when there is a real tested delta. Pin exact source, test commands, outcomes, artifacts, performance context and next executable task. Pending/skipped jobs are not passes. No unsolicited upstream writes, merge, release, or flash. Do not weaken qualification gates.
5. Keep one active instrument per creative lane until a playable milestone; thereafter at most two, only when QA can absorb them. Prefer closing real delivery gaps to creating more projects. End when useful work is complete; do not wait artificially.

## Ownership

| Lane | Primary ownership | Initial issue |
|---|---|---|
| PLATFORM | platform/, shared primitives/, desktop/, tools/, build configs and common CI | #2 |
| SYNTH | instruments/phasespace/; later synth instruments | #3 |
| GEN | instruments/chronogrid/; later composition instruments | #4 |
| CYBER | instruments/xenobio/; later dynamical systems | #5 |
| VISUAL | instruments/phosphor/; later visual instruments | #6 |
| QA | qualification/, catalogue/, integration coordination | #7 |
| HYBRID (optional) | scoped cluster/ and instruments/swarm/, transport adapters in coordination with PLATFORM | #9 |
| ACCEL (optional) | measured, agreed bottleneck in a separate PR | Assigned issue |

Paths are intended ownership boundaries, not proof of implemented directories. Shared interface changes need a concrete consumer and independent review. Creative workers may start native cores immediately, use thin local adapters, and submit separately scoped shared changes. QA may repair real defects in coordination; a different worker or independent job must reproduce authored fixes, or mark provisional. No silent overwrite of active work.

## Acceptance

All implementation remains Draft/HOLD until independent qualification and explicit owner approval. Main is accepted reference. Charter bootstrap alone is authorized on main. No repository-wide license asserted; prefer original code and record exact source, notices, license, changes and distribution obligations before reuse. References include unmerged work; no copying merely because visible. No unattended physical flash or board configuration changes. Physical results require actual hardware evidence. Follow docs/QUALIFICATION.md. Schedules are proposed, not active merely because documented.

## Performance and cluster work

#8 is owner-led Diviner characterisation (see docs/PERFORMANCE.md); consume measured results and links, do not write reference repo from a scheduled worker. #9 is optional HYBRID, wireless compute nodes and dual CYD (docs/CLUSTER.md). PLATFORM can progress a minimal shared contract while HYBRID is inactive; no creative lane depends on physical node availability. Local standalone delivery and independent QA remain required.
