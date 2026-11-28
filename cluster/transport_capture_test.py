import unittest
from transport_capture import compare, read_jobs, stats


def log(modes, offset=0):
    return "\n".join(f"job seed={i} mode={mode} elapsed_us={i * 100 + offset} "
                     f"verify_local_us=99 match=1 grid={i:08x}" for i, mode in enumerate(modes, 1))


class TransportCaptureTests(unittest.TestCase):
    def test_comparison(self):
        r = compare([("local", log(["local"] * 4)),
                     ("udp", log(["remote", "remote", "local", "remote"], 20))], 4, 1)
        self.assertTrue(r["matched_grid_digests"])
        self.assertEqual(r["captures"][1]["remote"], 2)
        self.assertEqual(r["captures"][1]["local"], 1)
        self.assertEqual(r["captures"][1]["remote_elapsed_us"]["p50"], 220)
        self.assertEqual(r["captures"][1]["elapsed_us"]["p95"], 420)
        self.assertEqual(r["captures"][1]["completed_remote_result_payload_bytes"], 256)
        self.assertIsNone(r["captures"][0]["remote_elapsed_us"])

    def test_reject_bad_job(self):
        good = log(["local"] * 2)
        for bad in (good.replace("match=1", "match=0"), good.replace("seed=2", "seed=1"),
                    good.replace("elapsed_us=100", "elapsed_us=xyz"), good + "\nMISMATCH: stopped",
                    good.splitlines()[0], good + "\njob seed=3"):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                read_jobs(bad, 2)

    def test_reject_mismatched_comparison(self):
        good = log(["local"] * 2)
        remote = log(["remote"] * 2)
        for c, n, w in (([("r", remote), ("l", good)], 2, 0),
                        ([("l", good), ("r", remote.replace("00000002", "00000003"))], 2, 0),
                        ([("l", good), ("l", remote)], 2, 0),
                        ([("l", good), ("r", remote)], 2, 2),
                        ([("l", good)], 2, 0)):
            with self.assertRaises(ValueError):
                compare(c, n, w)

    def test_nearest_rank(self):
        self.assertEqual(stats([5, 1, 3]), dict(count=3, p50=3, p95=5, p99=5, max=5))
        self.assertIsNone(stats([]))


if __name__ == "__main__":
    unittest.main()
