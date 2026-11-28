"""Matched two-cohort workloads with 0, 1 and 2 actual worker processes."""
import re
import socket
import subprocess
import sys


def ports(n):
    sockets = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(n)]
    try:
        for s in sockets:
            s.bind(("127.0.0.1", 0))
        return [s.getsockname()[1] for s in sockets]
    finally:
        for s in sockets:
            s.close()


def trial(nodes, fault):
    addresses = ports(2 * nodes)
    workers = []
    try:
        for i in range(nodes):
            mode = "faults" if fault and i == 1 else "clean"
            workers.append(subprocess.Popen(
                [sys.argv[2], "worker", str(addresses[2*i+1]), str(addresses[2*i]), mode],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True))
        result = subprocess.run(
            [sys.argv[1], str(nodes), "60", *map(str, addresses)],
            capture_output=True, text=True, timeout=30)
        print(f"nodes={nodes} fault={fault}\n{result.stdout}\n{result.stderr}", flush=True)
        if result.returncode:
            raise RuntimeError("coordinator failed")
        m = re.search(r"PASS nodes=\d+ matched=60 remote=(\d+) local=(\d+)", result.stdout)
        if not m:
            raise RuntimeError("missing matched-workload witness")
        remote, local = map(int, m.groups())
        if remote + local != 120:
            raise RuntimeError("completion accounting")
        if nodes == 0 and (remote, local) != (0, 120):
            raise RuntimeError("local baseline invalid")
        if nodes and not fault and remote < 110:
            raise RuntimeError("insufficient real remote completions")
        if fault and (local < 5 or remote < 90):
            raise RuntimeError("missing independent fallback witness")
        if nodes == 2:
            slots = re.findall(r"slot=(\d+) remote=(\d+) local=(\d+)", result.stdout)
            if len(slots) != 2 or any(int(s[1]) < 40 for s in slots):
                raise RuntimeError("both workers must execute useful computation")
    finally:
        for w in workers:
            w.terminate()
            out, _ = w.communicate(timeout=5)
            print("worker:", out, flush=True)


for n, fault in ((0, False), (1, False), (2, False), (2, True)):
    trial(n, fault)
