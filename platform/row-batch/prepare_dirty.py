"""Prepare an opt-in P6 adapter in an exact disposable CHRONOGRID M4 checkout."""
import hashlib
from pathlib import Path
import subprocess
import sys

PIN = 'c0a866a71ecf0a6be958a04f9870b92d0a7fe069'
EXPECTED = {
    'core.hpp': '1938c700c31b47188d571664594f1e9bfeb6e4bf',
    'firmware/src/main.cpp': 'fb2b3eb08fdce7ecd4241e0ecc5c760d07833e2a',
    'firmware/platformio.ini': '0c6f58497b151dbbc52461cae7c3f9149dc088b1',
}
HEADERS = ('typed_strip.hpp', 'dirty_grid.hpp', 'dirty_display.hpp')
REPLACEMENTS = [
  [
    "#include \"../../core.hpp\"",
    "#include \"../../core.hpp\"\n#include \"../../dirty_display.hpp\""
  ],
  [
    "static Surface surface;static Exchange exchange;static Control frameControl;\nstatic uint32_t frameHead=0;static std::atomic<uint32_t> head{0};",
    "static Surface surface;static Exchange exchange;\nstatic std::atomic<uint32_t> head{0};"
  ],
  [
    "static uint32_t inputAt=0,frameAt=0;static unsigned scan=height;\nstatic std::array<uint16_t,width> rowbuf{};\nstatic std::array<lgfx::rgb565_t,width> line{};",
    "static uint32_t inputAt=0,frameAt=0;\nstatic cyd::platform::DirtyDisplay<lgfx::rgb565_t> dirtyDisplay;"
  ],
  [
    "display.fillScreen(uint16_t(0));digitalWrite(21,HIGH);",
    "display.fillScreen(uint16_t(0));dirtyDisplay.invalidate();digitalWrite(21,HIGH);"
  ],
  [
    "        if(scan==height&&uint32_t(now-frameAt)>=50){\n            frameControl=surface.c;frameHead=head.load(std::memory_order_acquire);\n            if(!armed){frameControl.play=false;frameHead=0;}\n            scan=0;frameAt=now;\n        }\n        if(scan<height){\n            display.startWrite();\n            for(unsigned n=0;n<4&&scan<height;++n,++scan){\n                gridseq::row(frameControl,frameHead,scan,rowbuf.data());\n                for(unsigned x=0;x<width;++x)line[x]=rowbuf[x];\n                display.pushImage(0,int(scan),int(width),1,line.data());\n            }\n            display.endWrite();\n        }",
    "        if(!dirtyDisplay.busy()&&uint32_t(now-frameAt)>=50){\n            Control frameControl=surface.c;\n            uint32_t frameHead=head.load(std::memory_order_acquire);\n            if(!armed){frameControl.play=false;frameHead=0;}\n            dirtyDisplay.begin(frameControl,frameHead);frameAt=now;\n        }\n        if(dirtyDisplay.busy())dirtyDisplay.step(display);"
  ]
]

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
    src = main.read_text(encoding='utf-8')
    for old,new in REPLACEMENTS:
        if src.count(old) != 1:
            raise RuntimeError('unexpected adapter context: '+old)
        src = src.replace(old,new)
    for name in HEADERS:
        if (base/name).exists():
            raise RuntimeError('refusing overwrite: '+name)
    headers = {name: Path(__file__).with_name(name).read_bytes() for name in HEADERS}
    for name, data in headers.items():
        (base/name).write_bytes(data)
    main.write_text(src,encoding='utf-8',newline='\n')
    print('Prepared exact M4 P6: selective typed strips; four-row loop budget.')

if __name__ == '__main__':
    prepare(sys.argv[1])
