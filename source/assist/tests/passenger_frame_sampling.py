#!/usr/bin/env python3
"""Run the shipped passenger display over the real ProcessReader/frame transport.

Only Win32 I/O, viewport geometry and the UTF-16/wchar display conversion are
controlled. The sampling/lease body, collector, motion, immutable pages and IPC
reader execute production logic.
This counts live RPM calls; it is not a game, GPU, Windows UI or FPS benchmark.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / 'source'
SOURCE = SRC.parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--out', required=True, type=Path)
p.add_argument('--host', type=Path, default=SRC / 'passenger_draw.h')
a = p.parse_args()
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=True)
core = (HERE / 'world_frame_core.cpp').read_text()
memory = core[core.index('namespace fixture {\nsize_t checks'):core.index('void overlap()')]
platform = core[core.index('#define WINMOCK_IMAGE_FILES'):core.index('\nnamespace assist {\nstruct FixtureShared')]
platform = platform.replace('#include "../../external_reader/source/process_reader.h"', '#include "' + str(SOURCE / 'external_reader/source/process_reader.h') + '"')
platform = platform.replace('#include "../source/world_lifecycle.h"', '')
windows = (SOURCE / 'tests/winmock/windows.h').read_text()
driver = (HERE / 'world_frame_core_test.py').read_text()
windows += driver.split('windows += r"""', 1)[1].split('"""', 1)[0]
host = a.host.read_text()
production = host[:host.index('static void drawPassengerBubble(')].replace('#include "native_viewport_host.h"', '')
# Win32 wchar_t is UTF-16; Linux wchar_t has different size and alignment.
# Adapt only this presentation conversion, leaving every read/guard/lease intact.
conversion = 'std::wstring line(reinterpret_cast<const wchar_t*>(group.name));'
assert production.count(conversion) == 1
production = production.replace(conversion, 'std::wstring line;for(uint16_t c:group.name){if(!c)break;line.push_back(wchar_t(c));}')
call = 'passengerView(now,transform,token,confirmed)' if 'const worldframe::Token&source' in production else 'passengerView(now,transform)'
production += '\nstatic auto passengerSample(uint32_t now,const worldframe::Token&token,bool confirmed=true){nativeviewhost::Transform transform;auto result=' + call + ';CHECK(!externalReader.worldFrameActive());return result;}\n'
main = (SRC / 'main.cpp').read_text()
demand = re.search(r'const bool passengerOverlay=[^;]+;', main).group()
demand += '\n' + re.search(r'externalReader\.setWorldDemand\([^;]+;', main).group()
fragments = {'windows.h': windows, 'host_platform.h': memory + '}\n' + platform,
             'passenger_production.h': production, 'passenger_demand.h': 'static void configureDemand(){' + demand + '}\n'}
for name, text in fragments.items():
    (out / name).write_text(text)
exe = out / 'passenger-frame-sampling'
flags = ['-std=c++17', '-O1', '-UNDEBUG', '-D_GLIBCXX_ASSERTIONS', '-fsanitize=undefined', '-fno-sanitize-recover=all', '-pthread', '-I', str(out), '-I', str(SRC)]
runs = []
for i, cmd in enumerate([['g++', *flags, str(HERE / 'passenger_frame_sampling.cpp'), '-o', str(exe)], [str(exe)]]):
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    (out / ('build.log' if i == 0 else 'run.log')).write_text(r.stdout + r.stderr)
    print(r.stdout + r.stderr, end='')
    runs.append(dict(command=cmd, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr))
    if r.returncode:
        break
paths = [a.host, SRC / 'main.cpp', SRC / 'passenger_motion.h', SRC / 'display_sample_lease.h', SRC / 'label_motion.h',
         SOURCE / 'external_reader/source/passenger_bubbles.h', SOURCE / 'external_reader/source/game_view.h',
         SOURCE / 'external_reader/source/process_reader.h', SOURCE / 'shared/source/world_frame.h',
         SOURCE / 'shared/source/world_frame_capture.h', SOURCE / 'shared/source/world_frame_transport.h',
         HERE / 'world_frame_core.cpp', HERE / 'world_frame_core_test.py', HERE / 'passenger_frame_sampling.cpp', Path(__file__).resolve()]
report = {'pass': all(r['exit_code'] == 0 for r in runs), 'scope': __doc__, 'runs': runs,
          'source_sha256': {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in paths},
          'extracted_sha256': {name: hashlib.sha256(text.encode()).hexdigest() for name, text in fragments.items()},
          'windows_executed': False, 'actual_game': False, 'hardware_gpu': False, 'multiplayer': False}
(out / 'receipt.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
raise SystemExit(0 if report['pass'] else 1)
