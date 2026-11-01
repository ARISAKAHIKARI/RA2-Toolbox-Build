"""Compile actual Host placement, UI mode routes and persistence for both ABIs."""
import argparse
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--zig', required=True)
p.add_argument('--out', required=True)
a = p.parse_args()
here = Path(__file__).resolve().parent
src = here.parent / 'source'
out = Path(a.out).resolve()
out.mkdir(parents=True, exist_ok=True)
main = (src / 'main.cpp').read_text(encoding='utf-8-sig')
io = (src / 'extra_settings_io.h').read_text(encoding='utf-8-sig')
ui = (src / 'auto_settings_ui.h').read_text(encoding='utf-8-sig')
keys = (src / 'hotkey_host.h').read_text(encoding='utf-8-sig')

def block(text, needle):
    start = text.index(needle)
    at = text.index('{', start)
    depth = 1
    end = at + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def statement(text, prefix):
    start = text.index(prefix)
    return text[start:text.index(';', start) + 1]

tech = block(main, 'else if(id==896&&').removeprefix('else ')
placement = block(ui, 'if(id==9096&&')
dispatch = block(keys, 'static bool dispatchHotkeyGroup(')
load = '\n'.join((statement(io, 'extra.placementImplementation=buildingmodes::placement('),
                  statement(io, 'extra.placementEventKey=value('),
                  statement(main, 'techImplementation=buildingmodes::technology(value(')))
save = '\n'.join((statement(io, 'write(L"Experimental",L"PlacementImplementation",'),
                  statement(io, 'write(L"Hotkeys",L"PlaceCompletedBuilding",'),
                  statement(main, 'write(L"Tech",L"Implementation",')))
arg = statement(main, 'if(command==OpenTech)arg2=techImplementation;')
generated = dispatch + '\nstatic void fixtureLoad(){' + load + '}\n'
generated += 'static void fixtureSave(){' + save + '}\n'
generated += 'static void fixtureTechRoute(int id,int notification){HWND w=ui;WPARAM wp=MAKEWPARAM(id,notification);' + tech + '}\n'
generated += 'static bool fixturePlacementRoute(int id,int notification){bool select=notification==CBN_SELCHANGE;' + placement + 'return false;}\n'
generated += 'static void send(int command,int arg,int arg2){' + block(main, 'if((command==Produce||command==CancelTechProduction)&&') + arg + 'sentCommand=command;sentArgument=arg;sentImplementation=arg2;}\n'
inc = out / 'production-routes.inc'
inc.write_text(generated, encoding='utf-8')

# Secondary source contracts cover the gates surrounding the compiled routes.
assert 'if(c==PlaceCompletedBuilding)return false;' in block(main, 'static bool queueCommand(')
assert block(main, 'static void send(int command').split('{', 1)[1].lstrip().startswith('if(command==PlaceCompletedBuilding)')
assert 'queueCommand' not in block(main, 'if(c==PlaceCompletedBuilding){')
assert keys.count('hotkeyPending.add(') == 1  # only after immediate extraction
assert 'placementeventhost::immediate(delayed,' in keys
assert 'GetMessageTime()' in keys
assert 'shared->placementImplementation=extra.placementImplementation;' in io
results = {}
for arch in ('x86_64', 'x86'):
    exe = out / (arch + '.exe')
    command = [str(Path(a.zig).resolve()), 'c++', '-target', arch + '-windows-gnu', '-std=c++17', '-O1', '-UNDEBUG',
               str(here / 'building_event_host.cpp'), '-DHOST_BUILDING_ROUTES="' + inc.as_posix() + '"', '-o', str(exe), '-luser32']
    built = subprocess.run(command, capture_output=True, text=True)
    (out / (arch + '-build.log')).write_text(built.stdout + built.stderr, encoding='utf-8')
    if built.returncode:
        raise SystemExit(built.stdout + built.stderr)
    ran = subprocess.run([str(exe)], capture_output=True, text=True)
    (out / (arch + '.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8')
    if ran.returncode:
        raise SystemExit(ran.stdout + ran.stderr)
    results[arch] = json.loads(ran.stdout.strip().splitlines()[-1])
(out / 'result.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
print(json.dumps(results, indent=2))
