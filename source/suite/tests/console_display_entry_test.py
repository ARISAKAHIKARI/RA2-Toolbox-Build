"""Exercise visible Video entry creation -> Assist owner -> saved setting.

Compiles the production VideoSettings class and extracts the current production
display forwarding block, owner branches and INI statements. Only Win32 controls,
message delivery and the profile store are simulated. This does not run a game or
claim Windows visual coverage. --headers can point to frozen old suite headers
for an executable negative control; production owner statements remain explicit.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import tempfile


# Original bytes at the released pre.5 source revision. The negative control
# must remain reproducible without the old git object or a network checkout.
PRE5_SOURCE = '18af5094f1cbd955ebaac9fbe3c85a0682ef7ce9'
PRE5_SHA256 = {
    'display_settings.h': 'a26736afe50c92049789f77340695d5ffbd56f5b0f3fb8d37de3a2b1d2673772',
    'settings_dependencies.h': 'b9671635669d63170a297d65cd02e37978be783822c30a80399f22ab9c5794f1',
    'video_settings_ui.h': '1e584429f6ab18a3e22cd69931e707bf17edc5ffb3cf3bc6f989a9b667cfc240',
}
PRE5_FAILURES = [
    'FAIL missing SuperweaponPanel display spec',
    'FAIL missing DetailedStatus display spec',
    'FAIL live camera motion owner is independent of unavailable renderer',
    'FAIL live camera motion visible switch is independently enabled',
    'FAIL motion switch reaches shared setting owner',
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def block(text, marker):
    start = text.index(marker)
    opening = text.index('{', start)
    depth = 1
    at = opening + 1
    while depth:
        depth += (text[at] == '{') - (text[at] == '}')
        at += 1
    return text[start:at]


def statement(text, marker):
    start = text.index(marker)
    return text[start:text.index(';', start) + 1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--headers', type=Path)
    parser.add_argument('--negative-control', action='store_true',
                        help='Also compile/run SHA-verified pre.5 headers; require the five known assertion failures.')
    args = parser.parse_args()
    if args.headers and args.negative_control:
        parser.error('--negative-control compares current production headers; do not combine with --headers')
    source = Path(__file__).resolve().parents[2]
    suite = source / 'suite/source'
    assist = source / 'assist/source'
    headers = args.headers.resolve() if args.headers else suite
    dependencies = headers / 'settings_dependencies.h'
    if not dependencies.exists():
        dependencies = assist / 'settings_dependencies.h'
    out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix='console-display-entry-'))
    out.mkdir(parents=True, exist_ok=True)
    paths = [headers / 'display_settings.h', headers / 'video_settings_ui.h',
             suite / 'settings_categories.h', suite / 'modern_console_main.h',
             assist / 'main.cpp', assist / 'auto_settings_ui.h', assist / 'extra_settings_io.h',
             dependencies, assist / 'watch_scene.h', Path(__file__).resolve()]
    raw = {str(p): p.read_bytes() for p in paths}
    host = raw[str(assist / 'main.cpp')].decode()
    auto = raw[str(assist / 'auto_settings_ui.h')].decode()
    io = raw[str(assist / 'extra_settings_io.h')].decode()
    modern = raw[str(suite / 'modern_console_main.h')].decode()
    fragments = {
        'forward': block(host, 'if(msg==suite::ApplyDisplaySetting){'),
        'superweapon_owner': block(auto, 'if(id==9405&&notification==BN_CLICKED){'),
        'details_owner': block(host, 'if(id==481){'),
        'request': block(modern, 'bool requestDisplaySetting(int option,int value){'),
        'superweapon_load': statement(io, 'extra.superweaponPanel=value('),
        'superweapon_save': statement(io, 'write(L"Display",L"SuperweaponPanel",'),
        'details_load': statement(host, 'showDetails=value('),
        'details_save': statement(host, 'write(L"Display",L"DetailedStatus",'),
        'display_readback': block(host, 'for(int i=suite::RenderUpgrade;i<suite::DisplayOptionCount;++i){'),
        'motion_publisher_static_evidence': statement(host, 'profile.motion=renderHighRefresh;'),
        'motion_consumer_static_evidence': statement(raw[str(assist / 'watch_scene.h')].decode(), 'auto data=motion_.sampleLatest('),
    }
    prefix = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#include "display_settings.h"
#include "settings_categories.h"
#include "settings_dependencies.h"
using HWND=uintptr_t;using HANDLE=void*;using WPARAM=uintptr_t;using LPARAM=intptr_t;using INT_PTR=intptr_t;using BOOL=int;
constexpr int FALSE=0,TRUE=1,BST_CHECKED=1,BST_UNCHECKED=0;
constexpr int BN_CLICKED=0,EN_CHANGE=1,CBN_SELCHANGE=2,EN_KILLFOCUS=3,WM_COMMAND=100;
constexpr int BM_GETCHECK=10,BM_SETCHECK=11,HKM_GETHOTKEY=12,HKM_SETHOTKEY=13,CB_GETCURSEL=14,CB_SETCURSEL=15,CB_ADDSTRING=16,EM_SETLIMITTEXT=17;
constexpr int BS_AUTOCHECKBOX=1,WS_TABSTOP=2,WS_BORDER=4,ES_NUMBER=8,ES_AUTOHSCROLL=16,CBS_DROPDOWNLIST=32;
constexpr const wchar_t* HOTKEY_CLASSW=L"HOTKEY";
struct Control {int id=0,value=0;bool enabled=true;std::wstring text;};
static std::map<HWND,Control> controls;
static std::map<int,HWND> owners,visible;
static uintptr_t nextHandle=10;
static HWND ui=1,detailCheck=0;
static int LOWORD(WPARAM v){return int(v&65535);}
static int HIWORD(WPARAM v){return int((v>>16)&65535);}
static WPARAM MAKEWPARAM(int id,int notification){return WPARAM(id)|(WPARAM(notification)<<16);}
static HWND GetDlgItem(HWND,int id){return owners[id];}
static intptr_t SendMessageW(HWND h,int msg,WPARAM a,LPARAM){
 if(!h)return 0;auto&c=controls[h];
 if(msg==BM_GETCHECK||msg==HKM_GETHOTKEY||msg==CB_GETCURSEL)return c.value;
 if(msg==BM_SETCHECK||msg==HKM_SETHOTKEY||msg==CB_SETCURSEL)c.value=int(a);
 return 0;
}
static void SetWindowTextW(HWND h,const wchar_t*s){controls[h].text=s;}
static void GetWindowTextW(HWND h,wchar_t*out,int n){std::wcsncpy(out,controls[h].text.c_str(),size_t(n-1));out[n-1]=0;}
static int GetDlgItemInt(HWND,int id,BOOL*ok,int){const auto h=owners[id];*ok=h?TRUE:FALSE;return h?controls[h].value:0;}
static HWND GetFocus(){return 0;}
static void EnableWindow(HWND h,bool on){if(h)controls[h].enabled=on;}
static bool IsWindowEnabled(HWND h){return h&&controls[h].enabled;}
static void SetPropW(HWND,const wchar_t*,HANDLE){}
namespace modernui {static void heading(HWND){}static void pressKey(HWND){}static void tip(HWND,const wchar_t*){}}
static suite::DisplayState sharedDisplay;
static bool delivery=true;
struct Posted{int option,value;};static std::vector<Posted> posted;
namespace suite {
 constexpr int ApplyDisplaySetting=83;
 static bool readDisplaySettings(DisplayState&out){out=sharedDisplay;return true;}
 static struct Frontend{bool post(int module,int msg,int option,int value){if(!delivery||module!=2||msg!=ApplyDisplaySetting)return false;posted.push_back({option,value});return true;}} frontend;
 bool requestDisplaySetting(int,int);
}
#include "video_settings_ui.h"
static bool ready=true,updatingControls=false,dirty=false,showDetails=false;
static struct{bool superweaponPanel=false;}extra;
static unsigned changes=0,syncs=0,saves=0,notifications=0;
static std::map<std::wstring,int> profile;
static int value(const wchar_t*section,const wchar_t*key,int fallback){auto i=profile.find(std::wstring(section)+L"/"+key);return i==profile.end()?fallback:i->second;}
static void write(const wchar_t*section,const wchar_t*key,int val){profile[std::wstring(section)+L"/"+key]=val;}
static void changedSetting(){dirty=true;++changes;}
static void syncAdditional(){++syncs;}
struct ControlUpdate{bool was=updatingControls;ControlUpdate(){updatingControls=true;}~ControlUpdate(){updatingControls=was;}};
'''
    program = prefix + '\nnamespace suite {' + fragments['request'] + '}\n'
    program += 'static bool save(){++saves;' + fragments['superweapon_save'] + fragments['details_save'] + 'return true;}\n'
    program += 'static void reload(){' + fragments['superweapon_load'] + fragments['details_load'] + '}\n'
    program += 'static bool owner(int id,int notification){' + fragments['superweapon_owner'] + fragments['details_owner'] + 'return false;}\n'
    program += r'''
static int proc(HWND,int msg,WPARAM wp,LPARAM){
 if(msg==WM_COMMAND&&!updatingControls){++notifications;owner(LOWORD(wp),HIWORD(wp));}return 0;
}
'''
    program += 'static void publishSuiteDisplay(){suite::DisplayState state;' + fragments['display_readback'] + 'sharedDisplay=state;}\n'
    program += r'''
static int forward(HWND w,int msg,WPARAM wp,LPARAM lp){
''' + fragments['forward'] + '\nreturn -1;}\n'
    program += r'''
static unsigned checks=0,failed=0,scenarios=0;
static void check(bool v,const char*label){++checks;if(!v){++failed;std::printf("FAIL %s\n",label);}}
static int optionFor(int id){for(int n=0;n<suite::DisplayOptionCount;++n)if(suite::displaySpecs[n].id==id)return n;return -1;}
static HWND create(const wchar_t*,const wchar_t*text,int,int,int,int,int,int id){const HWND h=++nextHandle;controls[h]={id,0,true,text};if(id)visible[id]=h;return h;}
static void deliver(){auto work=posted;posted.clear();for(auto x:work)forward(ui,suite::ApplyDisplaySetting,x.option,x.value);}
int main(){
 for(int id:{9405,481}){auto h=++nextHandle;owners[id]=h;controls[h]={id,0,true,L""};}detailCheck=owners[481];
 ++scenarios;reload();check(!extra.superweaponPanel&&!showDetails,"unchanged empty-profile defaults");
 int panel=optionFor(9405),details=optionFor(481);
 ++scenarios;int id=9405;check(suite::categories::row(4,&id,1)==suite::categories::Video,"panel hidden from unified tactical page");
 id=481;check(suite::categories::row(3,&id,1)==suite::categories::Video,"details hidden from unified tactical page");
 check(panel>=0,"missing SuperweaponPanel display spec");check(details>=0,"missing DetailedStatus display spec");
 suite::VideoSettings video;video.create(create,suite::VideoGroup::Monitor,0,0,700);
 for(int native:{9405,481}){
  ++scenarios;const int option=optionFor(native);if(option<0)continue;
  const HWND control=visible[12000+option];check(control!=0,"visible monitor setting exists");if(!control)continue;
  check(controls[control].enabled,"setting independently enabled without RenderUpgrade");
  check(!controls[control].value,"visible fresh-profile switch off");
  const unsigned before=notifications;controls[control].value=1;
  check(video.command(MAKEWPARAM(12000+option,BN_CLICKED)),"visible click accepted by VideoSettings");
  check(posted.size()==1&&posted[0].option==option&&posted[0].value==1,"correct option posted to owner");
  deliver();check(notifications==before+1,"one owner notification for one toggle");
  check(native==9405?extra.superweaponPanel:showDetails,"owner model enabled");
  if(dirty){save();dirty=false;}check(value(L"Display",native==9405?L"SuperweaponPanel":L"DetailedStatus",0)==1,"enabled preference persisted");
  video.sync(true);check(controls[control].value==1,"owner state returned to visible control");
  extra.superweaponPanel=showDetails=false;reload();check(native==9405?extra.superweaponPanel:showDetails,"preference survives restart");
  controls[control].value=0;video.command(MAKEWPARAM(12000+option,BN_CLICKED));deliver();if(dirty){save();dirty=false;}
  check(!(native==9405?extra.superweaponPanel:showDetails),"owner model disabled");
  reload();check(!(native==9405?extra.superweaponPanel:showDetails),"off preference survives restart");
  ++scenarios;const unsigned old=notifications;ready=false;controls[control].value=1;video.command(MAKEWPARAM(12000+option,BN_CLICKED));deliver();ready=true;
  check(notifications==old,"not-ready Assist does not mutate owner");
  video.sync(true);check(controls[control].value==0,"rejected mutation returns authoritative off");
  check(!suite::requestDisplaySetting(option,-1)&&!suite::requestDisplaySetting(option,2),"boolean bounds enforced");
 }
 ++scenarios;check(!suite::requestDisplaySetting(-1,1)&&!suite::requestDisplaySetting(suite::DisplayOptionCount,1),"invalid display options rejected");
 if(panel>=0){delivery=false;check(!suite::requestDisplaySetting(panel,1),"unavailable owner rejects post");delivery=true;}
 ++scenarios;profile[L"Display/SuperweaponPanel"]=1;profile[L"Display/DetailedStatus"]=1;reload();check(extra.superweaponPanel&&showDetails,"existing enabled profiles retained");
 ++scenarios;assist::settingsdeps::State rendererOff;
 check(assist::settingsdeps::decision(9324,rendererOff).enabled(),"live camera motion owner is independent of unavailable renderer");
 check(!assist::settingsdeps::decision(9306,rendererOff).enabled()&&!assist::settingsdeps::decision(9313,rendererOff).enabled(),"retained render parameters keep parent dependency");
 sharedDisplay.values[suite::RenderHighRefresh]=1;suite::VideoSettings render;render.create(create,suite::VideoGroup::Render,0,0,700);
 auto motion=visible[12000+suite::RenderHighRefresh];check(motion!=0,"motion entry exists");
 check(IsWindowEnabled(motion),"live camera motion visible switch is independently enabled");
 check(!IsWindowEnabled(visible[12000+suite::RenderUpgrade])&&!IsWindowEnabled(visible[12000+suite::RenderPhysical]),"disabled renderer entry and physical setting stay closed");
 check(controls[motion].value==1,"existing camera motion preference retained");controls[motion].value=0;
 render.command(MAKEWPARAM(12000+suite::RenderHighRefresh,BN_CLICKED));
 check(posted.size()==1&&posted[0].option==suite::RenderHighRefresh&&posted[0].value==0,"motion switch reaches shared setting owner");posted.clear();
 sharedDisplay.values[suite::RenderHighRefresh]=0;render.sync(true);check(controls[motion].value==0,"camera motion authoritative off reflected");
 check(saves>0||failed>0,"roundtrip save actually exercised");
 std::printf("{\"passed\":%s,\"scenarios\":%u,\"checks\":%u,\"failed\":%u}\n",failed?"false":"true",scenarios,checks,failed);return failed?1:0;
}
'''
    cpp = out / 'production-entry.cpp'
    cpp.write_text(program)
    exe = out / 'production-entry'
    command = ['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Wno-unused-parameter',
               '-Wno-unused-function', '-Wno-misleading-indentation', '-I' + str(headers),
               '-I' + str(suite), '-I' + str(assist), str(cpp), '-o', str(exe)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (out / 'compile.log').write_text(compiled.stdout + compiled.stderr)
    report = {'schema': 1, 'scope': 'portable production VideoSettings and Assist owner path; Win32/game not executed',
              'sources': {str(p.relative_to(source)) if p.is_relative_to(source) else p.name: sha(raw[str(p)]) for p in paths},
              'extracted_source_sha256': {k: sha(v.encode()) for k, v in fragments.items()},
              'generated_cpp_sha256': sha(cpp.read_bytes()), 'compile_exit': compiled.returncode}
    if compiled.returncode:
        report['passed'] = False
        print(compiled.stderr)
    else:
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        (out / 'run.log').write_text(run.stdout + run.stderr)
        print(run.stdout, end='')
        report['execution'] = json.loads(run.stdout.splitlines()[-1])
        report['run_exit'] = run.returncode
        report['passed'] = run.returncode == 0
    if args.negative_control:
        fixtures = Path(__file__).resolve().parent / 'fixtures/entry_pre1475'
        actual = {name: sha((fixtures / name).read_bytes()) for name in PRE5_SHA256}
        if actual != PRE5_SHA256:
            report['negative_control'] = {'passed': False, 'fixture_sha256': actual,
                                          'error': 'pre.5 fixture byte hashes changed'}
            report['passed'] = False
        else:
            negative_out = out / 'pre5'
            negative = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--headers', str(fixtures),
                                       '--out', str(negative_out)], capture_output=True, text=True)
            (out / 'pre5-driver.log').write_text(negative.stdout + negative.stderr)
            negative_report = json.loads((negative_out / 'result.json').read_text())
            run_log = negative_out / 'run.log'
            failures = [line for line in run_log.read_text().splitlines() if line.startswith('FAIL ')] if run_log.exists() else []
            expected = (negative.returncode == 1 and negative_report.get('compile_exit') == 0 and
                        negative_report.get('run_exit') == 1 and failures == PRE5_FAILURES and
                        negative_report.get('execution') == {'passed': False, 'scenarios': 7, 'checks': 16, 'failed': 5})
            report['negative_control'] = {'passed': expected, 'provenance_commit': PRE5_SOURCE,
                                          'fixture_sha256': actual, 'driver_exit': negative.returncode,
                                          'expected_failures': PRE5_FAILURES, 'observed_failures': failures,
                                          'result': negative_report,
                                          'run_log_sha256': sha(run_log.read_bytes()) if run_log.exists() else None}
            report['passed'] = report['passed'] and expected
            print('pre.5 negative control: ' + ('expected five assertion failures observed' if expected else 'FAILED'))
    (out / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
