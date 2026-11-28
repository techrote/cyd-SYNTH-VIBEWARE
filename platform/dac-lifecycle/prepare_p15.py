"""Stage an isolated compile-only Grimoire M5 timer-DAC comparison."""
import pathlib
import shutil
import subprocess
import sys

root = pathlib.Path(sys.argv[1]).resolve()
pin = "9382d8beea9dc259cb148c68f1c2899a857e18d1"
if subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip() != pin:
    raise SystemExit("wrong consumer pin")
p = root / "instruments/fm1-ports/grimoire"
here = pathlib.Path(__file__).resolve().parent
for name in ("clock_sink.hpp", "timer_lifecycle.hpp", "esp32_timer_dac.hpp"):
    if (p / name).exists():
        raise SystemExit("unexpected consumer file: " + name)
    shutil.copyfile(here / name, p / name)
target = p / "target/main.cpp"
s = target.read_text()

def replace_once(old, new):
    global s
    if s.count(old) != 1:
        raise SystemExit("unexpected source anchor: " + old)
    s = s.replace(old, new, 1)

replace_once('#include "../panel.hpp"', '#include "../audio_owner.hpp"\n#include "../esp32_timer_dac.hpp"')
replace_once("BoardSession session;", """DRAM_ATTR cyd_platform::ClockSink sink;
cyd_platform::Esp32TimerDac driver(sink);
cyd_platform::TimerLifecycle<cyd_platform::Esp32TimerDac,cyd_platform::ClockSink> lifecycle(driver,sink);
AudioOwner<cyd_platform::ClockSink> session(sink);""")
replace_once("std::array<std::int16_t,128> pcm{};\nvolatile std::uint32_t digitalWitness=0;\n", "")
replace_once("std::uint32_t frameAt=0,touchAt=0,due=0;\nunsigned fraction=0;", "std::uint32_t frameAt=0,touchAt=0;")
start = s.index("void digitalBlock(")
end = s.index("void present(", start)
s = s[:start] + "void audioService(std::uint32_t now){session.fill(now);}\n" + s[end:]
replace_once("session.disarm();ready=false;return;", "session.disarm();lifecycle.stop();ready=false;return;")
replace_once("due=micros();ready=true;", "ready=lifecycle.start();")
replace_once("digitalBlock(micros());present(now);delay(1);", "audioService(now);present(now);audioService(millis());delay(1);")
replace_once("BOOT to arm digital test", "BOOT to arm - engineering")
replace_once("Tap 8 points - no DAC", "Tap 8 points")
replace_once("Digital test only - no DAC", "Timer DAC - engineering")
s = "// P15 experimental compile-only comparison; no physical authorization.\n" + s
target.write_text(s)
