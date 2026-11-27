import subprocess
import sys
import socket
import time

def ports():
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as b:
        a.bind(("127.0.0.1",0))
        b.bind(("127.0.0.1",0))
        return str(a.getsockname()[1]),str(b.getsockname()[1])

a,b=ports()
leader=subprocess.Popen([sys.argv[1],"leader",a,b],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
try:
    time.sleep(.15)
    follower=subprocess.run([sys.argv[1],"follower",b,a],capture_output=True,text=True,timeout=12)
    out,_=leader.communicate(timeout=12)
    print("leader:",out,"follower:",follower.stdout,follower.stderr)
    if leader.returncode or follower.returncode:
        raise SystemExit(1)
finally:
    if leader.poll() is None:
        leader.kill()
        leader.wait()
