"""Offline analysis of M18 CHECKPOINT_*_V1 serial meter pairs; no device I/O."""
import argparse
import hashlib
import json
import re
from pathlib import Path

METER = ('uptime_us work_us work_max_us net_us net_max_us export_us export_max_us '
         'render_us render_max_us tx rx rejected tx_fail heap min_heap largest online enabled').split()
PANEL = 'bytes local_seq remote_seq saved target admitted refused caught_up cold slices frames contact'.split()
WORKER = 'bytes seq starts complete aborted pending'.split()
ARMS = ('radio-off', 'radio-local', 's3', 'c3', 'outage')


def row(line, tag, keys):
    parts = line.split()
    if len(parts) != len(keys) + 1 or parts[0] != tag:
        raise ValueError('invalid ' + tag)
    result = {}
    for key, token in zip(keys, parts[1:]):
        if not re.fullmatch(key + r'=[0-9]{1,20}', token):
            raise ValueError('invalid ' + key)
        value = int(token.split('=')[1])
        wide = key == 'uptime_us' or key in ('work_us', 'net_us', 'export_us', 'render_us')
        if value >= 2**(64 if wide else 32):
            raise ValueError('overflow ' + key)
        result[key] = value
    return result


def parse(log, role):
    if role not in ('panel', 'worker'):
        raise ValueError('unknown role')
    tag, keys = ('CHECKPOINT_PANEL_V1', PANEL) if role == 'panel' else ('CHECKPOINT_WORKER_V1', WORKER)
    samples, pending = [], None
    for raw in log.splitlines():
        line = raw.strip()
        if line.startswith('M18 checkpoint visual-only') and (samples or pending is not None):
            raise ValueError('boot inside capture')
        if not line.startswith('CHECKPOINT_'):
            continue
        if pending is None:
            pending = row(line, 'CHECKPOINT_METER_V1', METER)
        else:
            samples.append((pending, row(line, tag, keys)))
            pending = None
    if pending is not None or len(samples) != 2:
        raise ValueError('exactly two complete meter groups required')
    return samples


