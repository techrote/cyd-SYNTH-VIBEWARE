"""Assemble a frozen author-qualified collection; never run or flash payloads."""
import argparse
import hashlib
import io
import json
import re
import stat
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def read_zip(data):
    files, seen = {}, {}
    with ZipFile(io.BytesIO(data)) as archive:
        for info in archive.infolist():
            name = info.filename.rstrip('/')
            parts = name.split('/')
            require(all(re.fullmatch(r'[A-Za-z0-9_.-]+', p) and p not in ('.', '..')
                        and not p.endswith('.') and not re.fullmatch(
                            r'(?i)(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\..*)?', p)
                        for p in parts), f'unsafe ZIP path: {name}')
            require(not stat.S_ISLNK(info.external_attr >> 16), f'link: {name}')
            for i in range(1, len(parts) + 1):
                prefix = '/'.join(parts[:i])
                kind = 'dir' if i < len(parts) or info.is_dir() else 'file'
                key = prefix.casefold()
                require(seen.get(key, (prefix, kind)) == (prefix, kind), f'collision: {name}')
                seen[key] = (prefix, kind)
            if not info.is_dir():
                require(name not in files, f'duplicate: {name}')
                files[name] = archive.read(info)
    return files


def verify(files, manifest, single=False, dot=False):
    checked = set()
    pattern = r'([0-9a-fA-F]{64}) (.+)' if single else r'([0-9a-fA-F]{64}) [ *](.+)'
    for line in files[manifest].decode('ascii').splitlines():
        match = re.fullmatch(pattern, line)
        require(match is not None, f'malformed {manifest}')
        expected, name = match.groups()
        if dot and name.startswith('./'):
            name = name[2:]
        require(name not in checked and name != manifest and name in files, f'unexpected {name}')
        require(sha(files[name]) == expected.lower(), f'hash mismatch: {name}')
        checked.add(name)
    require(checked == set(files) - {manifest} and checked, f'incomplete {manifest}')
    return len(checked)


def load_outer(directory, pin):
    data = (directory / f'github-actions-artifact-{pin["artifact"]}.zip').read_bytes()
    require(sha(data) == pin['sha256'], f'outer digest mismatch: {pin["artifact"]}')
    return read_zip(data)


def assemble(pins, directory, output):
    payload, receipts = {}, []
    head, tree = pins['head'], pins['tree']
    for pin in pins['apps']:
        outer = load_outer(directory, pin)
        inner = outer[pin['zip']]
        receipt = pin.get('zip_sha_file')
        require(set(outer) == ({pin['zip'], receipt} if receipt else {pin['zip']}), 'outer inventory')
        if receipt:
            require(outer[receipt].decode('ascii').strip().lower() == sha(inner), 'inner ZIP hash')
        files = read_zip(inner)
        count = verify(files, pin['manifest'], pin.get('single', False))
        if 'SOURCE.txt' in files:
            require(files['SOURCE.txt'].decode('ascii').splitlines() == [head, tree], 'source identity')
        else:
            for key, value in [('source-commit.txt', head), ('source-tree.txt', tree)]:
                require(files[key].decode('ascii').strip() == value, 'source identity')
        require(pin['exe'] in files, 'missing application executable')
        for name, data in files.items():
            payload[f'{pin["name"]}/{name}'] = data
        receipts.append(dict(pin, inner_sha256=sha(inner), payload_checks=count))
    board = load_outer(directory, pins['board'])
    aggregate = verify(board, 'SHA256SUMS', dot=True)
    require(board['batch-identity.txt'].decode().splitlines() == [f'commit={head}', f'tree={tree}', 'physical=HOLD'], 'board identity')
    nested = 0
    for profile in pins['board']['profiles']:
        files = {n.split('/', 1)[1]: b for n, b in board.items() if n.startswith(profile + '/')}
        nested += verify(files, 'SHA256SUMS')
        r = json.loads(files['receipt.json'])
        require((r['head'], r['tree'], r['profile'], r['physical']) == (head, tree, profile, 'HOLD'), 'profile identity')
        require(files['firmware.map'], 'empty map')
    require(not any(n.lower().endswith(('.bin', '.elf', '.hex', '.uf2')) for n in board), 'installable payload')
    payload.update({f'board-evidence/{n}': b for n, b in board.items()})
    payload['COLLECTION.json'] = (json.dumps(dict(head=head, tree=tree, physical='HOLD',
        qualification='author-qualified preview; independent exact-head Windows/target pending',
        apps=receipts, board=dict(pins['board'], aggregate_checks=aggregate, nested_checks=nested)),
        indent=2, sort_keys=True) + '\n').encode()
    payload['START.txt'] = ('AUTHOR-QUALIFIED PREVIEW - Draft/HOLD\nExtract once; launch the individual Windows applications below.\n'
        + '\n'.join(f'{p["name"]}/{p["exe"]}' for p in pins['apps'])
        + '\nRead each application README before use. Board evidence is compile-only; no firmware or flash authorization.\n').encode()
    payload['SHA256SUMS.txt'] = ''.join(f'{sha(b)}  {n}\n' for n, b in sorted(payload.items())).encode()
    with ZipFile(output, 'x', compression=ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in sorted(payload.items()):
            info = ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    count = verify(read_zip(output.read_bytes()), 'SHA256SUMS.txt')
    print(f'PASS {len(receipts)} apps, {sum(r["payload_checks"] for r in receipts)} app payload hashes, '
          f'{aggregate}+{nested} board hashes, {count} collection payload hashes; SHA256 {sha(output.read_bytes())}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pins', type=Path)
    parser.add_argument('downloads', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    assemble(json.loads(args.pins.read_text()), args.downloads, args.output)
