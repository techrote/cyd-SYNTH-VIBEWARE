"""Prepare opt-in reference only in a disposable exact M3 or M4 checkout."""
from pathlib import Path
import hashlib, shutil, sys, subprocess

root = Path(sys.argv[1]).resolve()
pins = {
    "0139740e3db44496a779d6629b424397f9f53dfb": ("50b8bb909b0c54480bf6a1180d830083ba4cbdaa", "9a854bb0822449f905f7b247ec872daa3466db63"),
    "072ede0c2909836f90639d2112ea3786d770795d": ("6b254f4da1a876f1cbd73e08d557ba5c82f29f8f", "40ff5073b6407ccf2ba734bbb6b1a75fa5c41bb0"),
}
pin = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
if pin not in pins:
    raise RuntimeError("unsupported exact consumer revision")
m4 = pin == "072ede0c2909836f90639d2112ea3786d770795d"
base = root / "instruments/phasespace"
def blob(path):
    data = path.read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
expected = {
    "core.hpp": pins[pin][0],
    "replay.cpp": pins[pin][1],
    "firmware/src/main.cpp": "becd2a5871a93ecf94c469b49a6c311bf748f956",
    "windows.cpp": "0a9ba2efa645971a50af0a318defec9634a89dd3",
    "CMakeLists.txt": "eb4e0b1449c8633c14a25eeaba624da1aaf0486d",
    "firmware/platformio.ini": "114d8a1de1f12f6444c6ea7004768e44bc6e46b6",
}
for path, sha in expected.items():
    if blob(base / path) != sha:
        raise RuntimeError(f"unexpected source blob: {path}")
here = Path(__file__).resolve().parent
for name in ("strip.hpp", "strip_test.cpp", "topology_test.cpp"):
    if (base / name).exists():
        raise RuntimeError(f"refusing to overwrite {name}")
shutil.copyfile(here / "strip.hpp", base / "strip.hpp")
shutil.copyfile(here / "strip_test.cpp", base / "strip_test.cpp")
if m4:
    shutil.copyfile(here / "topology_test.cpp", base / "topology_test.cpp")
def edit(path, pairs):
    s = path.read_text(encoding="utf-8")
    for a,b in pairs:
        if s.count(a) != 1:
            raise RuntimeError(f"expected exactly one {a!r} in {path}")
        s = s.replace(a,b)
    path.write_text(s, encoding="utf-8", newline="\n")
edit(base / "firmware/src/main.cpp", [
    ('#include "../../core.hpp"', '#include "../../strip.hpp"'),
    ('static Trace trace,frame;', 'static cyd_platform::PhaseStripTrace trace,frame;'),
    ('std::array<uint16_t,width> row{};', 'std::array<uint16_t,width*4> row{};'),
    ('for(unsigned n=0;n<4&&scan<height;++n,++scan){\n                frame.row(frameControl,scan,row.data());',
     'frame.strip(frameControl,scan,std::min(4u,height-scan),row.data());\n            for(unsigned n=0;n<4&&scan<height;++n,++scan){'),
    ('line[x]=row[x];', 'line[x]=row[n*width+x];'),
])
edit(base / "windows.cpp", [
    ('#include "core.hpp"', '#include "strip.hpp"'),
    ('static Trace trace;', 'static cyd_platform::PhaseStripTrace trace;'),
    ('std::array<uint16_t,width> line{};', 'std::array<uint16_t,width*4> line{};'),
    ('for(unsigned y=0;y<height;++y){trace.row(surface.c,y,line.data());for(unsigned x=0;x<width;++x){',
     'for(unsigned y=0;y<height;++y){if(y%4==0)trace.strip(surface.c,y,std::min(4u,height-y),line.data());for(unsigned x=0;x<width;++x){'),
    ('auto v=line[x];', 'auto v=line[(y%4)*width+x];'),
])
with (base / "CMakeLists.txt").open("a", encoding="utf-8") as f:
    f.write("\nadd_executable(platform_strip_test strip_test.cpp)\nadd_test(NAME platform_strip_test COMMAND platform_strip_test)\n")
    if m4:
        f.write("\nadd_executable(platform_topology_test topology_test.cpp)\nadd_test(NAME platform_topology_test COMMAND platform_topology_test)\n")
print(f"Prepared disposable exact consumer {pin}; no ref writes.")
