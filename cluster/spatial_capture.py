"""Offline M12 CYD panel meter comparison; never accesses serial or hardware."""
import argparse
import hashlib
import json
import re
from pathlib import Path

METER = 'uptime_us compute_us steps deferred io_us io_calls encode_us tx rx rejected heap min_heap largest online enabled'.split()
SLICE = 'calls max_us tick_compute_max_us pending pending_compute_us'.split()
PANEL = 'render_us render_calls local_frames remote_frames contact'.split()
ARMS = ('radio-off', 'radio-local', 's3', 'c3', 'outage')


def row(line, tag, keys):
    parts = line.split()
    if parts[0] != tag or len(parts) != len(keys) + 1:
        raise ValueError(f'invalid {tag} row')
    values = {}
    for token, key in zip(parts[1:], keys):
        prefix = key + '='
        if not token.startswith(prefix) or not re.fullmatch(r'[0-9]{1,20}', token[len(prefix):]):
            raise ValueError(f'invalid {tag}.{key}')
        value = int(token[len(prefix):])
        if value >= 2**64:
            raise ValueError(f'overflow {tag}.{key}')
        values[key] = value
    return values


def parse(log, reaction):
    schema = ([('SPATIAL_SLICE_V1', SLICE)] if reaction else []) + [('SPATIAL_METER_V1', METER), ('SPATIAL_PANEL_V1', PANEL)]
    samples, current = [], {}
    for raw in log.splitlines():
        line = raw.strip()
        if line.startswith('Spatial visual-only') and (samples or current):
            raise ValueError('boot banner inside capture')
        if not line.startswith('SPATIAL_'):
            continue
        tag, keys = schema[len(current)]
        current[tag] = row(line, tag, keys)
        if len(current) == len(schema):
            samples.append(current)
            current = {}
    if current or len(samples) != 2:
        raise ValueError('capture requires exactly two complete meter groups')
    return samples


