import socket
import subprocess
import sys
import threading

def ports():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as b:
        a.bind(('127.0.0.1', 0))
        b.bind(('127.0.0.1', 0))
        return a.getsockname()[1], b.getsockname()[1]

for mode in ('clean', 'faults'):
    a, b = ports()
    receiver = subprocess.Popen([sys.argv[1], 'receive', str(a), str(b), mode],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        ready = []
        reader = threading.Thread(target=lambda: ready.append(receiver.stdout.readline()), daemon=True)
        reader.start()
        reader.join(timeout=5)
        if reader.is_alive() or ready != ['READY\n']:
            raise RuntimeError('checkpoint receiver did not become ready')
        subprocess.run([sys.argv[1], 'send', str(b), str(a), mode], check=True, timeout=10)
        output, _ = receiver.communicate(timeout=10)
        print(mode, output, flush=True)
        if receiver.returncode or 'PASS checkpoint_udp' not in output:
            raise RuntimeError('checkpoint transfer failed')
    finally:
        if receiver.poll() is None:
            receiver.kill()
            receiver.communicate()
