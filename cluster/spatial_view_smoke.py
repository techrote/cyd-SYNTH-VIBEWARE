"""Actual Windows graphical reaction viewer + snapshot or checkpoint UDP blackout."""
import socket
import subprocess
import sys
import threading

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as b:
    a.bind(('127.0.0.1', 0))
    b.bind(('127.0.0.1', 0))
    own, peer = a.getsockname()[1], b.getsockname()[1]
checkpoint = len(sys.argv) == 4 and sys.argv[3] == 'checkpoint'
viewer = subprocess.Popen([sys.argv[2], '--checkpoint-smoke' if checkpoint else '--udp-smoke', str(own), str(peer)],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
try:
    ready = []
    thread = threading.Thread(target=lambda: ready.append(viewer.stdout.readline()), daemon=True)
    thread.start()
    thread.join(timeout=5)
    if thread.is_alive() or ready != ['READY\n']:
        raise RuntimeError('reaction viewer did not become ready')
    args = [sys.argv[1], 'send', str(peer), str(own), 'view-faults' if checkpoint else 'faults']
    if not checkpoint:
        args.append('unused.ppm')
    subprocess.run(args, check=True, timeout=15)
    out, _ = viewer.communicate(timeout=10)
    print(out, flush=True)
    if viewer.returncode:
        raise SystemExit(viewer.returncode)
finally:
    if viewer.poll() is None:
        viewer.kill()
        viewer.wait(timeout=5)
