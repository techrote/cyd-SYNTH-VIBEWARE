#!/usr/bin/env python3
"""Prepare isolated non-flashing XENOBIO display A/B/C builds from exact M4R."""
import argparse
import hashlib
from pathlib import Path

PINS = {
    'core.hpp': '6953826440779ef79a53adc76d63b2829079ed88',
    'labeled.hpp': 'c35cae93e683db681f5ccd32e34fece8e919d3a0',
    'runtime.hpp': '33d4229f36bf6c8d487dd6cb7c0581248befd981',
    'target_input.hpp': '0242c9305b1dfff902dbca6e73abab198d231d68',
    'firmware/src/main.cpp': 'dd7947fcca5cc6487db50814ed855e6ccb9fc790',
    'firmware/platformio.ini': 'f9293158373f647e134e8bbafdee99b47a667800',
}
PLATFORM = ('span.hpp', 'dirty_scan.hpp', 'scan_adapter.hpp')
MODES = {'original': 0, 'span': 1, 'selective': 2}


def blob(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def replace_once(s, old, new):
    if s.count(old) != 1:
        raise ValueError('source anchor mismatch: ' + old)
    return s.replace(old, new, 1)


def prepare(source, out):
    files = {p: (source / p).read_bytes() for p in PINS}
    for p, data in files.items():
        if blob(data) != PINS[p]:
            raise ValueError('exact M4R blob mismatch: ' + p)
    platform = {p: (Path(__file__).parent / p).read_bytes() for p in PLATFORM}
    # Never overwrite an existing checkout or prior run.
    out.mkdir(parents=True, exist_ok=False)
    for mode, value in MODES.items():
        target = out / mode
        for p, data in {**files, **platform}.items():
            dest = target / p
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
        main = files['firmware/src/main.cpp'].decode()
        main = replace_once(main, '#include "../../labeled.hpp"', '#include "../../scan_adapter.hpp"')
        main = replace_once(main, 'constexpr unsigned block=128, strip=4;', 'constexpr unsigned block=128;')
        start = main.index('unsigned row=240;')
        end = main.index('\n\nvoid audioTask', start)
        main = main[:start] + '''uint32_t touchAt=0;
static_assert(sizeof(lgfx::rgb565_t)==2,"RGB565 storage required");
using cyd::platform::XenobioScanMode;
cyd::platform::XenobioScanAdapter<static_cast<XenobioScanMode>(XENO_SCAN_MODE),lgfx::rgb565_t> display;''' + main[end:]
        start = main.index('void presentStrip(uint32_t now) {')
        end = main.index('\n}\n}\nvoid setup()', start) + 2
        main = main[:start] + '''void presentStrip(uint32_t now) {
    display.step(now,[](){return runtime.snapshot();},[](int y,lgfx::rgb565_t* pixels){
        lcd.pushImage(0,y,320,4,pixels);
        lcd.waitDMA();
        return true; // original driver path exposes no transfer-failure receipt here
    });
}''' + main[end:]
        (target / 'firmware/src/main.cpp').write_bytes(main.encode())
        ini = files['firmware/platformio.ini'].decode()
        ini = replace_once(ini, '-DLGFX_USE_V1 -O2', f'-DLGFX_USE_V1 -O2 -DXENO_SCAN_MODE={value}')
        (target / 'firmware/platformio.ini').write_bytes(ini.encode())
        print(mode, 'main', blob(main.encode()), 'ini', blob(ini.encode()))
    print('Prepared source only; no compile, flash, or physical result.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='exact M4R instruments/xenobio directory')
    parser.add_argument('out', type=Path, help='new disposable output directory')
    args = parser.parse_args()
    prepare(args.source, args.out)
