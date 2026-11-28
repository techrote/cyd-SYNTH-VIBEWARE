import copy
import json
import tempfile
import unittest
from pathlib import Path

from collection import sha
from collection_fetch import REPO, acquire


class FetchTests(unittest.TestCase):
    def setUp(self):
        self.head = 'a' * 40
        self.pins = dict(head=self.head, tree='b' * 40, apps=[], board=None)
        self.responses = {}
        self.requests = []
        for i in range(1, 7):
            data = f'archive {i}'.encode()
            pin = dict(run=100 + i, artifact=i, sha256=sha(data))
            if i == 6:
                self.pins['board'] = pin
            else:
                self.pins['apps'].append(pin)
            self.responses[f'runs/{100+i}'] = dict(id=100+i, head_sha=self.head,
                head_repository={'full_name': REPO}, status='completed', conclusion='success')
            self.responses[f'artifacts/{i}'] = dict(id=i, expired=False, digest='sha256:'+sha(data),
                workflow_run=dict(id=100+i, head_sha=self.head))
            self.responses[f'artifacts/{i}/zip'] = data

    def request(self, path):
        self.requests.append(path)
        value = self.responses[path]
        return value if isinstance(value, bytes) else json.dumps(value).encode()

    def test_download_and_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            acquire(self.pins, root, self.request)
            self.assertEqual(len(list(root.iterdir())), 6)
            self.assertEqual(sum(p.endswith('/zip') for p in self.requests), 6)
            self.requests.clear()
            acquire(self.pins, root, self.request)
            self.assertFalse(any(p.endswith('/zip') for p in self.requests))
            (root / 'github-actions-artifact-1.zip').write_bytes(b'corrupt')
            with self.assertRaisesRegex(ValueError, 'cached digest'):
                acquire(self.pins, root, self.request)

    def test_metadata_failure_before_download(self):
        cases = [('runs/101', 'conclusion', 'failure'), ('runs/101', 'status', 'queued'),
                 ('runs/101', 'head_sha', 'c'*40), ('runs/101', 'head_repository', {'full_name':'other/repo'}),
                 ('artifacts/6', 'expired', True), ('artifacts/6', 'digest', 'sha256:'+'0'*64),
                 ('artifacts/6', 'workflow_run', {'id': 999, 'head_sha': self.head}),
                 ('artifacts/6', 'workflow_run', {'id': 106, 'head_sha': 'c'*40})]
        baseline = copy.deepcopy(self.responses)
        for path, key, value in cases:
            self.responses = copy.deepcopy(baseline)
            self.responses[path][key] = value
            self.requests.clear()
            with self.subTest(path=path, key=key), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(ValueError):
                    acquire(self.pins, Path(tmp), self.request)
                self.assertEqual(list(Path(tmp).iterdir()), [])
                self.assertFalse(any(p.endswith('/zip') for p in self.requests))

    def test_corrupted_download(self):
        self.responses['artifacts/1/zip'] = b'wrong'
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, 'downloaded digest'):
                acquire(self.pins, Path(tmp), self.request)
            self.assertEqual(list(Path(tmp).iterdir()), [])


if __name__ == '__main__':
    unittest.main()
