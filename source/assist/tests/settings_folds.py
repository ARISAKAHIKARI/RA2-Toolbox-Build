import argparse
import json
from pathlib import Path
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
controls = (src / 'tactical_controls.h').read_text(encoding='utf-8-sig')
def block(text, needle):
    start = text.index(needle)
    at = text.index('{', start)
    depth, end = 1, at + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
generated = block(main, 'struct PageControl {') + ';\nstatic std::vector<PageControl> pageControls;\n'
for needle in ('static void applySettingsFolds()', 'static int pageBottom()', 'static void finalizeTacticalLayout()', 'static bool toggleSettingsFold(', 'static void setConsoleFeedback(HWND control,const std::wstring&text){', 'static void updateConsoleFoldErrors(){', 'static void updateControlModeStatus(DWORD now)'):
    generated += block(main, needle) + '\n'
focus = block(main, 'case modernui::FocusMessage:')
generated += 'static LRESULT focusRoute(HWND w,LPARAM lp){' + focus[focus.index('{') + 1:-1] + '}\n'
search = block(main, 'if(id==991)')
generated += 'static LRESULT searchRoute(){' + search[search.index('{') + 1:-1] + '}\n'
low_start=main.index(' const int lowDamage=shared?shared->lowDamageStatus:0;')
low_end=main.index('\n {',low_start)
generated+='static void fixtureLowDamage(){'+main[low_start:low_end]+'}\n'
generated+=block(main, 'static bool networkHouseReady(){')+'\n'
generated+='static void fixtureAdaptationStatus(DWORD now=100)'+block(main,' {\n  const bool fresh=shared&&commandFresh(now);')+'\n'

assert 'updateConsoleFoldErrors();' in block(main, 'static void updateModeStatus(')
assert 'static void updateControlModeStatus' not in block(main,'static void updatePlayerTestUI(')

inc = out / 'production-layout.inc'
inc.write_text(generated, encoding='utf-8')
for group, children in ((9601, ('combo(9195', 'number(710', 'number(718', 'mouse(pointerinput::MouseAirFirst')),
                        (9602, ('combo(9056', 'key(9051', '9058', 'key(9446', 'combo(9447')),
                        (9603, ('combo(9040', 'combo(9041', 'number(881')),
                        (9604, ('key(870', 'socialKeys[i]')),
                        (9605, ('oreStatus=', 'oreMiners', 'oreRefineries')),
                        (9606, ('key(450', 'MouseAdvanced', 'number(787', 'number(818', 'combo(802')),
                        (9607, ('edit(421', '422,', 'button(423', 'button(428', 'button(814', '815,')),
                        (9608, ('combo(440', 'key(441', 'edit(467', 'number(444', 'toggle(445')),
                        (9609, ('compute::Count', 'combo(9000+i', '833,')),
                        (9610, ('featuretypes::Count', '9200+f*2', '9201+f*2')),
                        (9611, ('for(int i:{9,10})', 'swList=combo(410')),
                        (9612, ('combo(9250', '865)')),
                        (9613, ('9197,',)),
                        (9614, ('9188,',)),
                        (9615, ('number(9480', 'number(9485', 'toggle(9486'))):
    section = controls.split('activeFoldGroup=' + str(group) + ';', 1)[1].split('activeFoldGroup=0;', 1)[0]
    assert all(child in section for child in children)
    assert 'button(' + str(group) + ',L"展开设置"' in controls
assert controls.count('toggle(705,') == controls.count('number(710,') == controls.count('toggle(899,') == 1
assert '磁电全局自动摔落（实验性）' not in controls
assert 'if(c.foldHidden){openedSettingGroups.insert(c.fold);applySettingsFolds();' in main
assert block(main, 'static void layoutPanel()').count('!c.foldHidden') == 3
assert 'activeFoldGroup' in block(main, 'static HWND placedControl(')
assert 'if(HIWORD(wp)==BN_CLICKED&&toggleSettingsFold(id))return 0;' in main
assert controls.count('number(787,') == controls.count('number(818,') == controls.count('combo(802)') == 1
tech = controls.split('activeFoldGroup=9607;', 1)[1].split('activeFoldGroup=0;', 1)[0]
assert 'toggle(420,' not in tech and 'combo(896' not in tech and '815,' in tech
assert controls.count('activeFoldGroup=0;') == controls.count('activeFoldGroup=') // 2
results = {}
for arch in ('x86_64', 'x86'):
    exe = out / (arch + '.exe')
    cmd = [str(Path(a.zig).resolve()), 'c++', '-target', arch + '-windows-gnu', '-std=c++17', '-O1', '-UNDEBUG',
           str(here / 'settings_folds.cpp'), '-DSETTINGS_FOLD_ROUTES="' + inc.as_posix() + '"', '-o', str(exe)]
    built = subprocess.run(cmd, capture_output=True, text=True)
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
