"""Exact XENOBIO M4R package verification and independent Windows replay."""
import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import subprocess
import zipfile

HEAD = '9a314395eac5bf577192bbdb3a58fb7cc628cf22'
TREE = '6098ecf645cb898cab3b157d5a3daa5a9ce383f9'
OUTER = 'ef262de8e063948dca74650fa5284f5c6cb8d5a274b96650557c87e15cc2fdd2'
INNER = '452079bd55b0f0923350efd44bbf127058b72458553e709c9a6e440d20f9136c'
BLOBS = {
    'CMakeLists.txt':'2edd1e2de0302c50585fcbc40d0c29fcbf9dc52e',
    'M2.md':'338a16e2ee37c6e0d2220ecd54029a463806145b',
    'M3.md':'f8029c8812b1119719c640ea112903aaf3c8f715',
    'M4R.md':'ec1092add0292df2811a5c1c7c603f577029e9b6',
    'README.md':'91b2b4f1e5e89397e7e5bb87d7356991968af42f',
    'WINDOWS.md':'8fc1da7d13d064d2b6928e62a8dded9d5d1d13c9',
    'core.hpp':'6953826440779ef79a53adc76d63b2829079ed88',
    'labeled.hpp':'c35cae93e683db681f5ccd32e34fece8e919d3a0',
    'replay.cpp':'bd315ebd1de6522aaea75fcd44d33e5b0a4c34f6',
    'runtime.hpp':'33d4229f36bf6c8d487dd6cb7c0581248befd981',
    'runtime_test.cpp':'79fef2e5482fef0a97958790c72d12885b4ef013',
    'target_input.hpp':'0242c9305b1dfff902dbca6e73abab198d231d68',
    'target_input_test.cpp':'f5098f32644dd41e6b39d8839ff8454e89926ea7',
    'windows.cpp':'8050853a826f06893acdbe893ff1a64a7ec99857',
}
EXES = {'Xenobio.exe','xenobio_replay.exe','xenobio_runtime_test.exe'}
MEDIA = {'baseline.wav','no-touch.wav','runtime.ppm'} | {f'scene{i}.{ext}' for i in range(6) for ext in ('wav','ppm')}

def need(ok, msg):
    if not ok:
        raise ValueError(msg)

def sha(b): return hashlib.sha256(b).hexdigest()

def blob(b):
    b = b.replace(b'\r\n',b'\n')
    return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()

def verify(raw):
    need(sha(raw)==OUTER,'outer digest')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        need(sorted(z.namelist())==['Xenobio-Windows.sha256','Xenobio-Windows.zip'],'outer members')
        inner=z.read('Xenobio-Windows.zip')
        need(sha(inner)==INNER==z.read('Xenobio-Windows.sha256').decode().strip().lower(),'inner digest')
    allowed=EXES | {'manifest.sha256','pe-dependencies.txt','source-commit.txt','source-tree.txt','WINDOWS.md'} | {'source/'+n for n in BLOBS} | {'media/'+n for n in MEDIA}
    with zipfile.ZipFile(io.BytesIO(inner)) as z:
        names=z.namelist()
        need(len(names)==len(set(names)) and set(names)==allowed|{'source/firmware/'},'unexpected archive layout')
        need(sum(x.file_size for x in z.infolist())<16*1024*1024,'archive size bound')
        files={n:z.read(n) for n in allowed}
    need(files['source-commit.txt'].decode().strip()==HEAD and files['source-tree.txt'].decode().strip()==TREE,'source pins')
    seen=set()
    for line in files['manifest.sha256'].decode().splitlines():
        m=re.fullmatch(r'([0-9a-fA-F]{64}) (.+)',line)
        need(m is not None,'manifest syntax')
        h,n=m.groups(); need(n in files and n not in seen and sha(files[n])==h.lower(),'manifest '+n); seen.add(n)
    need(seen==allowed-{'manifest.sha256'},'manifest coverage')
    for n,h in BLOBS.items(): need(blob(files['source/'+n])==h,'source blob '+n)
    need(files['WINDOWS.md']==files['source/WINDOWS.md'],'guide mismatch')
    print(f'PASS exact archives, {len(seen)} manifest entries, {len(BLOBS)} source blobs',flush=True)
    return files

def compare(expected, actual):
    need({p.name for p in actual.iterdir() if p.is_file()}==MEDIA,'media set')
    for n in MEDIA: need((actual/n).read_bytes()==expected['media/'+n],'media mismatch '+n)
    print('PASS 15 exact regenerated media',flush=True)

def run(files, out):
    need(os.name=='nt','Windows required')
    out.mkdir(parents=True,exist_ok=False)
    for n,b in files.items():
        p=out/n; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(b)
        need(p.read_bytes()==b,'extraction mismatch '+n)
    media=out/'QA regenerated media'; media.mkdir()
    for exe,args in [('xenobio_replay.exe',[media]),('xenobio_runtime_test.exe',[media/'runtime.ppm']),('Xenobio.exe',['--smoke'])]:
        subprocess.run([str(out/exe),*map(str,args)],cwd=out,check=True,timeout=120)
    compare(files,media)
    print('PASS extracted Windows replay and GDI smoke (no driver playback)',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('outer',type=Path); p.add_argument('--run',type=Path); p.add_argument('--compare',type=Path)
    a=p.parse_args(); files=verify(a.outer.read_bytes())
    if a.run: run(files,a.run.resolve())
    if a.compare: compare(files,a.compare)
