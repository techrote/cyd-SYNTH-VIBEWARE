"""Opt-in CHRONOGRID M3 target adapter in a disposable exact checkout."""
import hashlib
from pathlib import Path
import subprocess
import sys

PIN = '9f7b1ff5bead8d718befd1284b6bd4ae1a4b67ed'
EXPECTED = {
    'core.hpp': 'c3a2557abdff7e7ef198d7735670d022e66b0f74',
    'firmware/src/main.cpp': 'fb2b3eb08fdce7ecd4241e0ecc5c760d07833e2a',
    'firmware/platformio.ini': '0c6f58497b151dbbc52461cae7c3f9149dc088b1',
}
REPLACEMENTS = (
    ('#include "../../core.hpp"', '#include "../../core.hpp"\n#include "typed_strip.hpp"'),
    ('static std::array<uint16_t,width> rowbuf{};\nstatic std::array<lgfx::rgb565_t,width> line{};',
     'static cyd::platform::TypedStrip<lgfx::rgb565_t,width,height,4> strip{};'),
    ('''            display.startWrite();
            for(unsigned n=0;n<4&&scan<height;++n,++scan){
                gridseq::row(frameControl,frameHead,scan,rowbuf.data());
                for(unsigned x=0;x<width;++x)line[x]=rowbuf[x];
                display.pushImage(0,int(scan),int(width),1,line.data());
            }
            display.endWrite();''',
     '''            scan+=strip.submit(scan,4,[&](unsigned y,uint16_t* p){
                gridseq::row(frameControl,frameHead,y,p);
            },display);'''),
)


def blob(data):
    return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()


def prepare(root):
    root = Path(root).resolve()
    head = subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    if head != PIN:
        raise RuntimeError('unsupported consumer revision')
    if subprocess.check_output(['git','-C',str(root),'status','--porcelain'],text=True).strip():
        raise RuntimeError('consumer checkout is not clean')
    base = root/'instruments/chronogrid'
    for name, sha in EXPECTED.items():
        if blob((base/name).read_bytes().replace(b'\r\n',b'\n')) != sha:
            raise RuntimeError('unexpected source blob: '+name)
    main = base/'firmware/src/main.cpp'
    header = main.with_name('typed_strip.hpp')
    if header.exists():
        raise RuntimeError('refusing overwrite')
    src = main.read_text(encoding='utf-8')
    for old,new in REPLACEMENTS:
        if src.count(old)!=1:
            raise RuntimeError('unexpected adapter context: '+old)
        src = src.replace(old,new)
    header.write_bytes(Path(__file__).with_name('typed_strip.hpp').read_bytes())
    main.write_text(src,encoding='utf-8',newline='\n')
    print('Prepared exact CHRONOGRID M3; 4-row typed strip, same scan budget and snapshot.')


if __name__ == '__main__':
    prepare(sys.argv[1])
