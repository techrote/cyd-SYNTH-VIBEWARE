"""QA017: verify frozen desktop archives and non-installable target receipts."""
import hashlib, io, json, re, sys, zipfile
from pathlib import Path, PurePosixPath

HEAD = '260098ea439ccd0f34fef26d454e89f31de0720e'
TREE = '2274b606cfd0e6cf8c503adeeff681e8eaed6602'
PINS = {
    15275269110: ('Chronogrid', 'bd4c07ded004f2921e4bd288bacb8593a61e7af04a38e8ab62f89ad61ee6f69b', 'chronogrid.exe'),
    15275568149: ('Chronomorph', '792f29cd94013cd5eb32e36696aebc988f1f0ccd09d9b3e51461f3334b746670', 'Chronomorph.exe'),
    15275591688: ('Xenoresonator', '05586d362b09b5d3de984de33695fee4b2a3357ffb91e2c9ec50801b7f0e32dc', 'Xenoresonator.exe'),
    15275681368: ('Ecophon', '516bffd6aa3c321443f7e62d61f2e768f523868da5c97020240019f61eb3b09c', 'Ecophon.exe'),
    15275736208: ('Xenobio', '36e7e89b7f662a83792ed5d58de7a529878efa863c1eae83edc65a7afdb69fed', 'Xenobio.exe'),
    15276005706: ('Phasespace', '1c2f174d3d214d981a998c1d62b750474b7b0bedf96f9c6cde99cdb97f63606d', 'phasespace.exe'),
}
TARGETS = {
    15274562553: '174a7a839d93b7578432d8f5542907f227dea403007b6f02a62bd2b782ff13f1',
    15275992757: '73fd6d26a6893b0fce176a36d3d853cb91016ee0be45bdd75d63bc5113e51b71',
}

def sha(b):
    return hashlib.sha256(b).hexdigest()

def require(ok, message):
    if not ok:
        raise ValueError(message)

def members(raw):
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        out, seen = {}, set()
        for i in z.infolist():
            n = i.filename.rstrip('/')
            require(n and not n.startswith('/') and '\\' not in n and ':' not in n and all(p not in ('', '.', '..') for p in n.split('/')), 'unsafe path: '+n)
            require(n.casefold() not in seen, 'duplicate path: '+n)
            seen.add(n.casefold())
            if not i.is_dir():
                require(PurePosixPath(n).suffix.lower() not in ('.bin', '.elf', '.hex', '.uf2', '.img'), 'firmware member: '+n)
                out[n] = z.read(i)
        return out

def check_manifest(files, name, prefix=''):
    entries = set()
    for line in files[name].decode('utf-8-sig').splitlines():
        h, p = line.split(maxsplit=1)
        p = prefix + p.lstrip('*').removeprefix('./').replace('\\', '/')
        require(p not in entries and p in files and sha(files[p]) == h.lower(), 'manifest mismatch: '+p)
        entries.add(p)
    expected = {n for n in files if n.startswith(prefix) and n != name}
    require(entries == expected, 'manifest coverage: '+name)
    return len(entries)

def outer(path, digest):
    raw = path.read_bytes()
    require(sha(raw) == digest, 'outer digest: '+path.name)
    return members(raw)

def desktop(files, exe):
    names = [n for n in files if n.endswith('.zip')]
    require(len(names) == 1, 'one original desktop archive required')
    raw = files[names[0]]
    for n in files:
        if n.endswith('.sha256'):
            require(files[n].decode().split()[0].lower() == sha(raw), 'inner sidecar mismatch')
    inner = members(raw)
    m = next(n for n in ('SHA256SUMS.txt', 'manifest.sha256') if n in inner)
    count = check_manifest(inner, m)
    identity = inner['SOURCE.txt'].decode().splitlines() if 'SOURCE.txt' in inner else [inner[n].decode().strip() for n in ('source-commit.txt', 'source-tree.txt')]
    require(identity == [HEAD, TREE], 'desktop identity')
    require(exe in inner and inner[exe][:2] == b'MZ', 'desktop executable missing')
    return raw, count

def target(files, integrated):
    count = 0
    if integrated:
        count += check_manifest(files, 'SHA256SUMS')
        require(files['batch-identity.txt'].decode().splitlines() == ['commit='+HEAD, 'tree='+TREE, 'physical=HOLD'], 'batch identity')
        receipts = [n for n in files if n.endswith('/receipt.json')]
        require(len(receipts) == 7, 'seven target receipts required')
        for n in receipts:
            r = json.loads(files[n]); p = n.rsplit('/', 1)[0]+'/'
            require(r['head'] == HEAD and r['tree'] == TREE and r['profile']+'/' == p, 'receipt identity')
            count += check_manifest(files, p+'SHA256SUMS', p)
    else:
        require(files['profile-status.tsv'].decode().splitlines() == ['cyd-port-xeno\tPASS', 'cyd-port-ecophon\tPASS', 'cyd-port-ecophon-timer\tPASS'], 'independent profile status')
        for p, h, t in [('cyd-port-xeno/', '767f7ce5f66fb742a9530b426fd9513c5eb67fdb', '8771d94734efbd5af28f784a27c67ef498142a98'), ('cyd-port-ecophon/', 'd353906d58b960fb987b18524d25dfe831078e60', '7f104198c8e911cea0c0d77abbdf3adfb586b999'), ('cyd-port-ecophon-timer/', 'd353906d58b960fb987b18524d25dfe831078e60', '7f104198c8e911cea0c0d77abbdf3adfb586b999')]:
            require(files[p+'source-commit.txt'].decode().strip() == h and files[p+'source-tree.txt'].decode().strip() == t, 'independent identity')
            count += check_manifest(files, p+'manifest.sha256', p)
    return count

def run(root, output):
    guide = ['CYD SYNTH VIBEWARE — six standalone Windows auditions', 'Draft/HOLD; Windows only. No ESP32 firmware. Extract each original ZIP into its own folder, then run the named instrument executable. See each package guide. Author Windows CI PASS; fresh independent integrated Windows replay not yet run. Physical execution BLOCKED.', 'Source '+HEAD, 'Tree '+TREE, '']
    payload = {}
    for i, (name, digest, exe) in PINS.items():
        raw, count = desktop(outer(root / f'qa-{i}.zip', digest), exe)
        print(name, count, sha(raw))
        payload[name+'.zip'] = raw
        guide.append(f'{name}: {exe}; original artifact {i}; SHA256 {sha(raw)}')
    for i, digest in TARGETS.items():
        print('target', i, target(outer(root / f'qa-{i}.zip', digest), i == 15275992757))
    payload['START-HERE.txt'] = ('\n'.join(guide)+'\n').encode()
    payload['SHA256SUMS.txt'] = ''.join(f'{sha(b)}  {n}\n' for n,b in sorted(payload.items())).encode()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_STORED) as z:
        for n,b in sorted(payload.items()):
            z.writestr(zipfile.ZipInfo(n, (2026,11,28,0,0,0)), b)
    print('collection', sha(output.read_bytes()))

if __name__ == '__main__':
    run(Path(sys.argv[1]), Path(sys.argv[2]))
