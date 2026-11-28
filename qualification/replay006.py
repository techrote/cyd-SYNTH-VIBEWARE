"""Independent exact-archive QA; desktop only. No physical or rights approval."""
import hashlib, io, os, re, subprocess, sys, zipfile
from pathlib import Path, PurePosixPath

PINS = {
    'grid': (15253554975, 'de3db28b2d51bf951afaa5f1d135cff413bb93896fe8e75eac4db00ef04ca87e', 'chronogrid-windows.zip', 'c0a866a71ecf0a6be958a04f9870b92d0a7fe069', 'chronogrid.exe', 'chronogrid_replay.exe', 'manifest.sha256', ()),
    'chrono': (15254076393, '128fce0651c2c0b42f1b2160e566d809235ea8df74585a09576fba97af88faa3', 'Chronomorph.zip', 'a42f33ecd37628fc6db64f0f0ffafc333f82af6d', 'Chronomorph.exe', 'replay.exe', 'SHA256SUMS.txt', ('runtime-test.exe', 'gate-test.exe')),
}

def need(ok, why):
    if not ok: raise ValueError(why)

def sha(raw): return hashlib.sha256(raw).hexdigest()

def members(raw):
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        names = set(); seen = set(); files = {}; total = 0
        for e in z.infolist():
            n = e.filename; p = PurePosixPath(n); folded = n.rstrip('/').casefold()
            need(n and not p.is_absolute() and '\\' not in n and ':' not in n and all(x not in ('', '.', '..') and x == x.rstrip(' .') for x in n.rstrip('/').split('/')), 'unsafe path: '+n)
            need(folded not in seen and (e.external_attr >> 16) & 0o170000 != 0o120000, 'duplicate/link: '+n)
            seen.add(folded)
            if e.is_dir(): continue
            total += e.file_size
            need(total <= 64*1024*1024 and len(seen) <= 256, 'archive limit')
            files[n] = z.read(e)
        return files

def git(root, *args): return subprocess.check_output(['git','-C',str(root),*args],text=True).strip()

def verify(raw, pin, tree, kind):
    aid, digest, archive, head, app, replay, receipt, extras = pin
    need(sha(raw)==digest, 'outer SHA256')
    outer=members(raw)
    expected={archive, archive[:-4]+'.sha256'} if kind=='grid' else {archive}
    need(set(outer)==expected, 'outer members')
    inner=outer[archive]
    if kind=='grid': need(outer[archive[:-4]+'.sha256'].decode('utf-8-sig').strip().lower()==sha(inner), 'inner SHA256')
    files=members(inner)
    need(all(PurePosixPath(n).suffix.lower() in {'.exe','.wav','.ppm','.md','.txt','.hpp','.cpp','.sha256'} for n in files), 'unexpected payload')
    need({n for n in files if n.lower().endswith('.exe')}=={app,replay,*extras}, 'executable set')
    if kind=='grid':
        need(files['source-commit.txt'].decode().strip()==head and files['source-tree.txt'].decode().strip()==tree, 'source mismatch')
    else:
        need(files['SOURCE.txt'].decode().split()==[head,tree], 'source mismatch')
    checked=set()
    for line in files[receipt].decode('utf-8-sig').splitlines():
        m=re.fullmatch(r'([0-9a-fA-F]{64}) (?:[ *])?(.+)',line)
        need(m is not None, 'receipt format'); h,n=m.groups()
        need(n in files and n not in checked and sha(files[n])==h.lower(), 'receipt mismatch '+n)
        checked.add(n)
    need(checked==set(files)-{receipt}, 'incomplete receipt')
    print('PASS archive', aid, 'inner SHA256', sha(inner), 'entries',len(checked), flush=True)
    return files

def media_equal(expected, actual):
    e={p.name:sha(p.read_bytes()) for p in expected.iterdir() if p.is_file()}
    a={p.name:sha(p.read_bytes()) for p in actual.iterdir() if p.is_file()}
    need(e and e==a, 'media mismatch'); print('PASS media',len(e),flush=True)

def run(kind, source, out):
    need(os.name=='nt','Windows required')
    pin=PINS[kind]; aid,_,_,head,app,replay,_,extras=pin
    need(git(source,'rev-parse','HEAD')==head,'checkout mismatch')
    tree=git(source,'rev-parse','HEAD^{tree}')
    out.mkdir(parents=True,exist_ok=False); rawpath=out/'outer.zip'
    with rawpath.open('wb') as f:
        subprocess.run(['gh','api',f'repos/techrote/cyd-SYNTH-VIBEWARE/actions/artifacts/{aid}/zip'],stdout=f,check=True,timeout=120)
    files=verify(rawpath.read_bytes(),pin,tree,kind)
    root=out/'extracted package'; root.mkdir()
    for n,b in files.items():
        p=root/n; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(b); need(sha(p.read_bytes())==sha(b),'extraction mismatch')
    if kind=='chrono':
        for n in files:
            if n.endswith(('.hpp','.cpp')):
                # Git checkout line endings may differ; compare normalized source text.
                need(files[n].decode().splitlines()==(source/'instruments/fm1-ports/chronomorph'/n).read_text().splitlines(),'packaged source mismatch '+n)
    media=root/'QA regenerated media'; media.mkdir()
    def execute(exe,*args):
        subprocess.run([str(root/exe),*map(str,args)],cwd=root,check=True,timeout=120)
    execute(replay,media); media_equal(root/'media',media)
    for exe in extras: execute(exe)
    execute(app,'--smoke')
    print('PASS independent extracted Windows',kind,head,tree,flush=True)

if __name__=='__main__':
    if sys.argv[1]=='compare': media_equal(Path(sys.argv[2]),Path(sys.argv[3]))
    else: run(sys.argv[1],Path(sys.argv[2]).resolve(),Path(sys.argv[3]).resolve())
