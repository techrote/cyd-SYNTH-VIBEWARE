"""QA batch 002: pinned desktop package audit and deterministic assembly.
Requires Python 3 and GNU objdump (PE support). No executable is launched.
Usage: python assemble002.py ARTIFACT_DIRECTORY OUTPUT.zip
"""
import json
import re
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path
from assemble import checked_zip, need, sha

INTEGRATION = '225be00c601d8aeec93c9051483f2b6b2cc3beb5'
CHECKOUT = 'a5376052bcb1a803edea7be007267a9cbfb71557'
TREE = '8bf304338573caccfc6925875ddfe1d048fb3826'
PINS = [
    ('PHASESPACE M3', 46102984182, 15246432643, INTEGRATION, CHECKOUT, TREE, '37e06a84c358d9c54fe4158482aecdfa1bf9c0ebb13f6678feb9a227285176a6', 'phasespace-windows.zip', 'a01568c9f02b1cacf614338906925a92e4796ab10f3ef491844db19ccefb22b3', 'phasespace.exe', None, 2),
    ('Xenoresonator M3', 46102984161, 15246670783, INTEGRATION, CHECKOUT, TREE, '5c2ab846eb2f6d21939a276d920b3e0b4119aa2d208835331925d43acec66f53', 'Xenoresonator.zip', 'ac5c4807494d2be57308fb3362dde716a9c74f262c245e9f2614f4bfb3226bfb', 'Xenoresonator.exe', 'SHA256SUMS.txt', 2),
    ('VECTORFIELD M10', 46104403890, 15245944682, 'e83a1edb686606c357f236e35488ba065b8865dc', '9cca681d606ca3be907bb5c7cb3f622c5de9ad18', '3b054e8bf8b853c922aaccdbe833d798cc6582ac', '026ca0e8d88ad66bc8f80ce96f77e35b94e090db7f4175690a34fcb229ccf093', 'cluster-native-windows.zip', '9cf94d9e342db625cccf1506c59ff1754da16921dfbc74f8ed5a66e8ea6d2a98', 'cluster_multi_field.exe', 'files.sha256', 16),
]
SYSTEM_DLLS = {'kernel32.dll','user32.dll','gdi32.dll','winmm.dll','ws2_32.dll'}
GUIDE = '''CYD VIBEWARE — owner audition batch 002 (Draft/HOLD)
Extract the collection and each instrument ZIP into separate directories.
PHASESPACE M3: phasespace.exe; mouse XY, 1–6 patches, Q quantize, M mode, R loop, X panic.
Xenoresonator M3: Xenoresonator.exe; materials, Couple, Strike/Rake, Panic.
VECTORFIELD M10: cluster_multi_field.exe; mouse hold/drag, M mute, Esc exit; local first.
Read each instrument README. Start at low system volume.
Independent static inspection: no separately installed VC++ runtime DLL imports found;
Xenoresonator uses Windows UCRT. Windows 10/11 x64 intended; no independent launch yet.
Python is needed for optional HYBRID multi-process test scripts, not instrument audition.
No firmware included; no flashing, redistribution or physical approval implied.
Author CI passed; independent host source checks and archive custody are separate gates.
Manifest records exact checkout/tree, original unchanged ZIPs and actual PE imports.
'''

def receipt(z, name):
    seen = set()
    for line in z.read(name).decode('utf-8-sig').splitlines():
        m = re.fullmatch(r'([0-9A-Fa-f]{64}) [ *](.+)', line)
        need(m is not None, 'invalid checksum receipt')
        h, path = m.groups()
        need(path not in seen and path != name and sha(z.read(path)) == h.lower(), 'receipt mismatch: '+path)
        seen.add(path)
    need(seen == {n for n in z.namelist() if not n.endswith('/') and n != name}, 'incomplete receipt')
    return len(seen)

def imports(data):
    with tempfile.TemporaryDirectory(prefix='QA PE ') as d:
        p = Path(d)/'audit binary.exe'
        p.write_bytes(data)
        out = subprocess.check_output(['objdump','-p',str(p)], text=True, timeout=15)
    need('file format pei-x86-64' in out, 'not Windows x64 PE')
    dlls = sorted(set(re.findall(r'DLL Name:\s+(\S+)',out,re.I)))
    need(bool(dlls), 'missing import table evidence')
    need(all(x.lower() in SYSTEM_DLLS or re.fullmatch(r'api-ms-win-crt-[a-z]+-l1-1-0\.dll',x.lower()) for x in dlls), 'unexpected DLL imports: '+str(dlls))
    return dlls

def verify(root, pin):
    name, run, aid, head, checkout, tree, outer_hash, inner_name, inner_hash, exe, receipt_name, exe_count = pin
    raw = (root/f'github-actions-artifact-{aid}.zip').read_bytes()
    need(sha(raw)==outer_hash, 'outer hash mismatch: '+name)
    with checked_zip(raw) as outer:
        need(set(outer.namelist())=={inner_name,inner_name[:-4]+'.sha256'}, 'unexpected outer payload')
        inner = outer.read(inner_name)
        need(sha(inner)==inner_hash and outer.read(inner_name[:-4]+'.sha256').decode().strip().lower()==inner_hash, 'inner hash mismatch')
    with checked_zip(inner) as z:
        names = [n for n in z.namelist() if not n.endswith('/')]
        need(all(Path(n).suffix.lower() in {'.exe','.wav','.ppm','.md','.txt','.hpp','.cpp','.py','.sha256'} for n in names), 'unexpected payload type')
        need(z.read('source-commit.txt').decode().strip()==checkout, 'checkout mismatch')
        need(z.read('source-tree.txt').decode().strip()==tree, 'tree mismatch')
        need('README.md' in names and exe in names, 'missing audition entry')
        covered = receipt(z,receipt_name) if receipt_name else 0
        pe = {n:imports(z.read(n)) for n in names if n.lower().endswith('.exe')}
        need(len(pe)==exe_count, 'wrong executable count')
    return inner, dict(name=name, source=head, checkout=checkout, tree=tree, run=run, artifact=aid, outer_sha256=outer_hash, inner_sha256=inner_hash, executable=exe, receipt_entries=covered, imports=pe, independent_windows='NOT RUN', physical='NOT RUN', rights='HOLD')

def assemble(root, output):
    verified = [verify(root,p) for p in PINS]
    with zipfile.ZipFile(output,'x',compression=zipfile.ZIP_STORED) as z:
        def put(n,b): z.writestr(zipfile.ZipInfo(n,(2026,11,28,0,0,0)),b)
        for p,(inner,_) in zip(PINS,verified): put(p[7],inner)
        put('README.txt',GUIDE)
        put('manifest.json',json.dumps([m for _,m in verified],indent=2)+'\n')
    with checked_zip(output.read_bytes()) as z:
        for p,(inner,_) in zip(PINS,verified): need(z.read(p[7])==inner,'assembly altered inner ZIP')
    print(json.dumps([m for _,m in verified],indent=2))
    print('Collection SHA-256:',sha(output.read_bytes()))

if __name__=='__main__':
    assemble(Path(sys.argv[1]),Path(sys.argv[2]))
