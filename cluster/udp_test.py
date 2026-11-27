import socket
import subprocess
import sys


def ports():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as b:
        a.bind(('127.0.0.1', 0))
        b.bind(('127.0.0.1', 0))
        return a.getsockname()[1], b.getsockname()[1]


for mode in ('clean', 'faults'):
    a, b = ports()
    worker = subprocess.Popen([sys.argv[1], 'worker', str(b), str(a), mode], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        # The coordinator retries its handshake, so process startup ordering is immaterial.
        result = subprocess.run([sys.argv[1], 'controller', str(a), str(b), mode], capture_output=True, text=True, timeout=15)
        print(mode, result.stdout, result.stderr, sep='\n', flush=True)
        if result.returncode:
            raise SystemExit(result.returncode)
    finally:
        worker.terminate()
        out, _ = worker.communicate(timeout=5)
        print('worker:', out, flush=True)
