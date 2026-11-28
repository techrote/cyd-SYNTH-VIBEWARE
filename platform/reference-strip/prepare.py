"""Prepare opt-in consumer only in a disposable checkout of exact PHASESPACE M3."""
from pathlib import Path
import hashlib, shutil, sys, subprocess

root = Path(sys.argv[1]).resolve()
pin = "0139740e3db44496a779d6629b424397f9f53dfb"
assert subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip() == pin
base = root / "instruments/phasespace"
def blob(path):
    # Git Windows worktrees may use CRLF; compare canonical LF source blobs.
    data = path.read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
assert blob(base / "core.hpp") == "50b8bb909b0c54480bf6a1180d830083ba4cbdaa"
assert blob(base / "firmware/src/main.cpp") == "becd2a5871a93ecf94c469b49a6c311bf748f956"
assert blob(base / "windows.cpp") == "0a9ba2efa645971a50af0a318defec9634a89dd3"
assert blob(base / "replay.cpp") == "9a854bb0822449f905f7b247ec872daa3466db63"
here = Path(__file__).resolve().parent
shutil.copyfile(here / "strip.hpp", base / "strip.hpp")
shutil.copyfile(here / "strip_test.cpp", base / "strip_test.cpp")
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
print("Prepared disposable exact-M3 consumer; no ref writes.")
