"""Compile actual merged Host config/dispatch/input routes; no game attachment."""
import argparse
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--zig', required=True)
p.add_argument('--out', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
src = root / 'assist/source'
out = Path(a.out).resolve()
out.mkdir(parents=True, exist_ok=True)

def read(name):
    return (src / name).read_text(encoding='utf-8-sig')

def block(text, needle):
    start = text.index(needle)
    begin = text.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

io = read('extra_settings_io.h')
load = block(io, 'static void loadAdditionalSettings()')
save = block(io, 'static bool saveAdditionalSettings()')
sync = block(io, 'static void syncAdditional()')
route = block(read('auto_settings_ui.h'), 'static bool autoSettingChanged(')
preview = read('mo_orders_host.h')
new_load = load[load.index(' superweaponOptions.detached='):load.index(' // Older releases')]
new_load += load[load.index(' extra.moFeatures='):load.index(' extra.techEnhance=')]
new_save = save[save.index(' write(L"Display",L"SuperweaponDetached"'):save.index(' write(L"ViewZoom",L"Enabled"')]
new_route = route[route.index(' if(id>=9440'):route.index(' if(id==886')]
new_sync = sync[sync.index(' shared->moFeatures='):sync.index(' shared->buildTimeTooltip=')]
pointer = read('pointer_host.h')
guard = ' if(middleinput::ownsPointer(e.operation==7?6:e.operation,runtimeGesturePolicy(),runtimeGestureEnabled(),e.keys))return;'
assert guard in pointer
guard = guard.replace('return;', 'return false;')
generated = ('static void loadNewSettings(){\n' + new_load + '}\n'
             'static void saveNewSettings(){\n' + new_save + '}\n'
             'static void publishNewSettings(){\n' + new_sync + '}\n'
             'static bool changeNewSettings(int id,int notification){const bool edit=notification==EN_CHANGE,select=notification==CBN_SELCHANGE;\n' + new_route + '\nreturn false;}\n'
             'static bool oldPointerAllowed(int operation,const uint8_t*keys){struct{int operation;const uint8_t*keys;}e{operation,keys};\n' + guard + '\nreturn true;}\n'
             + block(preview, 'struct Drawing {') + ';\n'
             + block(preview, 'static bool sameDrawing(') + '\n')
inc = out / 'actual-host-routes.inc'
inc.write_text(generated, encoding='utf-8')
main = read('main.cpp')
assert 'runtimeFeatureDemand(configured,extra,techMode,buildTimeTooltipEnabled)' in main
assert pointer.index(guard.replace('return false;', 'return;')) < pointer.index('if(e.operation==5&&hookSmartUnload')
assert 'formationInputOwns()||runtimeMiddleOwnsNow()||!context' in read('hotkey_host.h')
assert 'if(runtimeMiddleOwnsNow()){typeTap.clear();variantTap.clear();hotkeyPending.clear();hotkeyRegistry.cancel();return;}' in read('hotkey_host.h')
assert main.count('serviceSuperweaponPanel();') == 1
assert 'send(SelectSuper,index,int(instance),0,false,type)' in main
assert 'superweaponOptions.enabled=enabled&&extra.superweaponPanel&&overlayvisibility::visible();' in main
for removed in ('nativeviewportprojection', 'viewportSerial'):
    assert removed not in main + io
assert 'shared->sidebarMode=extra.sidebarEnabled&&extra.sidebarMode==2?2u:0u;' in io
assert 'shared->pauseViewKeys[0]=extra.pauseKeys[0]' in io
controls = read('tactical_controls.h')
for id in (9400, 9401, 9402, 9405):
    assert controls.count('toggle(' + str(id) + ',') == 1
assert 'toggle(9403,' in controls and 'toggle(9462,' in controls
assert 'EnableWindow(GetDlgItem(ui,9404),FALSE)' not in controls
assert '严格无代码补丁核心' not in controls
result = {'scope': 'extracted actual Host routes and headers, not live game', 'architectures': {}}
for arch in ('x86_64', 'x86'):
    logs = {}
    for name, flags in (('gesture_host_merge', ['-DGESTURE_HOST_PRODUCTION="' + inc.as_posix() + '"']),
                        ('middle_input_owner', []), ('runtime_feature_demand', [])):
        exe = out / (name + '-' + arch + '.exe')
        cmd = [str(Path(a.zig).resolve()), 'c++', '-target', arch + '-windows-gnu', '-std=c++17', '-O1', '-UNDEBUG',
               str(root / 'assist/tests' / (name + '.cpp')), *flags, '-o', str(exe)]
        built = subprocess.run(cmd, capture_output=True, text=True)
        (out / (name + '-' + arch + '-build.log')).write_text(built.stdout + built.stderr, encoding='utf-8')
        if built.returncode:
            raise SystemExit(built.stdout + built.stderr)
        ran = subprocess.run([str(exe)], capture_output=True, text=True)
        (out / (name + '-' + arch + '.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8')
        if ran.returncode:
            raise SystemExit(ran.stdout + ran.stderr + str(ran.returncode))
        logs[name] = ran.stdout.strip() or 'PASS exhaustive assertion fixture'
    result['architectures'][arch] = logs
(out / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
