"""Host-only tests: no actual target toolchain or device required."""
import configparser
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from board_batch import PROFILES, plan, sizes


class BoardBatch(unittest.TestCase):
    def test_sizes(self):
        log = 'RAM:   [=         ]   8.4% (used 27636 bytes from 327680 bytes)\nFlash: [=         ]  12.2% (used 382697 bytes from 3145728 bytes)\n'
        self.assertEqual(sizes(log)['ram']['used'], 27636)
        self.assertEqual(sizes(log)['flash']['used'], 382697)
        for bad in (log + log, log.splitlines()[0], log.replace('3145728', '1048576'), log.replace('27636', '400000'), log.replace('27636', '0')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                sizes(bad)

    def test_plan_and_exact_archive(self):
        script = Path(__file__).with_name('board_batch.py').resolve()
        with tempfile.TemporaryDirectory(prefix='profile test ') as temp:
            repo = Path(temp)
            def git(*args):
                return subprocess.check_output(['git', '-C', str(repo), *args], text=True, stderr=subprocess.STDOUT).strip()
            git('init', '-q')
            for directory, profile, platform in PROFILES:
                target = repo / directory / 'platformio.ini'
                target.parent.mkdir(parents=True)
                target.write_text('[platformio]\ndefault_envs = ' + profile + '\n[env:' + profile + ']\nplatform = espressif32 @ ' + platform + '\nboard = esp32dev\nframework = arduino\nboard_build.partitions = huge_app.csv\n', encoding='utf-8')
            self.assertEqual(len(plan(repo)), 5)
            git('add', '.')
            git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'fixture')
            head, tree = git('rev-parse', 'HEAD'), git('rev-parse', 'HEAD^{tree}')
            # Dirty working copy must not contaminate a pinned build.
            target.write_text(target.read_text().replace('esp32dev', 'wrong-board'), encoding='utf-8')
            with self.assertRaises(ValueError):
                plan(repo)
            command = [sys.executable, str(script), '--repo', str(repo), '--head', head, '--tree']
            valid = subprocess.run(command + [tree], capture_output=True, text=True, check=True)
            self.assertIn('cyd-port-chrono', valid.stdout)
            self.assertIn('HOLD', valid.stdout)
            invalid = subprocess.run(command + ['0' * 40], capture_output=True, text=True)
            self.assertNotEqual(invalid.returncode, 0)
            self.assertIn('source identity mismatch', invalid.stderr)


if __name__ == '__main__':
    unittest.main()
