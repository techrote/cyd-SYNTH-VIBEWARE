"""Integration-owned extracted-package gate; never executes archive payloads."""
import argparse
import hashlib
import re
import tempfile
from pathlib import Path

MANIFEST = 'SHA256SUMS.txt'


def safe_name(name):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*', name):
        raise ValueError(f'Unsafe path: {name!r}')
    for part in name.split('/'):
        if part in ('.', '..') or part.endswith('.') or re.fullmatch(
            r'(?i)(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\..*)?', part
        ):
            raise ValueError(f'Unsafe component: {part!r}')
    return name


def register_path(seen, name):
    key = safe_name(name).casefold()
    if key in seen:
        raise ValueError(f'Case-colliding path: {name} and {seen[key]}')
    seen[key] = name
    return key


def inventory(root, manifest):
    if '/' in safe_name(manifest):
        raise ValueError('Manifest must be a root filename')
    if not root.is_dir() or root.is_symlink() or getattr(root, 'is_junction', lambda: False)():
        raise ValueError('A real package directory is required')
    found = {}
    seen = {}
    for path in root.rglob('*'):
        if path.is_symlink() or getattr(path, 'is_junction', lambda: False)():
            raise ValueError(f'Unexpected link: {path}')
        name = path.relative_to(root).as_posix()
        key = register_path(seen, name)
        if path.is_dir():
            if key == manifest.casefold():
                raise ValueError('Manifest path is a directory')
            continue
        if not path.is_file():
            raise ValueError(f'Not a regular file: {path}')
        found[key] = name
    if manifest.casefold() in found and found.pop(manifest.casefold()) != manifest:
        raise ValueError('Manifest filename case mismatch')
    if not found:
        raise ValueError('Empty payload')
    return found


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def create(root, manifest):
    files = inventory(root, manifest)
    with (root / manifest).open('x', encoding='ascii', newline='\n') as out:
        for name in sorted(files.values()):
            out.write(f'{digest(root / name)}  {name}\n')


def verify(root, manifest, style):
    files = inventory(root, manifest)
    pattern = r'([0-9a-fA-F]{64}) (.+)' if style == 'single' else r'([0-9a-fA-F]{64}) [ *](.+)'
    seen = set()
    for line in (root / manifest).read_text(encoding='ascii').splitlines():
        match = re.fullmatch(pattern, line)
        if not match:
            raise ValueError('Malformed manifest entry')
        expected, name = match.groups()
        key = safe_name(name).casefold()
        if key in seen or files.get(key) != name:
            raise ValueError(f'Duplicate, missing or mismatched path: {name}')
        if digest(root / name) != expected.lower():
            raise ValueError(f'Payload digest mismatch: {name}')
        seen.add(key)
    if seen != set(files):
        raise ValueError('Incomplete payload coverage')
    print(f'PASS {len(seen)} extracted payload hashes and complete coverage')


def selftest():
    with tempfile.TemporaryDirectory(prefix='package gate ') as tmp:
        root = Path(tmp)
        (root / 'media').mkdir()
        (root / 'media' / 'a.wav').write_bytes(b'a')
        (root / 'test.exe').write_bytes(b'b')
        create(root, MANIFEST)
        manifest = root / MANIFEST
        valid = manifest.read_text()
        verify(root, MANIFEST, 'gnu')
        manifest.write_text(valid.replace('  ', ' *'))
        verify(root, MANIFEST, 'gnu')
        manifest.write_text(valid.replace('  ', ' '))
        verify(root, MANIFEST, 'single')
        def refuse(label, action):
            try:
                action()
            except (ValueError, OSError):
                print(f'PASS refusal: {label}')
            else:
                raise AssertionError(f'Accepted {label}')
        for label, bad in [
            ('duplicate', valid + valid.splitlines()[0] + '\n'),
            ('omission', valid.splitlines()[0] + '\n'),
            ('traversal', valid.replace('media/a.wav', '../a.wav')),
            ('case alias', valid.replace('test.exe', 'TEST.exe')),
            ('malformed marker', valid.replace('  ', ' ?')),
            ('wrong hash', '0' * 64 + valid[64:]),
            ('reserved name', valid.replace('test.exe', 'NUL.exe')),
        ]:
            manifest.write_text(bad)
            refuse(label, lambda: verify(root, MANIFEST, 'gnu'))
        manifest.write_text(valid)
        (root / 'extra.txt').write_bytes(b'extra')
        refuse('unlisted payload', lambda: verify(root, MANIFEST, 'gnu'))
        (root / 'extra.txt').unlink()
        (root / 'test.exe').write_bytes(b'changed')
        refuse('corrupted payload', lambda: verify(root, MANIFEST, 'gnu'))
        (root / 'test.exe').write_bytes(b'b')
        # Synthetic namespace checks run on Windows as well as case-sensitive hosts.
        seen = {}
        register_path(seen, 'media')
        refuse('directory case alias', lambda: register_path(seen, 'MEDIA'))
        refuse('nested manifest', lambda: verify(root, 'media/a.wav', 'gnu'))
        manifest.unlink()
        manifest.mkdir()
        refuse('manifest directory', lambda: create(root, MANIFEST))
        manifest.rmdir()
        create(root, MANIFEST)
        verify(root, MANIFEST, 'gnu')
    print('PASS 4 positive and 12 negative package-gate fixtures')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['create', 'verify', 'selftest'])
    parser.add_argument('root', nargs='?', type=Path)
    parser.add_argument('--manifest', default=MANIFEST)
    parser.add_argument('--style', choices=['gnu', 'single'], default='gnu')
    args = parser.parse_args()
    if args.mode == 'selftest':
        selftest()
    else:
        if args.root is None or not args.root.is_dir() or args.root.is_symlink():
            parser.error('A real package directory is required')
        safe_name(args.manifest)
        if args.mode == 'create':
            create(args.root, args.manifest)
        else:
            verify(args.root, args.manifest, args.style)
