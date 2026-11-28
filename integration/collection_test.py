import io
import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile

from collection import load_outer, read_zip, sha, verify


def zipdata(entries):
    data = io.BytesIO()
    with ZipFile(data, 'w') as archive:
        for name, payload in entries:
            archive.writestr(name, payload)
    return data.getvalue()


class CollectionTests(unittest.TestCase):
    def test_zip_inventory(self):
        self.assertEqual(read_zip(zipdata([('media/a', b'a'), ('media/b', b'b')])),
                         {'media/a': b'a', 'media/b': b'b'})
        for entries in [[('../escape', b'a')], [('Media/a', b'a'), ('media/b', b'b')],
                        [('a', b'a'), ('a/b', b'b')], [('NUL.txt', b'a')]]:
            with self.subTest(entries=entries), self.assertRaises(ValueError):
                read_zip(zipdata(entries))

    def test_manifest(self):
        files = {'a': b'a', 'sum': (sha(b'a') + '  a\n').encode()}
        self.assertEqual(verify(files, 'sum'), 1)
        for bad in [dict(files, extra=b'x'), dict(files, a=b'x'),
                    dict(files, sum=files['sum'] * 2)]:
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                verify(bad, 'sum')
        self.assertEqual(verify(dict(files, sum=(sha(b'a') + ' a\n').encode()),
                                'sum', single=True), 1)

    def test_pinned_outer(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = zipdata([('a', b'a')])
            (root / 'github-actions-artifact-1.zip').write_bytes(payload)
            self.assertEqual(load_outer(root, {'artifact': 1, 'sha256': sha(payload)}), {'a': b'a'})
            with self.assertRaises(ValueError):
                load_outer(root, {'artifact': 1, 'sha256': '0' * 64})


if __name__ == '__main__':
    unittest.main()
