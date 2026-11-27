import unittest
from bench_capture import parse,compare,hist_summary

def fixture(nodes=0, bad=False, audio=0):
    rows=[f"bench begin schema=1 nodes={nodes} remote={int(nodes>0)} audio={audio} count=64 steps=64 particles=1024"]
    rows += [f"bench sample seed={i} us={i*2} grid={i+int(bad):08x} remote={nodes}" for i in range(1,65)]
    for n in ("control","loop"):
        rows.append(f"bench hist name={n} counts="+",".join(["64"]+["0"]*23))
    for slot in range(nodes):
        rows.append(f"bench peer slot={slot} sent=64 retry=0 remote=64 local=0 busy=0")
    rows.append("bench end reason=complete count=64 elapsed_us=16000000 frames=100 audio_blocks=0 audio_errors=0 heap_min_sample=10000 tx_fail=0 rx_drop=0 rx_reject=0")
    return "\n".join(rows)

class CaptureTests(unittest.TestCase):
    def test_matched(self):
        runs=parse(fixture()+"\n"+fixture(2))
        self.assertEqual(runs[1]["remote_cohorts"],128)
        self.assertEqual(runs[0]["batch_us"]["p95"],122)
        self.assertTrue(all(x["digest_match"] for x in compare(runs)))
    def test_bad(self):
        self.assertFalse(compare(parse(fixture()+"\n"+fixture(2,True)))[1]["digest_match"])
        self.assertIsNone(compare(parse(fixture(2)))[0]["digest_match"])
        self.assertIsNone(compare(parse(fixture()+"\n"+fixture(1,audio=1)))[1]["digest_match"])
    def test_truncated(self):
        with self.assertRaises(ValueError): parse(fixture().rsplit("\n",1)[0])
        with self.assertRaises(ValueError): parse(fixture().replace("seed=2 ","seed=1 "))
        with self.assertRaises(ValueError): parse(fixture().replace("count=64 elapsed","count=63 elapsed"))
        with self.assertRaises(ValueError): parse(fixture().replace("name=loop","name=control"))
    def test_bounds(self):
        self.assertEqual(hist_summary([0]*23+[1])["p95_upper_us"],None)
        self.assertEqual(hist_summary([0,1]+[0]*22)["p50_upper_us"],2)

if __name__=="__main__":
    unittest.main()
