"""Offline parser for CYD profiling logs. No serial writes, credentials, or flashing."""
import argparse
import csv
import json
import math
from pathlib import Path

BATCHES = 64
BINS = 24
HISTOGRAMS = {"control","loop","slice","touch","frame_period","audio_compute","audio_write","audio_period"}

def fields(line):
    out = {}
    for token in line.split()[2:]:
        key, sep, value = token.partition("=")
        if not sep or not value or key in out:
            raise ValueError(f"malformed benchmark field: {token}")
        out[key] = value
    return out

def number(d, key):
    n = int(d[key])
    if n < 0:
        raise ValueError(f"negative {key}")
    return n

def percentile(values, p):
    return sorted(values)[math.ceil(len(values)*p/100)-1]

def summarize(values):
    return {f"p{p}": percentile(values, p) for p in (50,95,99,100)}

def hist_summary(counts):
    n = sum(counts)
    result = {"samples": n, "overflow": counts[-1]}
    for p in (50,95,99,100):
        bound = None
        if n:
            rank = math.ceil(n*p/100)
            for i, c in enumerate(counts):
                rank -= c
                if rank <= 0:
                    bound = 2**i if i < BINS-1 else None
                    break
        result[f"p{p}_upper_us"] = bound
    return result

def parse(text):
    runs, run = [], None
    for raw in text.splitlines():
        line = raw.strip()
        if not line.startswith("bench "):
            continue
        kind = line.split()[1]
        if kind not in ("begin","sample","hist","peer","end"):
            continue
        d = fields(line)
        if kind == "begin":
            if run is not None:
                raise ValueError("truncated run before next begin")
            header = {k: number(d,k) for k in ("schema","nodes","remote","audio","count","steps","particles")}
            if (header["schema"]!=2 or header["nodes"]>2 or header["remote"]>1 or
                header["audio"]>1 or header["count"]!=BATCHES or header["steps"]!=64 or
                header["particles"]!=1024 or (header["remote"] and not header["nodes"])):
                raise ValueError("unsupported or inconsistent benchmark header")
            run = {"header":header,"samples":[],"histograms":{},"peers":{}}
        elif run is None:
            raise ValueError("benchmark record outside run")
        elif kind == "sample":
            row = {k:number(d,k) for k in ("seed","us","remote")}
            row["grid"] = d["grid"].lower()
            if (len(row["grid"])!=8 or any(c not in "0123456789abcdef" for c in row["grid"]) or
                row["seed"]!=len(run["samples"])+1 or row["seed"]>BATCHES or row["remote"]>2 or
                (row["remote"] and not run["header"]["remote"])):
                raise ValueError("invalid sample sequence, digest or remote count")
            run["samples"].append(row)
        elif kind == "hist":
            name=d["name"]
            counts=[int(v) for v in d["counts"].split(",")]
            if name not in HISTOGRAMS or name in run["histograms"] or len(counts)!=BINS or min(counts)<0:
                raise ValueError("invalid histogram")
            run["histograms"][name]={"counts":counts,**hist_summary(counts)}
        elif kind == "peer":
            slot=number(d,"slot")
            if slot>=run["header"]["nodes"] or str(slot) in run["peers"]:
                raise ValueError("invalid peer slot")
            run["peers"][str(slot)]={k:number(d,k) for k in ("sent","retry","remote","local","busy")}
        elif kind == "end":
            end={k:number(d,k) for k in ("count","elapsed_us","frames","audio_blocks","audio_errors",
                "heap_min_sample","tx_fail","rx_drop","rx_reject")}
            end["reason"]=d["reason"]
            if end["count"]!=len(run["samples"]) or end["elapsed_us"]==0 or set(run["histograms"])!=HISTOGRAMS or len(run["peers"])!=run["header"]["nodes"]:
                raise ValueError("incomplete or inconsistent benchmark")
            run["end"]=end
            run["complete"]=end["reason"]=="complete" and end["count"]==BATCHES
            if end["reason"]=="complete" and not run["complete"]:
                raise ValueError("claimed complete with missing samples")
            run["remote_cohorts"]=sum(x["remote"] for x in run["samples"])
            run["local_cohorts"]=2*len(run["samples"])-run["remote_cohorts"]
            if run["samples"]:
                run["batch_us"]=summarize([x["us"] for x in run["samples"]])
            run["frame_rate_hz"]=end["frames"]*1e6/end["elapsed_us"]
            runs.append(run)
            run=None
    if run is not None:
        raise ValueError("truncated benchmark")
    if not runs:
        raise ValueError("no benchmark records")
    return runs

def compare(runs):
    # Require a completed radio-off reference in each audio mode. A matching
    # 32-bit digest is a useful witness, not proof of full-grid equality.
    reports=[]
    for i, run in enumerate(runs):
        h=run["header"]
        refs=[r for r in runs if r["complete"] and r["header"]["nodes"]==0 and r["header"]["audio"]==h["audio"]]
        report={"run":i+1,"complete":run["complete"],"reference_count":len(refs),"digest_match":None}
        if run["complete"] and refs:
            hashes=[x["grid"] for x in run["samples"]]
            report["digest_match"]=all(hashes==[x["grid"] for x in r["samples"]] for r in refs)
        reports.append(report)
    return reports

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("logs",nargs="+",type=Path)
    p.add_argument("--output",required=True,type=Path,help="JSON report path")
    p.add_argument("--firmware-sha256",required=True)
    p.add_argument("--board",required=True)
    args=p.parse_args()
    digest=args.firmware_sha256.lower()
    if len(digest)!=64 or any(c not in "0123456789abcdef" for c in digest):
        p.error("firmware SHA-256 must be 64 hexadecimal characters")
    runs=[]
    for path in args.logs:
        for run in parse(path.read_text(encoding="utf-8",errors="strict")):
            run["source_log"]=str(path)
            runs.append(run)
    comparisons=compare(runs)
    args.output.write_text(json.dumps({"schema":2,"firmware_sha256":digest,"board":args.board,
        "evidence":"operator-supplied serial log; physical provenance not independently verified",
        "runs":runs,"comparison":comparisons},indent=2)+"\n",encoding="utf-8")
    with args.output.with_suffix(".csv").open("w",newline="",encoding="utf-8") as fp:
        writer=csv.DictWriter(fp,fieldnames=["run","nodes","audio","seed","us","grid","remote"])
        writer.writeheader()
        for i,r in enumerate(runs,1):
            for s in r["samples"]:
                writer.writerow({"run":i,"nodes":r["header"]["nodes"],"audio":r["header"]["audio"],**s})
    if any(not r["complete"] or r["digest_match"] is not True for r in comparisons):
        return 2
    return 0

if __name__=="__main__":
    raise SystemExit(main())
