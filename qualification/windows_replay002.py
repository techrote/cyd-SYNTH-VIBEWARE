"""Independent replay of the exact batch 002 desktop packages; no firmware.
Run with three inner archives in INPUT, output to a new directory OUTPUT.
--audit-only verifies and extracts without executing Windows code.
"""
import os
import subprocess
import sys
from pathlib import Path
from assemble import checked_zip, need, sha
from assemble002 import PINS, receipt


def run(root, *args):
    command = [str(a) for a in args]
    print('RUN', command, flush=True)
    subprocess.run(command, cwd=root, check=True, timeout=90)


def exact_media(actual, expected):
    names = {p.name for p in expected.iterdir() if p.is_file()}
    need(names == {p.name for p in actual.iterdir() if p.is_file()}, 'media set mismatch')
    for name in sorted(names):
        need(sha((actual/name).read_bytes()) == sha((expected/name).read_bytes()), 'media mismatch: '+name)
    print('PASS exact regenerated media:', len(names), flush=True)


def extract(source, output):
    roots = []
    for pin in PINS:
        name, _, _, _, checkout, tree, _, archive, digest, _, checksums, _ = pin
        raw = (source/archive).read_bytes()
        need(sha(raw)==digest, 'wrong inner archive: '+name)
        root = output/name
        root.mkdir(parents=True,exist_ok=False)
        with checked_zip(raw) as z:
            need(z.read('source-commit.txt').decode().strip()==checkout, 'checkout mismatch')
            need(z.read('source-tree.txt').decode().strip()==tree, 'tree mismatch')
            if checksums: receipt(z, checksums)
            z.extractall(root)
            for n in z.namelist():
                if not n.endswith('/'):
                    need(sha((root/n).read_bytes())==sha(z.read(n)), 'extracted bytes mismatch: '+n)
        roots.append(root)
        print('PASS extraction:',name,flush=True)
    return roots


def replay(roots):
    need(os.name=='nt','Windows execution required; use --audit-only elsewhere')
    phase, xeno, cluster = roots
    for root, test, app in [(phase,'phasespace_replay.exe','phasespace.exe'),(xeno,'replay.exe','Xenoresonator.exe')]:
        media = root/'QA replay media'
        media.mkdir()
        run(root,root/test,media)
        exact_media(media,root/'media')
        run(root,root/app,'--smoke')
    for exe in ['cluster_native','cluster_pairing','cluster_spatial','cluster_spatial_bench','cluster_dual_test','cluster_multi_test','cluster_multi_field_test','cluster_bench_test','cluster_lan_test','cluster_ble_test','cluster_field_replay']:
        run(cluster,cluster/(exe+'.exe'))
    for exe in ['cluster_field','cluster_multi_field']:
        run(cluster,cluster/(exe+'.exe'),'--smoke')
    for script, executables in [('udp_test',['cluster_udp']),('dual_udp_test',['cluster_dual_udp']),('multi_udp_test',['cluster_multi_udp','cluster_udp']),('spatial_udp_test',['cluster_spatial']),('field_udp_smoke',['cluster_udp','cluster_field']),('multi_field_smoke',['cluster_udp','cluster_multi_field']),('bench_capture_test',[]),('transport_capture_test',[])]:
        run(cluster,sys.executable,cluster/(script+'.py'),*(cluster/(e+'.exe') for e in executables))
    print('PASS batch 002 extracted Windows replay and graphical smoke; not physical audio or radio',flush=True)


if __name__=='__main__':
    roots = extract(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve())
    if '--audit-only' not in sys.argv[3:]: replay(roots)
