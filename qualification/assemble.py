"""Assemble pinned owner-audition ZIPs, without firmware or executable launch.
Usage: python assemble.py DOWNLOAD_DIRECTORY OUTPUT.zip
Download the three outer Actions ZIPs as github-actions-artifact-<id>.zip.
This verifies archive custody, not independent Windows execution or rights.
"""
import hashlib
import io
import json
import re
import stat
import sys
import zipfile
from pathlib import Path, PurePosixPath

PINS = [
    ('PHASESPACE', 46098869310, 15243949553, '0139740e3db44496a779d6629b424397f9f53dfb', 'e9907a882b8f2e3e20a46904741b1b5f69157a8e', '71690024a96e65d5bbb9a21159d5903002e7c0d067743e2bd5783975c453827b', 'phasespace-windows.zip', '211cda4ffd2cfde703d55be1587d3d4a55c076732565ff94b4a949f958e64740', 'phasespace.exe'),
    ('Xenoresonator', 46097456019, 15243976808, '96e1ddd9fbe2e811ba23a3042fe914c8f6d5f25c', None, 'd50ab7e748a5b9d5cb2a21357e8586467719fa0a55a01af0fc43ea6ce019beb2', 'Xenoresonator.zip', None, 'Xenoresonator.exe'),
    ('VECTORFIELD', 46099393624, 15244407975, '6956bdbe599be5642be8c26cab1b7908e7b39d5d', '1e57740b1d49d0c8051ecd964419940854098e4a', '350b9adc580722b60287063382f0efe4dbef5e7781dad6da4b4ab4f2db4a3ecc', 'cluster-native-windows.zip', '78d2ca56384a430130c0d73bfaa2b3b6921db0584bddf4e29a11f40df92a50aa', 'cluster_multi_field.exe'),
]
GUIDE = '''CYD VIBEWARE — owner audition batch 001 (Draft/HOLD)
Extract this collection, then extract each instrument ZIP into its own directory.
No firmware is included. These are original author-built Windows x64 packages,
archive-verified by QA; independent Windows execution and physical testing remain open.
PHASESPACE: phasespace.exe; mouse XY, 1–6 patches, Q quantize, M mode, R loop, X panic.
Xenoresonator: Xenoresonator.exe; tap/drag, four material buttons, Couple, Panic.
VECTORFIELD: cluster_multi_field.exe; mouse hold/drag, M mute, Esc exit. Local first.
Follow the individual README files. Start at low system volume. Escape exits or mutes.
PHASESPACE and VECTORFIELD require the Microsoft Visual C++ x64 runtime (not bundled).
Xenoresonator imports Windows system/UCRT DLLs. No toolchain required to audition.
No redistribution authorization inferred; dependency notices/rights review remains open.
Windows smoke tests in author CI do not open a physical audio device.
Source/CI/artifact identities are in manifest.json. Original inner ZIPs are unmodified.
'''

def sha(data):
    return hashlib.sha256(data).hexdigest()

def need(ok, message):
    if not ok:
        raise ValueError(message)

def checked_zip(data):
    z = zipfile.ZipFile(io.BytesIO(data))
    seen = set()
    total = 0
    for i in z.infolist():
        n = i.filename
        parts = n.rstrip('/').split('/')
        need(n and not n.startswith('/') and '\\' not in n and ':' not in n and all(p not in ('', '.', '..') and p == p.rstrip(' .') for p in parts), 'unsafe member: ' + n)
        need(n.casefold() not in seen, 'duplicate member: ' + n)
        seen.add(n.casefold())
        need(not stat.S_ISLNK(i.external_attr >> 16), 'symlink: ' + n)
        total += i.file_size
        need(total <= 64 * 1024 * 1024, 'uncompressed size limit')
    need(z.testzip() is None, 'CRC failure')
    return z

def verify(root, pin):
    name, run, aid, head, checkout, outer_sha, inner_name, inner_sha, exe = pin
    raw = (root / f'github-actions-artifact-{aid}.zip').read_bytes()
    need(sha(raw) == outer_sha, name + ': outer SHA-256 mismatch')
    with checked_zip(raw) as outer:
        inner = outer.read(inner_name)
        if inner_sha:
            need(sha(inner) == inner_sha, name + ': inner SHA-256 mismatch')
            need(outer.read(inner_name[:-4] + '.sha256').decode().strip().lower() == inner_sha, 'inner receipt mismatch')
    with checked_zip(inner) as z:
        names = z.namelist()
        need(all(PurePosixPath(n).suffix.lower() in ('.exe','.wav','.ppm','.md','.txt','.hpp','.cpp','.py') for n in names if not n.endswith('/')), 'unexpected payload type')
        need(z.read(exe).startswith(b'MZ'), 'missing PE executable')
        need('README.md' in names, 'missing README')
        if checkout:
            need(z.read('source-commit.txt').decode().strip() == checkout, 'checkout identity mismatch')
        else:
            entries = z.read('SHA256SUMS.txt').decode().splitlines()
            covered = set()
            for line in entries:
                m = re.fullmatch(r'([0-9a-fA-F]{64}) [ *](.+)', line)
                need(m is not None, 'invalid checksum receipt')
                digest, path = m.groups()
                need(path not in covered and sha(z.read(path)) == digest.lower(), 'receipt mismatch: ' + path)
                covered.add(path)
            need(covered == {n for n in names if n.endswith(('.exe','.wav','.ppm'))}, 'incomplete receipt')
        receipt = dict(name=name, source=head, checkout=checkout, run=run, artifact=aid, outer_sha256=outer_sha, inner_sha256=sha(inner), files=len(names), executable=exe, independent_windows='NOT RUN', physical='NOT RUN', rights='HOLD')
    return inner, receipt

def assemble(root, output):
    verified = [verify(root, p) for p in PINS]
    manifest = [r for _, r in verified]
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_STORED) as z:
        def put(name, data):
            z.writestr(zipfile.ZipInfo(name, (2026, 11, 28, 0, 0, 0)), data)
        for pin, (inner, _) in zip(PINS, verified):
            put(pin[6], inner)
        put('README.txt', GUIDE)
        put('manifest.json', json.dumps(manifest, indent=2) + '\n')
    with zipfile.ZipFile(output) as z:
        need(z.testzip() is None, 'assembled collection CRC failure')
        for pin, (inner, _) in zip(PINS, verified):
            need(z.read(pin[6]) == inner, 'assembly altered inner ZIP')
    print(json.dumps(manifest, indent=2))
    print('Collection SHA-256:', sha(output.read_bytes()))

if __name__ == '__main__':
    assemble(Path(sys.argv[1]), Path(sys.argv[2]))
