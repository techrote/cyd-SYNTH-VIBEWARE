"""Frozen Chronomorph M4 replay using the previously qualified batch006 verifier."""
import sys
from pathlib import Path
import replay006

replay006.PINS['chrono'] = (
    15254564233, 'efaa3e40dab65023a3c22734eea3ef97e1d8d63d3d941490dc789d73055f62f6',
    'Chronomorph.zip', 'e57f1bc20de0924ec976ff6abce7b5cae903f1a2',
    'Chronomorph.exe', 'replay.exe', 'SHA256SUMS.txt',
    ('runtime-test.exe', 'gate-test.exe'))
if __name__ == '__main__':
    replay006.run('chrono', Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
