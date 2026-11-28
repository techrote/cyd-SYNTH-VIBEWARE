"""Exercise the actual Windows GDI consumer against a real worker process."""
import socket
import subprocess
import sys

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as b:
    a.bind(("127.0.0.1", 0))
    b.bind(("127.0.0.1", 0))
    own, peer = a.getsockname()[1], b.getsockname()[1]
worker = subprocess.Popen([sys.argv[1], "worker", str(peer), str(own), "faults"],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
try:
    result = subprocess.run([sys.argv[2], "--udp-smoke", str(own), str(peer)],
                            capture_output=True, text=True, timeout=12)
    print(result.stdout, result.stderr, sep="\n", flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
finally:
    worker.terminate()
    out, _ = worker.communicate(timeout=5)
    print("worker:", out, flush=True)
