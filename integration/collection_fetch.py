"""Retrieve pinned GitHub Actions ZIPs and assemble the author preview (read-only)."""
import argparse
import json
import re
import subprocess
from pathlib import Path

from collection import assemble, require, sha

REPO = 'techrote/cyd-SYNTH-VIBEWARE'


def api(path):
    return subprocess.run(['gh', 'api', '--hostname', 'github.com', '--method', 'GET',
                           f'repos/{REPO}/actions/{path}'],
                          check=True, stdout=subprocess.PIPE, timeout=180).stdout


def acquire(pins, directory, request=api):
    """Validate live metadata before accepting cached or newly fetched ZIPs."""
    head = pins['head']
    require(re.fullmatch(r'[0-9a-f]{40}', head), 'invalid pinned head')
    require(re.fullmatch(r'[0-9a-f]{40}', pins['tree']), 'invalid pinned tree')
    entries = [*pins['apps'], pins['board']]
    require(len(pins['apps']) == 5 and len({p['artifact'] for p in entries}) == 6,
            'expected five applications and one unique board archive')
    for p in entries:
        require(all(type(p[k]) is int and p[k] > 0 for k in ('run', 'artifact')),
                'invalid run/artifact id')
        require(re.fullmatch(r'[0-9a-f]{64}', p['sha256']), 'invalid digest')
    # Complete metadata checks before writing any new cache entries.
    for p in entries:
        run = json.loads(request(f'runs/{p["run"]}'))
        require(run['id'] == p['run'] and run['head_sha'] == head and
                run['head_repository']['full_name'] == REPO and
                run['status'] == 'completed' and run['conclusion'] == 'success',
                f'run not successful at pinned source: {p["run"]}')
        artifact = json.loads(request(f'artifacts/{p["artifact"]}'))
        require(artifact['id'] == p['artifact'] and artifact['expired'] is False and
                artifact['digest'] == 'sha256:' + p['sha256'] and
                artifact['workflow_run']['id'] == p['run'] and
                artifact['workflow_run']['head_sha'] == head,
                f'artifact metadata mismatch: {p["artifact"]}')
    directory.mkdir(parents=True, exist_ok=True)
    for p in entries:
        path = directory / f'github-actions-artifact-{p["artifact"]}.zip'
        if path.exists():
            require(sha(path.read_bytes()) == p['sha256'], f'cached digest mismatch: {path.name}')
        else:
            data = request(f'artifacts/{p["artifact"]}/zip')
            require(sha(data) == p['sha256'], f'downloaded digest mismatch: {path.name}')
            # Exclusive create: never overwrite a concurrent or earlier download.
            with path.open('xb') as stream:
                stream.write(data)
        print(f'PASS pinned run {p["run"]}, artifact {p["artifact"]}, outer SHA256 {p["sha256"]}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pins', type=Path)
    parser.add_argument('downloads', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'output already exists')
    pins = json.loads(args.pins.read_text())
    acquire(pins, args.downloads)
    assemble(pins, args.downloads, args.output)


if __name__ == '__main__':
    main()
