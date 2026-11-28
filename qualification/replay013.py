"""Independent exact integrated Windows replay. No physical or acceptance inference."""
import hashlib, io, os, pathlib, re, subprocess, sys, tempfile, zipfile

HEAD = '319664f449f2c4e77ced3ff80f5a743e4ad36b59'
TREE = '5a4b413929afe0f420e80f7c2e71c42f1136ebce'
PINS = {
    'PHASESPACE': (15263486904, 'b197891f9c866d50302445be7f31cd72fd0c831fe95cbf2416c761966852110c', '5d35598b9a5d7038436975f17eef430bf8d6cc359b61e56bc6d5d5fb392fbaf4', 'phasespace.exe', 'phasespace_replay.exe', ()),
    'CHRONOGRID': (15263715490, 'c6fb112dd52ef845dfab5a4b1ae8664dbfe66af294c090dab4415625da9dd313', '4f7e3fdfc8a3658cb3d7ca072c678c5ddc75938e6c9dcdf1dcf5124a97197c39', 'chronogrid.exe', 'chronogrid_replay.exe', ()),
    'Xenoresonator': (15263133705, '3580249f2de06be03ea137504e7d89ec2dc74e8737aa1f4201de17a27f48bb21', '50334ebf59a3ea84224f403af8aa5ee60e3041356416ba28cef766b136d53f59', 'Xenoresonator.exe', 'replay.exe', ()),
    'Chronomorph': (15263302248, 'e3cb193a9379999bc850f18108795de3ccb5842bb487a5adf189a704e9e8a1d2', 'e3268724cacc77f3a22cfc1945a8de2774fbadb88aca82055392c99ac41a1f2c', 'Chronomorph.exe', 'replay.exe', ('runtime-test.exe', 'gate-test.exe')),
    'XENOBIO': (15262659578, 'bb6b3c6349f11ad89806dbfce3e3c40722e0aae40a66e77cb5decc2c904cbb8f', 'ecda1297486bb1efa32ac814b76bc52e342ab2455d25b0bff9524960fe11e621', 'Xenobio.exe', 'xenobio_replay.exe', ('xenobio_runtime_test.exe',)),
}

def need(ok, msg):
    if not ok: raise ValueError(msg)

def sha(data): return hashlib.sha256(data).hexdigest()

def verify(name, raw):
    _, _, digest, app, replay, extra = PINS[name]
    need(sha(raw) == digest, name + ' inner SHA256')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        f = {}
        seen = set()
        for i in z.infolist():
            n = i.filename.rstrip('/')
            need(n and all(p not in ('', '.', '..') and not p.endswith((' ', '.')) for p in n.split('/')) and not any(c in n for c in ('\\', ':')), 'unsafe path')
            need(n.casefold() not in seen and (i.external_attr >> 16) & 0o170000 != 0o120000, 'duplicate or symlink')
            seen.add(n.casefold())
            need(i.file_size < 32*1024*1024, 'oversize')
            if not i.is_dir(): f[n] = z.read(i)
    manifest = [n for n in ('SHA256SUMS.txt','manifest.sha256') if n in f]
    need(len(manifest) == 1, 'manifest count')
    m = manifest[0]
    seen = set()
    for line in f[m].decode('utf-8-sig').splitlines():
        match = re.fullmatch(r'([0-9a-fA-F]{64})[ \t]+\*?(.+)', line)
        need(match is not None, 'manifest syntax')
        h,n = match.groups()
        need(n not in seen and n in f and sha(f[n]) == h.lower(), 'manifest ' + n)
        seen.add(n)
    need(seen == set(f)-{m}, 'manifest coverage')
    pins = f['SOURCE.txt'].decode().splitlines() if 'SOURCE.txt' in f else [f[n].decode().strip() for n in ('source-commit.txt','source-tree.txt')]
    need(pins == [HEAD,TREE], 'source pins')
    need({n for n in f if n.lower().endswith('.exe')} == {app,replay,*extra}, 'executable inventory')
    need(not any(n.lower().endswith(('.bin','.elf','.hex','.uf2')) for n in f), 'firmware payload')
    print(name, 'verified', len(seen), 'manifest entries', flush=True)
    return f

def recover(name):
    aid,digest,*_ = PINS[name]
    raw = subprocess.check_output(['gh','api',f'repos/techrote/cyd-SYNTH-VIBEWARE/actions/artifacts/{aid}/zip'], timeout=180)
    need(sha(raw) == digest, name + ' outer SHA256')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        names = [n for n in z.namelist() if n.endswith('.zip')]
        need(len(names) == 1, 'inner archive count')
        return z.read(names[0])

def run(name, f, root):
    need(os.name == 'nt', 'Windows required')
    _,_,_,app,replay,extra = PINS[name]
    out = root/name/'extracted package'
    out.mkdir(parents=True)
    for n,b in f.items():
        p = out/n
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_bytes(b)
        need(p.read_bytes() == b, 'extraction readback ' + n)
    media = out/'QA regenerated media'
    media.mkdir()
    def execute(exe,*args):
        subprocess.run([str(out/exe),*map(str,args)],cwd=out,check=True,timeout=180)
    execute(replay,media)
    for exe in extra:
        execute(exe,media/'runtime.ppm') if name == 'XENOBIO' else execute(exe)
    expected = {n[6:]:b for n,b in f.items() if n.startswith('media/') and n.endswith(('.wav','.ppm'))}
    actual = {p.name:p.read_bytes() for p in media.iterdir() if p.suffix in ('.wav','.ppm')}
    execute(app,'--smoke')
    need(actual == expected, name + ' exact media mismatch')
    print(name, 'PASS extracted Windows replay,',len(expected),'media, GDI smoke (not physical audio)',flush=True)

if __name__ == '__main__':
    if len(sys.argv) == 2:
        with zipfile.ZipFile(sys.argv[1]) as z:
            for name in PINS: verify(name,z.read('Standalone/'+name+'.zip'))
    else:
        need(os.name == 'nt', 'Windows required')
        failed = []
        with tempfile.TemporaryDirectory(prefix='QA integrated replay ') as tmp:
            for name in PINS:
                try: run(name,verify(name,recover(name)),pathlib.Path(tmp))
                except Exception as e:
                    failed.append(name)
                    print(name,'FAIL',repr(e),flush=True)
        need(not failed,'failed profiles: '+', '.join(failed))