def window(log, role, arm):
    if arm not in ARMS or (role == 'worker' and arm in ('radio-off', 'radio-local')):
        raise ValueError('invalid role/arm')
    (a, p), (b, q) = parse(log, role)

    def delta(x, y, k):
        if y[k] < x[k]:
            raise ValueError('counter reset or wrap: ' + k)
        return y[k] - x[k]

    elapsed = delta(a, b, 'uptime_us')
    if not elapsed or not p['bytes'] or p['bytes'] != q['bytes']:
        raise ValueError('invalid duration or object footprint')
    for m, s in ((a, p), (b, q)):
        flag = 'contact' if role == 'panel' else 'pending'
        if any(v not in (0, 1) for v in (m['online'], m['enabled'], s[flag])):
            raise ValueError('invalid flag')
        if m['min_heap'] > m['heap'] or m['largest'] > m['heap']:
            raise ValueError('invalid heap snapshot')
        if role == 'worker' and s['starts'] != s['complete'] + s['aborted'] + s['pending']:
            raise ValueError('worker transfer accounting mismatch')
        if role == 'panel' and arm != 'outage' and s['contact']:
            raise ValueError('touch active at steady boundary')
    if b['min_heap'] > a['min_heap']:
        raise ValueError('lifetime heap minimum increased')
    work = ('work_us', 'net_us', 'export_us', 'render_us')
    maxima = {k.replace('_us', '_max_us'): b[k.replace('_us', '_max_us')] for k in work}
    for k in maxima:
        delta(a, b, k)
    d = {k: delta(a, b, k) for k in (*work, 'tx', 'rx', 'rejected', 'tx_fail')}
    counters = ('admitted', 'refused', 'caught_up', 'cold', 'slices', 'frames') if role == 'panel' else ('seq', 'starts', 'complete', 'aborted')
    d.update({k: delta(p, q, k) for k in counters})
    if role == 'panel' and any(d[k] for k in ('tx', 'tx_fail', 'export_us')):
        raise ValueError('panel transmitted or exported')
    if role == 'worker' and any(d[k] for k in ('rx', 'rejected', 'render_us')):
        raise ValueError('worker received or rendered')
    if arm in ('radio-off', 'radio-local'):
        if any(m['online'] != int(arm == 'radio-local') or m['enabled'] for m in (a, b)) or d['rx'] or d['admitted']:
            raise ValueError('local arm contradicts counters/flags')
        if arm == 'radio-off' and (d['net_us'] or d['rejected']):
            raise ValueError('radio-off arm performed socket work')
    if arm in ('s3', 'c3'):
        if any(not m['online'] or not m['enabled'] for m in (a, b)) or not d['rx' if role == 'panel' else 'tx']:
            raise ValueError('no active remote stream at boundaries')
    packets = d['rx' if role == 'panel' else 'tx']
    # Counters combine 788-byte projections and 1048-byte checkpoint fragments.
    return {'elapsed_us': elapsed, 'delta': d, 'loop_work_fraction': {k: d[k]/elapsed for k in work},
            'lifetime_max_us': maxima, 'heap_end': {k: b[k] for k in ('heap', 'min_heap', 'largest')},
            'object_bytes': q['bytes'], 'start_state': p, 'end_state': q,
            'start_flags': {k: a[k] for k in ('online', 'enabled')},
            'end_flags': {k: b[k] for k in ('online', 'enabled')},
            'application_bytes_per_s_bounds': [packets*n*1e6/elapsed for n in (788, 1048)],
            'completed_frames_or_ticks_per_s': d['frames' if role == 'panel' else 'seq']*1e6/elapsed}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--arm', choices=ARMS, required=True)
    ap.add_argument('--panel', type=Path, required=True)
    ap.add_argument('--panel-board', required=True)
    ap.add_argument('--panel-sha256', required=True)
    ap.add_argument('--worker', type=Path)
    ap.add_argument('--worker-board')
    ap.add_argument('--worker-sha256')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    try:
        remote = args.arm in ('s3', 'c3', 'outage')
        if remote != bool(args.worker) or remote != bool(args.worker_board) or remote != bool(args.worker_sha256):
            raise ValueError('remote/outage arms require worker log, board and digest; local arms omit them')
        inputs = [('panel', args.panel, args.panel_board, args.panel_sha256)]
        if remote:
            inputs.append(('worker', args.worker, args.worker_board, args.worker_sha256))
        paths = [p.resolve() for _, p, _, _ in inputs] + [args.out.resolve()]
        if len(set(paths)) != len(paths):
            raise ValueError('input/output paths must be distinct')
        report = {'schema': 1, 'arm_operator_reported': args.arm, 'endpoints': {},
                  'limits': 'Operator-reported identities and arm; independent unsynchronized windows. No paired loss, RTT, exact byte rate, RF goodput, interval maxima, exact state equivalence, audio or speedup inferred. Work includes control overhead; outage label does not prove failure/rejoin.'}
        for role, path, board, digest in inputs:
            if not board.strip() or not re.fullmatch(r'[0-9a-fA-F]{64}', digest):
                raise ValueError('invalid board or firmware digest')
            data = path.read_bytes()
            result = window(data.decode('utf-8'), role, args.arm)
            result.update(board_operator_reported=board, firmware_sha256_operator_reported=digest.lower(), log_sha256=hashlib.sha256(data).hexdigest())
            report['endpoints'][role] = result
        with args.out.open('x', encoding='utf-8') as f:
            json.dump(report, f, indent=2, allow_nan=False)
            f.write('\n')
    except (OSError, ValueError, UnicodeError) as exc:
        ap.error(str(exc))


if __name__ == '__main__':
    main()
