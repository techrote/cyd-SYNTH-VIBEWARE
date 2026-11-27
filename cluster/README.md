# CYD-CLUSTER-001: M0 portable vector-field prototype

Draft/HOLD, original source; no imported reference code. No firmware, audio, physical display, radio, or external transport in this initial slice. This is a bounded single-peer in-process experiment, not the final cluster architecture. It does not modify shared PLATFORM interfaces.

## Run

```sh
cmake -S cluster -B build/cluster -DCMAKE_BUILD_TYPE=Release
cmake --build build/cluster --config Release
ctest --test-dir build/cluster -C Release --output-on-failure -V
```

Alternatively GCC/Clang: `g++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic cluster/native.cpp -o cluster_native && ./cluster_native`. The executable runs deterministic fault cases, a wall-time host benchmark and ASCII density preview. No dependency besides standard C++17. No network or flash operations.

## Workload and wire contract

The job is an independent seeded integer vector-field particle cohort, up to 512 particles × 64 integration steps. The attractor coordinates represent a potential touch input. Result is an 8×8 occupancy grid of uint16 counts; sum must equal requested particle count. This is a coarse density field suitable for local visualization/control-rate mapping; no stateful XENOBIO or audio integration yet. The same kernel runs in local fallback and worker. Signed divisions truncate toward zero; positions wrap into uint16. The seed zero has a defined nonzero replacement. All protocol fields explicitly little-endian. One in-flight job, no heap use in core.

16-byte header: bytes 0–1 magic CV, 2 version (1), 3 type, 4–7 nonzero epoch, 8–11 sequence (zero only for hello/ready), 12–13 payload length, 14–15 reserved zero. Exact frame length required. Type 1 hello (0 payload); 2 ready (max particles and steps, 2×u16); 3 job (seed u32, particles, steps, x, y u16); 4 result (64×u16 row-major). Largest 144 bytes, beneath initial 250-byte cap. 28+144=172 application bytes per successful job excluding handshake, retransmissions and transport framing.

Coordinator owns monotonic deadline, uses wrap-safe uint32 elapsed with <=60s budget, and falls back to same local kernel at expiry or disconnect. Worker caches last request/result for idempotent identical retry, rejects older sequences and conflicting same-sequence payloads. Epochs are explicitly configured, nonzero and must be fresh across new sessions; create new objects on reset. Same-session disconnect/rejoin retains coordinator sequence. No distributed clock, automatic lease/rejoin, multi-peer orchestration, remote wall-clock deadline, integrity or authentication implemented; an actual transport must authenticate/authorize peer and supply correctly sized Frame. A result with internally consistent but false counts can pass validation; only trust authenticated workers. The codec is not a security boundary. Poll/compute must run on simulation/control loop, not audio callback; measured target worst-case fallback cost still required.

## Evidence and limits

The native test uses deterministic virtual deadlines and injected drop, delayed result, duplicate, old epoch/sequence, malformed lengths/headers, conflicting duplicate, saturation of single in-flight slot, disconnect/rejoin and millisecond wrap. It compares every completed grid to identical local computation. Benchmark reports actual host steady-clock percentiles of matched local computation and sequential in-process encoding/worker/decoding. This includes remote computation on the same CPU, not a real network RTT or proof of offload advantage. Host timings vary. Output digest is a repeatable witness, not cryptographic integrity. No physical timing claims.

Next: separate-process transport with explicit peer identity, bounded receive queue and loss/latency instrumentation; add target ESP-NOW profiles for classic ESP32/S3/C3 after reviewing exact Arduino/IDF callback versions. Expose time-varying touch-controlled heatmaps and local sound mapping with PLATFORM coordination. No target board flashing or deployment approval. Diviner measurements #8 not yet supplied.

## M1a: separate-process UDP experiment

The portable Link controller adds nonblocking send acceptance, periodic hello (100 ms), retry of the identical job (10 ms), application completion rather than MAC/socket acknowledgement, and automatic re-negotiation after an offload deadline triggers local fallback. No audio callback calls this code. Session epoch still configured out of band. The two real native processes bind only IPv4 loopback and filter the exact configured source IP/port: this is endpoint isolation, NOT cryptographic authentication or a LAN security model. No arbitrary network address or physical device access.

Build via CMake above (Python 3 required for the process test); run `python cluster/udp_test.py build/cluster/cluster_udp` (on Windows use the extracted EXE path). Two 100-job workloads: clean, then deterministic injected result loss (every 11th sequence all copies, every 7th first copy), duplicates (every 5th). Check every resulting grid against the same local kernel. A 60 ms coordinator deadline, 1 ms host polling sleep, and re-negotiation are included; timings are actual local UDP process wall times, not Wi-Fi, compute speedup or one-way latency. The test harness terminates the bounded worker after completion. The Windows console evaluator remains standalone; running its separate-process test requires Python.

Authored Linux GCC strict build, exact source blobs checked against commit: clean 100 remote / 0 fallback, p50/p95/p99/max 1078/1176/2240/3232 us. Faults 91 remote / 9 fallback, 58 retries, all exact matches; remote p50/p95/p99/max 1082/10830/12005/12005 us, fallback 60391/60597/60597/60597 us. Single run, 100 jobs per scenario; host scheduler included; no inferred device performance. New hosted qualification pending. Previous M0 hosted native/sanitizer/extracted Windows gates passed at 3a4f92e, run 46044967658.
