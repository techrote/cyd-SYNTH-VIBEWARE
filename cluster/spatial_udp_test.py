import pathlib
import socket
import subprocess
import sys
import tempfile
import threading

def ports():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as b:
        a.bind(('127.0.0.1', 0))
        b.bind(('127.0.0.1', 0))
        return a.getsockname()[1], b.getsockname()[1]

with tempfile.TemporaryDirectory() as tmp:
    for mode in ('clean', 'faults'):
        a, b = ports()
        path = str(pathlib.Path(tmp) / (mode + '.ppm'))
        viewer = subprocess.Popen([sys.argv[1], 'receive', str(a), str(b), mode, path],
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            # The viewer has a bounded 2.3-second lifetime; no indefinite handshake.
            ready = []
            thread = threading.Thread(target=lambda: ready.append(viewer.stdout.readline()), daemon=True)
            thread.start()
            thread.join(timeout=5)
            if thread.is_alive() or ready != ['READY\n']:
                raise RuntimeError('viewer did not become ready')
            subprocess.run([sys.argv[1], 'send', str(b), str(a), mode, path], check=True, timeout=10)
            out, _ = viewer.communicate(timeout=10)
            print(mode, out, flush=True)
            if viewer.returncode:
                raise SystemExit(viewer.returncode)
            data = pathlib.Path(path).read_bytes()
            assert data.startswith(b'P6\n320 240\n255\n') and len(data) == 15 + 320*240*3
        finally:
            if viewer.poll() is None:
                viewer.kill()
                viewer.communicate()