def interval(log, reaction, arm):
    a, b = parse(log, reaction)
    m, n = a['SPATIAL_METER_V1'], b['SPATIAL_METER_V1']
    p, q = a['SPATIAL_PANEL_V1'], b['SPATIAL_PANEL_V1']

    def delta(x, y, key):
        if y[key] < x[key]:
            raise ValueError(f'counter reset or wrap: {key}')
        return y[key] - x[key]

    elapsed = delta(m, n, 'uptime_us')
    if not elapsed:
        raise ValueError('zero elapsed time')
    for mm, pp in ((m, p), (n, q)):
        if any(v not in (0, 1) for v in (mm['online'], mm['enabled'], pp['contact'])):
            raise ValueError('invalid status flag')
        if mm['min_heap'] > mm['heap'] or mm['largest'] > mm['heap']:
            raise ValueError('invalid heap snapshot')
        if pp['contact'] and arm != 'outage':
            raise ValueError('touch active at steady capture boundary')
    if n['min_heap'] > m['min_heap']:
        raise ValueError('lifetime minimum heap increased')
    d = {k: delta(m, n, k) for k in ('compute_us', 'steps', 'deferred', 'io_us', 'io_calls', 'encode_us', 'tx', 'rx', 'rejected')}
    d.update({k: delta(p, q, k) for k in ('render_us', 'render_calls', 'local_frames', 'remote_frames')})
    if d['tx'] or d['encode_us']:
        raise ValueError('panel unexpectedly transmitted worker snapshots')
    if arm in ('radio-off', 'radio-local'):
        expected = int(arm == 'radio-local')
        if any(mm['online'] != expected or mm['enabled'] for mm in (m, n)) or any(d[k] for k in ('rx', 'remote_frames', 'deferred')):
            raise ValueError('local arm contradicts boundary flags or counters')
        if arm == 'radio-off' and any(d[k] for k in ('io_us', 'io_calls', 'rejected')):
            raise ValueError('radio-off arm performed socket work')
    elif arm in ('s3', 'c3'):
        if any(not mm['online'] or not mm['enabled'] for mm in (m, n)) or not d['rx'] or not d['remote_frames']:
            raise ValueError('remote arm has no observed active remote stream')
    elif arm != 'outage':
        raise ValueError('unknown arm')
    complete_cost = d['compute_us']
    slice_report = None
    if reaction:
        s, t = a['SPATIAL_SLICE_V1'], b['SPATIAL_SLICE_V1']
        for ss, mm in ((s, m), (t, n)):
            phase = ss['calls'] - 25 * mm['steps']
            if not 0 <= phase < 25 or ss['pending'] != int(phase != 0) or (not phase and ss['pending_compute_us']) or ss['pending_compute_us'] > mm['compute_us']:
                raise ValueError('inconsistent reaction slice boundary')
        calls = delta(s, t, 'calls')
        for k in ('max_us', 'tick_compute_max_us'):
            delta(s, t, k)
        complete_cost += s['pending_compute_us'] - t['pending_compute_us']
        if complete_cost < 0 or (not d['steps'] and complete_cost):
            raise ValueError('inconsistent completed tick work')
        slice_report = {'calls': calls, 'start_pending_us': s['pending_compute_us'], 'end_pending_us': t['pending_compute_us'], 'lifetime_max_slice_us': t['max_us'], 'lifetime_max_tick_compute_us': t['tick_compute_max_us']}
    work = {k: d[k] / elapsed for k in ('compute_us', 'io_us', 'render_us')}
    return {'elapsed_us': elapsed, 'delta': d, 'measured_loop_duty': work,
            'completed_tick_compute_us': complete_cost,
            'mean_completed_tick_compute_us': complete_cost / d['steps'] if d['steps'] else None,
            'completed_frames_per_s': (d['local_frames'] + d['remote_frames']) * 1e6 / elapsed,
            'accepted_application_bytes_per_s': d['rx'] * 788 * 1e6 / elapsed,
            'heap_end': {k: n[k] for k in ('heap', 'min_heap', 'largest')}, 'slice': slice_report}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--model', choices=('light', 'reaction'), required=True)
    ap.add_argument('--firmware-sha256', required=True)
    ap.add_argument('--board', required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('captures', nargs='+', metavar='ARM=LOG')
    args = ap.parse_args()
    if not re.fullmatch(r'[0-9a-fA-F]{64}', args.firmware_sha256) or not args.board.strip():
        ap.error('provide firmware digest and board identity')
    try:
        results = {}
        for spec in args.captures:
            arm, sep, path = spec.partition('=')
            if not sep or arm not in ARMS or arm in results:
                raise ValueError('invalid or repeated arm')
            data = Path(path).read_bytes()
            result = interval(data.decode('utf-8'), args.model == 'reaction', arm)
            result['log_sha256'] = hashlib.sha256(data).hexdigest()
            results[arm] = result
        if 'radio-off' not in results:
            raise ValueError('radio-off reference required')
        baseline = results['radio-off']['measured_loop_duty']
        for result in results.values():
            result['duty_difference_vs_radio_off'] = {k: result['measured_loop_duty'][k] - baseline[k] for k in baseline}
        output = {'schema': 1, 'model': args.model, 'board_operator_reported': args.board, 'firmware_sha256_operator_reported': args.firmware_sha256.lower(), 'arms': results,
                  'limits': 'Operator-supplied labels and identity; boundary flags do not prove continuous configuration, node identity, RF state or outage. Cumulative work only; no interval maxima, latency percentiles, audio or physical provenance. Meter serial overhead and boundary skew included; no matched-state or speedup claim.'}
        if any(args.out.resolve() == Path(spec.partition('=')[2]).resolve() for spec in args.captures):
            raise ValueError('output must not overwrite input')
        with args.out.open('x', encoding='utf-8') as f:
            json.dump(output, f, indent=2, allow_nan=False)
            f.write('\n')
    except (OSError, UnicodeError, ValueError) as exc:
        ap.error(str(exc))


if __name__ == '__main__':
    main()
