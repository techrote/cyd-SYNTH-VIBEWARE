"""Compile-only standalone CYD batch. Python 3.12; no device or upload actions."""
import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

from board_profiles import PROFILES, plan


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()


def sizes(log):
    result = {}
    for kind, capacity in [('RAM', 327680), ('Flash', 3145728)]:
        matches = re.findall(r'^' + kind + r':[^\n]*\(used (\d+) bytes from (\d+) bytes\)', log, re.M)
        if len(matches) != 1:
            raise ValueError('missing or ambiguous ' + kind + ' usage')
        used, total = map(int, matches[0])
        if not 0 < used <= total or total != capacity:
            raise ValueError('unexpected ' + kind + ' budget')
        result[kind.lower()] = {'used': used, 'capacity': total}
    return result


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def build(source, scratch, out, profiles, head, tree):
    for package, version in [('platformio', '6.1.18'), ('intelhex', '2.3.0')]:
        if importlib.metadata.version(package) != version:
            raise RuntimeError('wrong version of ' + package)
    import intelhex  # noqa: F401 -- fail before spending runner time
    for entry in profiles:
        profile = entry['profile']
        project = source / entry['directory']
        receipt = out / profile
        receipt.mkdir()
        env = os.environ.copy()
        env = {k: v for k, v in env.items() if not k.upper().startswith('PLATFORMIO_')}
        env['PLATFORMIO_CORE_DIR'] = str(scratch / ('pio-' + profile))
        env['NO_COLOR'] = '1'
        command = [sys.executable, '-m', 'platformio']
        with (receipt / 'build.txt').open('wb') as log:
            subprocess.run(command + ['run', '-d', str(project), '-e', profile], env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=1200)
        log = (receipt / 'build.txt').read_text(encoding='utf-8', errors='replace')
        usage = sizes(re.sub(r'\x1b\[[0-9;]*m', '', log))
        with (receipt / 'packages.txt').open('wb') as log_file:
            subprocess.run(command + ['pkg', 'list', '-d', str(project), '-e', profile], env=env, stdout=log_file, stderr=subprocess.STDOUT, check=True, timeout=120)
        built = project / '.pio' / 'build' / profile
        for filename in ('firmware.map', 'firmware.elf'):
            if not (built / filename).is_file() or not (built / filename).stat().st_size:
                raise RuntimeError('missing build output: ' + filename)
        shutil.copyfile(built / 'firmware.map', receipt / 'firmware.map')
        record = dict(entry, head=head, tree=tree, sizes=usage, elf_sha256=digest(built / 'firmware.elf'), physical='HOLD', qualification='author compile/link only')
        (receipt / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
        manifest = ''.join(digest(p) + '  ' + p.name + '\n' for p in sorted(receipt.iterdir()))
        (receipt / 'SHA256SUMS').write_text(manifest, encoding='ascii')
        print(profile + ': compile/link PASS (not physical qualification)', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    parser.add_argument('--head', required=True)
    parser.add_argument('--tree', required=True)
    parser.add_argument('--build', type=Path, metavar='NEW_EVIDENCE_DIRECTORY', help='explicitly compile all seven; omit for source/profile preflight only')
    args = parser.parse_args()
    if any(not re.fullmatch('[0-9a-f]{40}', s) for s in (args.head, args.tree)):
        parser.error('exact full lowercase commit and tree required')
    if git(args.repo, 'rev-parse', args.head + '^{commit}') != args.head or git(args.repo, 'rev-parse', args.head + '^{tree}') != args.tree:
        raise ValueError('source identity mismatch')
    with tempfile.TemporaryDirectory(prefix='cyd board batch ') as temp:
        scratch = Path(temp)
        source = scratch / 'source'
        source.mkdir()
        archive = scratch / 'source.tar'
        subprocess.run(['git', '-C', str(args.repo), 'archive', '--format=tar', '-o', str(archive), args.head], check=True)
        with tarfile.open(archive) as tar:
            tar.extractall(source, filter='data')
        archive.unlink()
        profiles = plan(source)
        print(json.dumps(dict(head=args.head, tree=args.tree, profiles=profiles, physical='HOLD'), indent=2), flush=True)
        if args.build is not None:
            out = args.build.resolve()
            out.mkdir(parents=True, exist_ok=False)
            build(source, scratch, out, profiles, args.head, args.tree)
            (out / 'COMPLETE.txt').write_text('Seven compile/link profiles completed; physical HOLD. No installable images included.\n', encoding='ascii')


if __name__ == '__main__':
    main()
