"""Assemble pinned QA batch 011 owner-only Windows audition archives.
No execution, installation, firmware redistribution, or qualification inference.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import stat
import zipfile

BASE = 'https://github.com/techrote/cyd-SYNTH-VIBEWARE'
HEAD = '319664f449f2c4e77ced3ff80f5a743e4ad36b59'
TREE = '5a4b413929afe0f420e80f7c2e71c42f1136ebce'
# name, local outer archive, author run, artifact, outer digest, executable
PINS = (
    ('PHASESPACE', 'integrated-phasespace.zip', 46145279990, 15263486904, 'b197891f9c866d50302445be7f31cd72fd0c831fe95cbf2416c761966852110c', 'phasespace.exe'),
    ('CHRONOGRID', 'integrated-chronogrid.zip', 46145279922, 15263715490, 'c6fb112dd52ef845dfab5a4b1ae8664dbfe66af294c090dab4415625da9dd313', 'chronogrid.exe'),
    ('Xenoresonator', 'integrated-xenoresonator.zip', 46145279932, 15263133705, '3580249f2de06be03ea137504e7d89ec2dc74e8737aa1f4201de17a27f48bb21', 'Xenoresonator.exe'),
    ('Chronomorph', 'integrated-chronomorph.zip', 46145279872, 15263302248, 'e3cb193a9379999bc850f18108795de3ccb5842bb487a5adf189a704e9e8a1d2', 'Chronomorph.exe'),
    ('XENOBIO', 'integrated-xenobio-319664f4.zip', 46145279876, 15262659578, 'bb6b3c6349f11ad89806dbfce3e3c40722e0aae40a66e77cb5decc2c904cbb8f', 'Xenobio.exe'),
    ('Ecophon-preview', 'ecophon-m3-author-outer.zip', 46143835690, 15263235062, '8404b52d27aef95e0fce06e2276de8a7ee9ed4b7ab5c2ad400bfa265f9ba5b27', 'Ecophon.exe'),
)
PREVIEW_HEAD = 'ba10122adbe2e431087d219f439815254f16eb0c'
PREVIEW_TREE = '21f8ad084c85da4976bf7eb6f2c79d1b80b53371'
MAX_FILE = 32 * 1024 * 1024


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def members(z):
    files, seen = {}, set()
    for info in z.infolist():
        n = info.filename.rstrip('/')
        parts = n.split('/')
        require(n and not any(p in ('', '.', '..') or p.endswith((' ', '.')) for p in parts)
                and '\\' not in n and ':' not in n and not n.startswith('/'), f'unsafe path: {n}')
        require(n.casefold() not in seen, f'duplicate path: {n}')
        seen.add(n.casefold())
        require(not stat.S_ISLNK(info.external_attr >> 16), f'symlink: {n}')
        require(info.file_size <= MAX_FILE, f'oversize: {n}')
        if not info.is_dir():
            files[n] = z.read(info)
    for n in files:
        require(not any(str(parent) in files for parent in PurePosixPath(n).parents if str(parent) != '.'), f'file parent: {n}')
    return files


def verify(outer, pin):
    name, _, run, artifact, digest, exe = pin
    require(sha(outer) == digest, f'{name}: outer digest')
    with zipfile.ZipFile(io.BytesIO(outer)) as z:
        a = members(z)
    archives = [n for n in a if n.endswith('.zip')]
    require(len(archives) == 1, f'{name}: inner archive count')
    inner = a.pop(archives[0])
    require(not a or (len(a) == 1 and next(iter(a)).endswith('.sha256') and next(iter(a.values())).decode().strip().lower() == sha(inner)), f'{name}: outer receipt')
    with zipfile.ZipFile(io.BytesIO(inner)) as z:
        f = members(z)
    manifests = [n for n in ('manifest.sha256', 'SHA256SUMS.txt') if n in f]
    require(len(manifests) == 1, f'{name}: manifest count')
    manifest = manifests[0]
    expected = set()
    for line in f[manifest].decode('utf-8-sig').splitlines():
        match = re.fullmatch(r'([0-9a-fA-F]{64})[ \t]+\*?(.+)', line)
        require(match is not None, f'{name}: malformed manifest')
        h, path = match.groups()
        require(path not in expected and path in f and sha(f[path]) == h.lower(), f'{name}: manifest entry {path}')
        expected.add(path)
    require(expected == set(f) - {manifest}, f'{name}: incomplete manifest')
    if 'SOURCE.txt' in f:
        source = f['SOURCE.txt'].decode().splitlines()
    else:
        source = [f[n].decode().strip() for n in ('source-commit.txt', 'source-tree.txt')]
    preview = name.endswith('-preview')
    require(source == ([PREVIEW_HEAD, PREVIEW_TREE] if preview else [HEAD, TREE]), f'{name}: source receipt')
    require(exe in f and f[exe][:2] == b'MZ', f'{name}: executable')
    require(not any(n.lower().endswith(('.bin', '.elf', '.hex', '.uf2')) for n in f), f'{name}: firmware payload')
    media = sum(n.lower().endswith(('.wav', '.ppm', '.png')) for n in f)
    return inner, {'name': name, 'executable': exe, 'commit': source[0], 'tree': source[1],
                   'run': run, 'artifact': artifact, 'url': f'{BASE}/actions/runs/{run}/artifacts/{artifact}',
                   'outer_sha256': digest, 'inner_sha256': sha(inner), 'manifest_files': len(expected),
                   'media_files': media, 'status': 'preview; independent native PASS, Windows/target independent NOT RUN' if preview else 'integrated author CI PASS; independent archive custody PASS; new cumulative Windows replay NOT RUN'}


def build(source, output):
    require(not output.exists(), 'refuse overwrite')
    payload, ledger = {}, []
    for pin in PINS:
        inner, row = verify((source / pin[1]).read_bytes(), pin)
        row['archive'] = ('Preview/' if pin[0].endswith('-preview') else 'Standalone/') + pin[0] + '.zip'
        payload[row['archive']] = inner
        ledger.append(row)
    guide = ['# CYD VIBEWARE — owner audition batch 011', '',
             'Draft/HOLD. Extract the collection, then each instrument ZIP into its own directory. Run the listed EXE. No installer, no device flashing, no firmware. Start with low playback volume; use Panic/Escape and close the application if necessary.', '',
             'Five standalone packages are from integrated PR #17 at exact 319664f4 (17/17 author jobs PASS). Their constituent source pins have prior independent evidence, but this collection only verifies archive custody; it does not independently execute these new binaries. Ecophon is a separate author-qualified preview with independent portable native execution, not independent Windows or ESP32 rebuild. No public redistribution rights asserted. Physical audio, touch, device timing and owner approval remain HOLD.', '',
             '| Instrument | Run | Controls |', '|---|---|---|',
             '| PHASESPACE | phasespace.exe | Touch XY, oscillator coupling and morph; release to stop. |',
             '| CHRONOGRID | chronogrid.exe | Paint spatial percussion steps, independent track ratios, transport and pattern controls. |',
             '| Xenoresonator | Xenoresonator.exe | Select resonant material and excite by touch. |',
             '| Chronomorph | Chronomorph.exe | Elastic clocks, scenes, rhythm and live tempo controls. |',
             '| XENOBIO | Xenobio.exe | Select a scene, then Feed, Attract or Disturb the ecology. |',
             '| Ecophon (preview) | Ecophon.exe | Select a scene, adjust fertility/pressure and sharing; release sustains drones, Panic stops. |', '',
             'Each inner archive retains its original media, controls/readme and source receipt. See PINS.json for exact provenance and SHA-256. Predecessor author archives remain frozen; no promotion or main merge.']
    payload['README.md'] = ('\n'.join(guide) + '\n').encode()
    payload['PINS.json'] = (json.dumps(ledger, indent=2, sort_keys=True) + '\n').encode()
    payload['SHA256SUMS.txt'] = ''.join(f'{sha(data)}  {n}\n' for n, data in sorted(payload.items())).encode()
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_STORED) as z:
        for n, data in sorted(payload.items()):
            info = zipfile.ZipInfo(n, (2026, 11, 28, 0, 0, 0))
            info.external_attr = 0o100644 << 16
            z.writestr(info, data)
    with zipfile.ZipFile(output) as z:
        require(members(z) == payload, 'collection readback mismatch')
    for row in ledger:
        print(row['name'], row['manifest_files'], row['media_files'], row['inner_sha256'])
    print('collection', sha(output.read_bytes()))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build(args.source, args.output)
