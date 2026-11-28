"""Independent replay of the two frozen Windows audition candidates."""
import argparse, hashlib, io, os, pathlib, re, subprocess, tempfile, zipfile

PINS = {
    'Xenoresonator': ('767f7ce5f66fb742a9530b426fd9513c5eb67fdb', '8771d94734efbd5af28f784a27c67ef498142a98', 15269132918, '19396af1add6a97b335a97da2353c36e81b67521af60eec94b9c3390a02eccb1', 'e384cb22763be96f2076302a82c7bfd46d5446665e7490d4b4d1311aa95f0154', 'Xenoresonator-767f7ce5-Windows-audition.zip', ()),
    'Ecophon': ('d353906d58b960fb987b18524d25dfe831078e60', '7f104198c8e911cea0c0d77abbdf3adfb586b999', 15265001841, 'e1963f29cc1043625806e2756bd94a50aee665b80fa58815dd17eded0608bf7a', '24a7f8ec9b6891920d267eea18b6ff198d231a64fa2167b30ec259500fdc56e9', 'Ecophon-d353906d-Windows-preview.zip', ('runtime-test.exe', 'recovery-test.exe')),
}

def need(ok, msg):
    if not ok: raise ValueError(msg)

def sha(b): return hashlib.sha256(b).hexdigest()

def verify(name, raw):
    head, tree, _, _, digest, _, extra = PINS[name]
    need(sha(raw) == digest, name + ' inner digest')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        files, seen = {}, set()
        for i in z.infolist():
            n = i.filename.rstrip('/')
            need(n and not any(c in n for c in ('\\', ':')) and all(p not in ('', '.', '..') and not p.endswith((' ', '.')) for p in n.split('/')), 'unsafe path')
            need(n.casefold() not in seen and (i.external_attr >> 16) & 0o170000 != 0o120000, 'duplicate/symlink')
            seen.add(n.casefold())
            need(i.file_size < 32*1024*1024, 'oversize entry')
            if not i.is_dir(): files[n] = z.read(i)
    seen = set()
    for line in files['SHA256SUMS.txt'].decode('utf-8-sig').splitlines():
        m = re.fullmatch(r'([0-9a-fA-F]{64})[ \t]+\*?(.+)', line)
        need(m is not None, 'manifest syntax')
        h,n = m.groups()
        need(n not in seen and n in files and sha(files[n]) == h.lower(), 'manifest ' + n)
        seen.add(n)
    need(seen == set(files)-{'SHA256SUMS.txt'}, 'manifest coverage')
    pins = files['SOURCE.txt'].decode().splitlines() if 'SOURCE.txt' in files else [files[n].decode().strip() for n in ('source-commit.txt','source-tree.txt')]
    need(pins == [head,tree], 'source pins')
    need({n for n in files if n.lower().endswith('.exe')} == {name+'.exe','replay.exe',*extra}, 'executable inventory')
    need(not any(n.lower().endswith(('.bin','.elf','.hex','.uf2')) for n in files), 'firmware payload')
    print(name, 'PASS verified', len(seen), 'manifest entries', flush=True)
    return files

def recover(name):
    _,_,aid,digest,*_ = PINS[name]
    raw = subprocess.check_output(['gh','api',f'repos/techrote/cyd-SYNTH-VIBEWARE/actions/artifacts/{aid}/zip'], timeout=180)
    need(sha(raw) == digest, name+' outer digest')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        names = [n for n in z.namelist() if n.endswith('.zip')]
        need(len(names)==1, 'inner archive count')
        return z.read(names[0])

def compare(name, files, media):
    expected = {n[6:]:b for n,b in files.items() if n.startswith('media/') and n.endswith(('.wav','.ppm'))}
    actual = {p.name:p.read_bytes() for p in media.iterdir() if p.suffix in ('.wav','.ppm')}
    need(actual == expected, name+' exact media mismatch')
    print(name, 'PASS exact media', len(expected), flush=True)

def execute(name, files, root):
    need(os.name == 'nt', 'Windows required')
    out = root/name/'extracted package'
    out.mkdir(parents=True)
    for n,b in files.items():
        p = out/n
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(b)
        need(p.read_bytes()==b, 'extraction readback '+n)
    media = out/'QA regenerated media'
    media.mkdir()
    env = {k:v for k,v in os.environ.items() if k.upper() not in ('GH_TOKEN','GITHUB_TOKEN','GH_ENTERPRISE_TOKEN','GITHUB_ENTERPRISE_TOKEN')}
    def run(exe,*args):
        subprocess.run([str(out/exe),*map(str,args)], cwd=out, env=env, check=True, timeout=180)
    run('replay.exe',media)
    for exe in PINS[name][-1]: run(exe)
    compare(name,files,media)
    run(name+'.exe','--smoke')
    print(name,'PASS extracted Windows graphical replay (not physical audition)',flush=True)

def main():
    p=argparse.ArgumentParser()
    p.add_argument('directory',type=pathlib.Path)
    p.add_argument('--recover',action='store_true')
    p.add_argument('--execute',action='store_true')
    p.add_argument('--compare',type=pathlib.Path,nargs=2,metavar=('XENO_MEDIA','ECOPHON_MEDIA'))
    a=p.parse_args()
    a.directory.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='QA batch 016 ') as tmp:
        for idx,(name,pin) in enumerate(PINS.items()):
            path=a.directory/pin[-2]
            raw=recover(name) if a.recover else path.read_bytes()
            f=verify(name,raw)
            if a.recover: path.write_bytes(raw)
            if a.execute: execute(name,f,pathlib.Path(tmp))
            if a.compare: compare(name,f,a.compare[idx])

if __name__=='__main__': main()
