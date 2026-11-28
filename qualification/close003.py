"""Finalize exact Collection 003 desktop replay status without replacing executables."""
import hashlib, json, sys, zipfile
from pathlib import Path

OLD = 'ef7e0c34365bf2f808e369edb87c3d6404a91eae48a7a44fc8e21c6de1e26bb4'
PINS = {
    'phasespace-windows.zip': (15248387458, 'd1049fa7fa6266cb9a76419eafb32ac6bf9124c491dd91b1879cc68da611127d', 46109559756, 23),
    'Xenoresonator.zip': (15248417526, 'b0cdbd473c36e60c06d3ad9c7965b9f5b622b44504ce5d093ca3a155dc5cc634', 46109559756, 16),
    'chronogrid-windows.zip': (15248384121, 'faee421ca6afb6c30bae9f12aafea00e1e50d6daaa66a78ce0fb98f7848e4acf', 46109559756, 12),
    'cluster-native-windows.zip': (15245944682, '9cf94d9e342db625cccf1506c59ff1754da16921dfbc74f8ed5a66e8ea6d2a98', 46104942985, None),
}

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def need(ok, message):
    if not ok:
        raise ValueError(message)

def finalize(source, output):
    raw = source.read_bytes()
    need(sha(raw) == OLD, 'wrong predecessor digest')
    from io import BytesIO
    with zipfile.ZipFile(BytesIO(raw)) as z:
        need(z.testzip() is None, 'bad predecessor CRC')
        need(set(z.namelist()) == set(PINS) | {'manifest.json', 'README.txt', 'optional-platform-audit.json'}, 'unexpected predecessor members')
        entries = json.loads(z.read('manifest.json'))
        by_artifact = {e['artifact']: e for e in entries}
        need(len(entries) == len(by_artifact) == len(PINS), 'invalid manifest cardinality')
        packages = {}
        for filename, (aid, digest, run, media) in PINS.items():
            package = z.read(filename)
            row = by_artifact[aid]
            need(sha(package) == digest == row['inner_sha256'], 'package digest mismatch: '+filename)
            packages[filename] = package
            row['independent_windows'] = f'PASS: {run} exact extracted replay and graphical smoke' + (f'; {media} regenerated media files byte-identical' if media is not None else '; clean M10 source CTest and UDP fault replay')
            row['independent_windows_run'] = run
        optional = json.loads(z.read('optional-platform-audit.json'))
        need(optional['artifact'] == 15247846558 and optional['inner_sha256'] == '90944795c30721e3b80b6da67b141316bbe932cc8e48e45c4edac27b9ab84b20', 'wrong P2 audit pin')
        optional['independent_windows'] = 'PASS: 46109559756 exact extracted replay, graphical smoke, 23 media files and clean source CTest 3/3'
        optional['independent_windows_run'] = 46109559756
        readme = z.read('README.txt').decode('utf-8')
        old = 'New M4/M1 independent Windows replay pending; retained M10 replay PASS.'
        need(old in readme, 'unexpected README')
        readme = readme.replace(old, 'All four exact packaged instruments passed independent Windows extracted replay and graphical smoke. See manifest for runs. Independent target rebuild, physical and rights gates remain open.')
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_STORED) as z:
        def put(name, data):
            z.writestr(zipfile.ZipInfo(name, (2026,11,28,0,0,0)), data)
        for name, package in packages.items():
            put(name, package)
        put('manifest.json', json.dumps(entries, indent=2)+'\n')
        put('optional-platform-audit.json', json.dumps(optional, indent=2)+'\n')
        put('README.txt', readme)
    with zipfile.ZipFile(output) as z:
        need(z.testzip() is None, 'bad output CRC')
        for name, package in packages.items():
            need(z.read(name) == package, 'changed executable archive: '+name)
    print('PASS four exact inner archives retained; SHA256', sha(output.read_bytes()))

if __name__ == '__main__':
    finalize(Path(sys.argv[1]), Path(sys.argv[2]))
