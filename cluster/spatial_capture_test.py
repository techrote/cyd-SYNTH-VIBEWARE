"""Synthetic serial fixtures, not physical measurement evidence."""
import unittest
from spatial_capture import interval, METER, PANEL, SLICE


def sample(uptime=1000, steps=4, phase=5, compute=450, pending=50,
           online=0, enabled=0, rx=0, remote=0, io=0, calls=0, deferred=0,
           reaction=True):
    def line(tag, keys, values):
        return tag + ' ' + ' '.join(f'{k}={v}' for k, v in zip(keys, values)) + '\n'
    s = line('SPATIAL_SLICE_V1', SLICE, (steps*25+phase, 10, 100, int(bool(phase)), pending)) if reaction else ''
    s += line('SPATIAL_METER_V1', METER, (uptime, compute, steps, deferred, io, calls, 0, 0, rx, 0, 10000, 9000, 8000, online, enabled))
    return s + line('SPATIAL_PANEL_V1', PANEL, (uptime//10, uptime//100, uptime//1000, remote, 0))


class CaptureTests(unittest.TestCase):
    def test_partial_boundaries(self):
        result = interval(sample() + sample(2000, 5, 3, 530, 30), True, 'radio-off')
        self.assertEqual(result['completed_tick_compute_us'], 100)
        self.assertEqual(result['slice']['calls'], 23)
        self.assertEqual(result['measured_loop_duty']['compute_us'], .08)
        self.assertEqual(result['completed_frames_per_s'], 1000)

    def test_pending_only(self):
        result = interval(sample() + sample(2000, 4, 7, 470, 70), True, 'radio-off')
        self.assertEqual(result['completed_tick_compute_us'], 0)
        self.assertIsNone(result['mean_completed_tick_compute_us'])

    def test_remote_pause(self):
        a = sample(online=1, enabled=1)
        b = sample(2000, online=1, enabled=1, rx=10, remote=1, io=100, calls=10, deferred=2)
        result = interval(a+b, True, 's3')
        self.assertEqual(result['delta']['steps'], 0)
        self.assertEqual(result['accepted_application_bytes_per_s'], 7880000)

    def test_light(self):
        result = interval(sample(reaction=False) + sample(2000, 5, compute=550, reaction=False), False, 'radio-off')
        self.assertEqual(result['completed_tick_compute_us'], 100)
        self.assertIsNone(result['slice'])

    def test_invalid(self):
        a = sample()
        b = sample(2000, 5, 3, 530, 30)
        cases = [a, a+b+a, a+b.replace('calls=128', 'calls=125', 1),
                 a+b.replace('pending_compute_us=30', 'pending_compute_us=999'),
                 a+b.replace('steps=5', 'steps=3'), a+b.replace('uptime_us=2000', 'uptime_us=1000'),
                 a+b.replace('rx=0', 'rx=1'), a+b.replace('online=0', 'online=2'),
                 a+'Spatial visual-only variant=1\n'+b, a+b.replace('heap=10000', 'heap=-1'),
                 a+b.replace('render_us=200', 'render_us=20'), a+b.replace('SPATIAL_PANEL_V1', 'SPATIAL_PANEL_V2')]
        for i, text in enumerate(cases):
            with self.subTest(i=i), self.assertRaises(ValueError):
                interval(text, True, 'radio-off')
        with self.assertRaises(ValueError):
            interval(a+b, True, 'c3')


if __name__ == '__main__':
    unittest.main()
