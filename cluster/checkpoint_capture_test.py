"""Synthetic schema fixtures only, never hardware evidence."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from checkpoint_capture import METER, PANEL, WORKER, window


def fixture(role, arm):
    online = int(arm != 'radio-off')
    enabled = int(arm in ('s3', 'c3', 'outage'))
    rows = []
    for i in (0, 1):
        m = dict.fromkeys(METER, 0)
        m.update(uptime_us=1000000*(i+1), work_us=10000*i, work_max_us=100*i,
                 net_us=2000*i*online, net_max_us=50*i*online,
                 heap=100000, min_heap=90000, largest=80000, online=online, enabled=enabled)
        keys = PANEL if role == 'panel' else WORKER
        v = dict.fromkeys(keys, 0)
        v['bytes'] = 30000
        if role == 'panel':
            m.update(rx=20*i*enabled, render_us=30000*i, render_max_us=300*i)
            v.update(frames=8*i, admitted=i*enabled, slices=25*i)
        else:
            m.update(tx=22*i, export_us=500*i, export_max_us=500*i)
            v.update(seq=10*i, starts=i, complete=i)
        rows.append('CHECKPOINT_METER_V1 ' + ' '.join(f'{k}={m[k]}' for k in METER))
        rows.append('CHECKPOINT_' + role.upper() + '_V1 ' + ' '.join(f'{k}={v[k]}' for k in keys))
    return '\n'.join(rows) + '\n'


class Capture(unittest.TestCase):
    def test_local(self):
        for arm in ('radio-off', 'radio-local'):
            v = window(fixture('panel', arm), 'panel', arm)
            self.assertEqual(v['application_bytes_per_s_bounds'], [0, 0])
            self.assertEqual(v['completed_frames_or_ticks_per_s'], 8)
            self.assertEqual(v['loop_work_fraction']['work_us'], .01)

    def test_remote(self):
        for arm in ('s3', 'c3', 'outage'):
            p = window(fixture('panel', arm), 'panel', arm)
            w = window(fixture('worker', arm), 'worker', arm)
            self.assertEqual(p['application_bytes_per_s_bounds'], [15760, 20960])
            self.assertEqual(w['application_bytes_per_s_bounds'], [17336, 23056])
            self.assertEqual(w['delta']['complete'], 1)

    def test_invalid(self):
        p = fixture('panel', 's3')
        cases = [p+p, p.splitlines()[0], p.replace('rx=20', 'rx=0'),
                 p.replace('heap=100000', 'heap=80000'), p.replace('frames=8', 'frames=4294967296'),
                 p.replace('work_us=10000', 'work_us=-1'), p.replace('uptime_us=2000000', 'uptime_us=1000000'),
                 p.replace('contact=0', 'contact=1'), p.replace('CHECKPOINT_PANEL_V1', 'CHECKPOINT_WORKER_V1'),
                 p.replace('tx=0', 'tx=1', 1), p.replace('bytes=30000', 'bytes=0'),
                 p.replace('work_max_us=100', 'work_max_us=4294967296'),
                 p.replace('CHECKPOINT_METER_V1 uptime_us=2000000', 'M18 checkpoint visual-only\nCHECKPOINT_METER_V1 uptime_us=2000000')]
        for bad in cases:
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                window(bad, 'panel', 's3')
        with self.assertRaises(ValueError):
            window(fixture('worker', 's3').replace('complete=1', 'complete=0'), 'worker', 's3')
        with self.assertRaises(ValueError):
            window(p, 'panel', 'radio-off')

    def test_cli(self):
        with tempfile.TemporaryDirectory(prefix='checkpoint capture ') as tmp:
            root = Path(tmp)
            p, w, out = (root/n for n in ('panel log', 'worker log', 'report.json'))
            p.write_text(fixture('panel', 's3'), encoding='utf-8')
            w.write_text(fixture('worker', 's3'), encoding='utf-8')
            cmd = [sys.executable, str(Path(__file__).with_name('checkpoint_capture.py')), '--arm', 's3', '--panel', str(p), '--panel-board', 'CYD', '--panel-sha256', 'a'*64, '--worker', str(w), '--worker-board', 'S3', '--worker-sha256', 'b'*64, '--out', str(out)]
            subprocess.run(cmd, check=True, capture_output=True)
            report = json.loads(out.read_text(encoding='utf-8'))
            self.assertEqual(set(report['endpoints']), {'panel', 'worker'})
            self.assertEqual(report['endpoints']['worker']['delta']['complete'], 1)
            before = out.read_bytes()
            self.assertNotEqual(subprocess.run(cmd, capture_output=True).returncode, 0)
            self.assertEqual(out.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
