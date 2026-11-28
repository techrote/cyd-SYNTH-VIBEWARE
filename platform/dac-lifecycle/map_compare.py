"""Verify Ecophon M4 target receipts without distributing the ELF or firmware.
Map placement is evidence, not proof of ISR call graph or physical timing.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
from zipfile import ZipFile

SOURCE = 'd353906d58b960fb987b18524d25dfe831078e60'
TREE = '7f104198c8e911cea0c0d77abbdf3adfb586b999'
DIGEST = 'a45928bfb9efc27b1ddc82048212704b376fd9fe22c003ccbb0ebe3ca3070ade'
SECTIONS = ('.iram0.text', '.dram0.data', '.dram0.bss', '.flash.text', '.flash.rodata')


def one(matches, label):
    if len(matches) != 1:
        raise ValueError(f'{label}: expected one match, found {len(matches)}')
    return matches[0]


def sections(mapping):
    result = {}
    for name in SECTIONS:
        addr, size = one(re.findall(r'^' + re.escape(name) + r'\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s*$', mapping, re.M), name)
        result[name] = (int(addr, 16), int(size, 16))
    return result


def placed(mapping, pattern, parent, ranges):
    start, size = ranges[parent]
    matches = [(int(a, 16), int(n, 16)) for a, n in re.findall(pattern, mapping, re.M)]
    addr, length = one([(a, n) for a, n in matches if a and n], pattern)
    if not (start <= addr and addr + length <= start + size):
        raise ValueError(f'{pattern}: outside {parent}')
    return {'address': hex(addr), 'bytes': length, 'section': parent}


def compare(archive):
    if hashlib.sha256(archive.read_bytes()).hexdigest() != DIGEST:
        raise ValueError('archive digest mismatch')
    with ZipFile(archive) as z:
        if len(z.namelist()) != len(set(z.namelist())) or z.testzip() is not None:
            raise ValueError('duplicate or corrupt ZIP member')
        if z.read('source.txt').decode().split() != [SOURCE, TREE]:
            raise ValueError('source/tree mismatch')
        maps = [z.read(n).decode() for n in ('firmware.map', 'timer-firmware.map')]
        logs = [z.read(n).decode() for n in ('build.log', 'timer-build.log')]
    ranges = list(map(sections, maps))
    result = {'source': SOURCE, 'tree': TREE, 'archive_sha256': DIGEST, 'profiles': []}
    for log, segs in zip(logs, ranges):
        ram = one(re.findall(r'^RAM:.*\(used (\d+) bytes from (\d+) bytes\)', log, re.M), 'RAM')
        flash = one(re.findall(r'^Flash:.*\(used (\d+) bytes from (\d+) bytes\)', log, re.M), 'flash')
        if '[SUCCESS]' not in log:
            raise ValueError('missing build success')
        result['profiles'].append({'ram': list(map(int, ram)), 'flash': list(map(int, flash)), 'sections_bytes': {k: v[1] for k, v in segs.items()}})
    t, s = maps[1], ranges[1]
    loc = r'\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+'
    obj = r'\.pio/build/cyd-port-ecophon-timer/src/main\.cpp\.o$'
    result['placement'] = {
        'application_iram_candidate': placed(t, r'^ \.iram1\.28' + loc + obj, '.iram0.text', s),
        'driver_iram_candidate': placed(t, r'^ \.iram1\.26' + loc + r'[^\n]*libdriver\.a\(timer\.c\.obj\)$', '.iram0.text', s),
        'sample_sink': placed(t, r'^ \.bss\._ZN12_GLOBAL__N_110sampleSinkE' + loc + obj, '.dram0.bss', s),
        'input': placed(t, r'^ \.bss\._ZN12_GLOBAL__N_15inputE' + loc + obj, '.dram0.bss', s),
    }
    return result


def self_test():
    fixture = '\n'.join(f'{n} 0x4000 0x100' for n in SECTIONS)
    assert sections(fixture)['.iram0.text'] == (0x4000, 0x100)
    for bad in (fixture + '\n.iram0.text 0x4000 0x100', fixture.replace('.iram0.text', '.other')):
        try:
            sections(bad)
        except ValueError:
            pass
        else:
            raise RuntimeError('invalid section accepted')
    for bad in ('x 0x40ff 0x2', 'x 0x0000 0x2', 'x 0x4000 0x2\nx 0x4001 0x2'):
        try:
            placed(bad, r'^x (0x[0-9a-f]+) (0x[0-9a-f]+)$', '.iram0.text', sections(fixture))
        except ValueError:
            pass
        else:
            raise RuntimeError('invalid placement accepted')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    self_test()
    print(json.dumps(compare(args.archive), indent=2))
