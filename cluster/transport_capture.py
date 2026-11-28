"""Compare serial-only compute diagnostics from ESP-NOW and trusted-LAN UDP.

No device access. The input is privately captured job lines (no provisioning).
A label is supplied by the operator, not proof of transport/hardware identity.
"""
import argparse
import json
import math
import re
from pathlib import Path

JOB = re.compile(r"job seed=(\d+) mode=(local|remote) elapsed_us=(\d+) "
                 r"verify_local_us=(\d+) match=([01]) grid=([0-9a-fA-F]{8})")


def read_jobs(text, count):
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if "MISMATCH" in line or line.startswith("start failed"):
            raise ValueError("device reported a failed computation")
        if not line.startswith("job "):
            continue
        m = JOB.fullmatch(line)
        if not m:
            raise ValueError("malformed job line")
        seed, mode, elapsed, verify, match, digest = m.groups()
        if match != "1" or int(seed) != len(rows) + 1:
            raise ValueError("failed verification or nonconsecutive seed; capture one complete run")
        rows.append(dict(seed=int(seed), mode=mode, elapsed_us=int(elapsed),
                         verify_local_us=int(verify), grid=digest.lower()))
    if len(rows) != count:
        raise ValueError(f"expected {count} jobs, received {len(rows)}")
    return rows


def stats(values):
    if not values:
        return None
    a = sorted(values)
    return {"count": len(a), **{f"p{p}": a[math.ceil(p * len(a) / 100) - 1]
                                  for p in (50, 95, 99)}, "max": a[-1]}


def compare(captures, count, warmup=0):
    if not 0 <= warmup < count <= 100000 or len(captures) < 2:
        raise ValueError("require at least two captures, 1..100000 jobs, and fewer warmup jobs")
    if len({label for label, _ in captures}) != len(captures):
        raise ValueError("capture labels must be unique")
    baseline = None
    results = []
    for label, content in captures:
        rows = read_jobs(content, count)
        grids = [r["grid"] for r in rows]
        if baseline is None:
            baseline = grids
            if any(r["mode"] != "local" for r in rows):
                raise ValueError("first capture must be the all-local reference")
        elif grids != baseline:
            raise ValueError(f"{label}: digest mismatch against local reference (check XY/workload)")
        timed = rows[warmup:]
        remote = [r for r in timed if r["mode"] == "remote"]
        local = [r for r in timed if r["mode"] == "local"]
        results.append(dict(label=label, jobs=len(timed), remote=len(remote), local=len(local),
                            elapsed_us=stats([r["elapsed_us"] for r in timed]),
                            remote_elapsed_us=stats([r["elapsed_us"] for r in remote]),
                            local_elapsed_us=stats([r["elapsed_us"] for r in local]),
                            verify_local_us=stats([r["verify_local_us"] for r in timed]),
                            completed_remote_result_payload_bytes=128 * len(remote)))
    return dict(schema=1, warmup=warmup, expected_jobs_per_capture=count,
                matched_grid_digests=True, quantile="nearest rank",
                limitations=["Operator labels are not device provenance; record exact source, profile, board and network separately.",
                             "Grid digests are noncryptographic; each job must also report exact local match.",
                             "Elapsed time includes coordinator polling; no one-way latency, wire goodput or airtime inferred.",
                             "Local verification runs after completion and affects cadence, not elapsed_us."],
                captures=results)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--count", type=int, required=True)
    p.add_argument("--warmup", type=int, default=0)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("capture", nargs="+", metavar="LABEL=PATH", help="local reference first")
    args = p.parse_args()
    try:
        captures = []
        for item in args.capture:
            label, sep, path = item.partition("=")
            if not sep or not label or not path:
                raise ValueError("expected LABEL=PATH")
            captures.append((label, Path(path).read_text(encoding="utf-8")))
        report = compare(captures, args.count, args.warmup)
        args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    except (ValueError, OSError) as exc:
        p.exit(2, f"capture rejected: {exc}\n")


if __name__ == "__main__":
    main()
