"""Run the actual Windows multi-cohort GDI consumer and two UDP workers."""
import re
import socket
import subprocess
import sys

sockets = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(4)]
try:
    for s in sockets:
        s.bind(("127.0.0.1", 0))
    ports = [s.getsockname()[1] for s in sockets]
finally:
    for s in sockets:
        s.close()
workers = []
try:
    for i, mode in enumerate(("clean", "faults")):
        workers.append(subprocess.Popen(
            [sys.argv[1], "worker", str(ports[2*i+1]), str(ports[2*i]), mode],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True))
    result = subprocess.run(
        [sys.argv[2], "--udp-smoke", *map(str, ports)], capture_output=True,
        text=True, timeout=18)
    print(result.stdout, result.stderr, sep="\n", flush=True)
    if result.returncode:
        raise RuntimeError("multi field GDI smoke failed")
    slots = re.findall(r"slot=(\d+) remote=(\d+) local=(\d+)", result.stdout)
    if len(slots) != 2 or any(int(s[1]) < 2 for s in slots):
        raise RuntimeError("both separate workers must contribute to field")
finally:
    for worker in workers:
        worker.terminate()
        out, _ = worker.communicate(timeout=5)
        print("worker:", out, flush=True)
