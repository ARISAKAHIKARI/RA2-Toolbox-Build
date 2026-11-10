#!/usr/bin/env python3
"""Actual formation capture/reset through the central read-only input boundary.

The full production formation bridge, its real reset, and exact central filter
functions execute with the existing controlled world/Win32 fixture. Unrelated
input owners are inactive seams. No Windows/game execution is claimed. The
optional negative control is a verified development intermediate, not pre.5.
"""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import subprocess

FIXTURE = Path(__file__).resolve().parent / 'fixtures/read_only_pre_pairing/native_message_policy.h'
FIXTURE_SHA = '959b707cedaca064e1b3c44030b152da14d697035093e1e0007d22bd07bf8569'
BALANCE_FAILURE = 'FAIL read-only release balances formation down after real reset'
EXPECTED = ([BALANCE_FAILURE] * 18 + ['FAIL readonly autorepeat preserves original release ownership']
            + ['FAIL original release remains balanced after acknowledged resume'] * 18)

EXTRA = r'''
static readonly::RuntimeState readOnlyState;
static bool pauseViewKeySuppressed[256]{};
static uint32_t blockedButtons;
static pointerinput::SuppressedClick blockedOrigins[2];
static bool pauseViewMessage(MSG*){return false;}
static bool nativePlacementCompletionPending(){return false;}
static void nativePlacementCompletionTick(uint32_t){}
static bool nativePlacementMessage(MSG*){return false;}
static bool moOrdersMessage(MSG*){return false;}
namespace directair {static bool message(MSG*){return false;}}
static bool reservationMessage(MSG*){return false;}
static bool nativeReady(){return true;}
static bool cooperativeSingleInput(uint32_t){return false;}
static bool nativeEnemy(uint32_t,uint32_t){return false;}
static bool rawArmed(uint32_t){return false;}
static bool rawMagnetron(uint32_t){return false;}
static bool csAllowedFor(uint32_t,bool){return false;}
static bool featureObjectAllowed(int,uint32_t){return false;}
namespace featuretypes{enum{AdvancedAttack};}
constexpr uint32_t AdvancedInput=1,MagnetronInput=2;
'''

CASES = r'''
static int scenarios=0,checks=0,failures=0;
static void check(bool value,const char*name){++checks;if(!value){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
static unsigned route(UINT message,WPARAM key=0,bool repeat=false){
 MSG m;m.message=message;m.wParam=key;
 m.lParam=message>=WM_MOUSEMOVE?MAKELPARAM(100,100):(repeat?LPARAM(1u<<30):0);
 AssistFilterMessage(&m);return m.message;
}
static void readonlyGate(int gate){
 if(gate==0)sh->readOnly=1;if(gate==1)sh->readOnlyRequest=1;if(gate==2)readOnlyState.active=true;
}
struct Mouse{unsigned physical,down,up,twice;WPARAM parameter;};
static const Mouse mice[]={
 {1,WM_MBUTTONDOWN,WM_MBUTTONUP,WM_MBUTTONDBLCLK,0},
 {6,WM_LBUTTONDOWN,WM_LBUTTONUP,WM_LBUTTONDBLCLK,0},
 {4,WM_RBUTTONDOWN,WM_RBUTTONUP,WM_RBUTTONDBLCLK,0},
 {2,WM_XBUTTONDOWN,WM_XBUTTONUP,WM_XBUTTONDBLCLK,WPARAM(1)<<16},
 {3,WM_XBUTTONDOWN,WM_XBUTTONUP,WM_XBUTTONDBLCLK,WPARAM(2)<<16}};
int main(){
 for(auto mouse:mice)for(int gate=0;gate<3;++gate){
  ++scenarios;setup();readOnlyState={};sh->moGestures.buttons[3]=mouse.physical;
  check(route(mouse.down,mouse.parameter)==WM_NULL&&formationQuick&&sh->formationPreview.active&&sh->formationPreview.count==25,
        "real formation handler acquires mouse down and publishes 25-unit preview");
  readonlyGate(gate);resetFormation();
  check(!formationQuick&&!formationQuickCaptured&&!formationButton&&!sh->formationPreview.active,
        "real formation reset retires the feature state");
  check(route(mouse.up,mouse.parameter)==WM_NULL,"read-only release balances formation down after real reset");
  check(commands.empty()&&!formationJobCount,"read-only release does not commit formation orders");
  check(route(mouse.down,mouse.parameter)==mouse.down&&route(mouse.up,mouse.parameter)==mouse.up,
        "fresh read-only mouse pair remains native");
 }
 for(int gate=0;gate<3;++gate){
  ++scenarios;setup();readOnlyState={};held[VK_MENU]=true;
  check(route(WM_SYSKEYDOWN,VK_MENU)==WM_NULL&&formationHolding&&formationSuppressed[VK_MENU]&&sh->formationPreview.count==25,
        "real traditional formation acquires configured hold key");
  readonlyGate(gate);resetFormation();
  check(!formationHolding&&!formationSuppressed[VK_MENU],"real reset clears feature key ownership");
  check(route(WM_SYSKEYUP,VK_MENU)==WM_NULL,"read-only release balances formation down after real reset");
  check(commands.empty()&&!formationJobCount,"read-only key release does not commit formation orders");
  check(route(WM_SYSKEYDOWN,VK_MENU)==WM_SYSKEYDOWN&&route(WM_SYSKEYUP,VK_MENU)==WM_SYSKEYUP,
        "fresh read-only system-key pair remains native");
 }
 ++scenarios;setup();readOnlyState={};
 check(route(WM_MBUTTONDOWN)==WM_NULL&&formationQuick,"ordinary command admission still enters formation capture");
 check(route(WM_MBUTTONUP)==WM_NULL&&formationJobCount==25,"ordinary release still commits its captured 25-unit formation");
 check(route(WM_MBUTTONUP)==WM_MBUTTONUP,"unmatched ordinary release remains native");
 ++scenarios;setup();readOnlyState={};
 check(route(WM_XBUTTONDOWN,WPARAM(3)<<16)==WM_XBUTTONDOWN&&route(WM_XBUTTONUP,WPARAM(3)<<16)==WM_XBUTTONUP,
       "unknown X-button does not acquire physical ownership");
 for(auto mouse:mice){
  ++scenarios;setup();readOnlyState={};sh->moGestures.buttons[3]=mouse.physical;
  check(route(mouse.down,mouse.parameter)==WM_NULL&&formationQuick,"real owner exists before a release is lost outside the route");
  readonlyGate(0);resetFormation();
  check(route(mouse.twice,mouse.parameter)==mouse.twice&&route(mouse.up,mouse.parameter)==mouse.up,
        "fresh read-only double-click retires lost release ownership");
  check(commands.empty()&&!formationJobCount,"fresh read-only double-click cannot commit formation orders");
 }
 ++scenarios;setup();readOnlyState={};held[VK_MENU]=true;
 check(route(WM_SYSKEYDOWN,VK_MENU)==WM_NULL&&formationHolding,"real hold key ownership exists before repeat");
 readonlyGate(0);resetFormation();
 check(route(WM_SYSKEYDOWN,VK_MENU,true)==WM_SYSKEYDOWN,"read-only repeat does not enter a feature owner");
 check(route(WM_SYSKEYUP,VK_MENU)==WM_NULL,"readonly autorepeat preserves original release ownership");
 check(commands.empty()&&!formationJobCount,"read-only repeat and release do not commit formation orders");
 // No central input message occurs while commands are closed. Feature reset
 // completes first; the old UP arrives only after the request has been ACKed.
 for(auto mouse:mice)for(int gate=0;gate<3;++gate){
  ++scenarios;setup();readOnlyState={};sh->moGestures.buttons[3]=mouse.physical;
  check(route(mouse.down,mouse.parameter)==WM_NULL&&formationQuick,"real mouse ownership before rapid read-only and resume");
  readonlyGate(gate);resetFormation();
  sh->readOnly=0;sh->readOnlyRequest=sh->readOnlyAck=1;readOnlyState={};
  check(route(mouse.up,mouse.parameter)==WM_NULL,"original release remains balanced after acknowledged resume");
  check(commands.empty()&&!formationJobCount,"resumed old release does not commit formation orders");
  check(route(mouse.down,mouse.parameter)==WM_NULL&&route(mouse.up,mouse.parameter)==WM_NULL&&formationJobCount==25,
        "a new pair after resume still performs the configured formation action");
 }
 for(int gate=0;gate<3;++gate){
  ++scenarios;setup();readOnlyState={};held[VK_MENU]=true;
  check(route(WM_SYSKEYDOWN,VK_MENU)==WM_NULL&&formationHolding,"real hold ownership before rapid read-only and resume");
  readonlyGate(gate);resetFormation();
  sh->readOnly=0;sh->readOnlyRequest=sh->readOnlyAck=1;readOnlyState={};
  check(route(WM_SYSKEYUP,VK_MENU)==WM_NULL,"original release remains balanced after acknowledged resume");
  check(commands.empty()&&!formationJobCount,"resumed old hold release does not commit formation orders");
  check(route(WM_SYSKEYDOWN,VK_MENU)==WM_NULL&&formationHolding&&route(WM_SYSKEYUP,VK_MENU)==WM_NULL,
        "a new hold pair after resume still performs the configured formation action");
 }
 ++scenarios;setup();readOnlyState={};
 check(route(WM_MBUTTONDOWN)==WM_NULL&&formationQuick,"real capture before a lost release and resumed rebind");
 sh->readOnly=1;resetFormation();sh->readOnly=0;sh->readOnlyRequest=sh->readOnlyAck=1;
 sh->formationPolicy.enabled=0;sh->moFeatures=0;
 check(route(WM_MBUTTONDOWN)==WM_MBUTTONDOWN&&route(WM_MBUTTONUP)==WM_MBUTTONUP,
       "new native down after resume retires an old lost release");
 check(commands.empty()&&!formationJobCount,"resumed fresh native pair does not enter the disabled owner");
 std::cout<<"{\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}
'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def function(text, signature):
    start = text.index(signature)
    at = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[at] == '{') - (text[at] == '}')
        at += 1
    return text[start:at]


def run(root, out, old_header=None):
    out.mkdir(parents=True, exist_ok=True)
    snapshot = out / 'snapshot'
    sources = {}

    def freeze(path):
        path = path.resolve()
        relative = str(path.relative_to(root))
        if relative in sources:
            return
        data = path.read_bytes()
        sources[relative] = sha(data)
        target = snapshot / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        for name in re.findall(rb'^\s*#\s*include\s*"([^"]+)"', data, re.M):
            child = path.parent / name.decode()
            if child.is_file():
                freeze(child)

    for name in ('formation.h', 'formation_bridge.h', 'mo_orders.h', 'pointer_intent.h', 'read_only_lifecycle.h'):
        freeze(root / 'source/assist/source' / name)
    helper = root / 'source/assist/tests/formation_quick_bridge_test.py'
    helper_data = helper.read_bytes()
    sources[str(helper.relative_to(root))] = sha(helper_data)
    (out / 'formation_fixture.source').write_bytes(helper_data)
    tree = ast.parse(helper_data)
    fixture = next(ast.literal_eval(node.value) for node in tree.body if isinstance(node, ast.Assign)
                   and any(isinstance(target, ast.Name) and target.id == 'fixture' for target in node.targets))
    fixture = fixture[:fixture.index('int main(){')]
    fixture = fixture.replace('@LAYOUT_HEADER@', '"formation.h"')
    fixture = fixture.replace('#include "mo_orders.h"', '#include "mo_orders.h"\n#include "pointer_intent.h"\n#include "read_only_lifecycle.h"')
    fixture = fixture.replace('namespace pointerinput{enum{AirWaypoint=1};struct Intent{int width=0,height=0,cameraX=0,cameraY=0,operation=0,x=0,y=0;};}', '')
    fixture = fixture.replace('bool stop=false,foreground=true;uint32_t heartbeat=1000;', '''bool stop=false,foreground=true;uint32_t heartbeat=1000;
 uint32_t readOnly=0,readOnlyRequest=0,readOnlyAck=0,pointerPolicySeq=0,flags=0;
 pointerinput::NativePolicy pointerPolicy{};pointerinput::Mailbox pointer{};pointerinput::SuppressedClick suppressedClick{};''')
    fixture = fixture.replace('constexpr int VK_MBUTTON=4,', '''constexpr unsigned WM_LBUTTONDBLCLK=0x203,WM_RBUTTONDBLCLK=0x206,WM_MBUTTONDBLCLK=0x209,WM_XBUTTONDOWN=0x20b,WM_XBUTTONUP=0x20c,WM_XBUTTONDBLCLK=0x20d;
constexpr int VK_LBUTTON=1,VK_RBUTTON=2,VK_XBUTTON1=5,VK_XBUTTON2=6,XBUTTON1=1,XBUTTON2=2;
constexpr int VK_MBUTTON=4,''')
    policy = root / 'source/assist/source/native_message_policy.h'
    policy_data = old_header if old_header is not None else policy.read_bytes()
    sources[str(policy.relative_to(root))] = sha(policy_data)
    (out / 'native_message_policy.source').write_bytes(policy_data)
    policy_text = policy_data.decode().replace('\r\n', '\n')
    bodies = [function(policy_text, 'static void recordRuntimeGesture('), function(policy_text, 'extern "C" int AssistFilterMessage(')]
    generated = fixture + '\n' + EXTRA + '\n' + '\n'.join(bodies) + '\n' + CASES
    cpp, binary = out / 'production.cpp', out / 'production'
    cpp.write_text(generated)
    command = ['g++', '-std=c++17', '-O1', '-I' + str(snapshot / 'source/assist/source'), str(cpp), '-o', str(binary)]
    compiled = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / 'compile.log').write_text(compiled.stdout)
    result = {'scope': __doc__, 'sources': sources,
              'extracted_source_sha256': [sha(body.encode()) for body in bodies],
              'generated_cpp_sha256': sha(generated.encode()), 'compile_argv': command, 'compile_exit': compiled.returncode}
    if compiled.returncode == 0:
        ran = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / 'run.log').write_text(ran.stdout)
        result.update(run_exit=ran.returncode, execution=json.loads(ran.stdout.splitlines()[-1]), raw_run_sha256=sha(ran.stdout.encode()))
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', '--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--negative-control', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    current = run(root, args.out / 'current')
    report = {'schema': 1, 'suite': 'native_readonly_pairing', 'scope': __doc__, 'current': current, 'sources': dict(current['sources'])}
    execution = current.get('execution', {})
    report['passed'] = current['compile_exit'] == 0 and current.get('run_exit') == 0 and execution == {'scenarios': 45, 'checks': 188, 'failures': 0}
    if args.negative_control:
        data = FIXTURE.read_bytes()
        if sha(data) != FIXTURE_SHA:
            raise SystemExit('Intermediate pre-ledger fixture SHA-256 mismatch')
        negative = run(root, args.out / 'negative-control', data)
        log = (args.out / 'negative-control/run.log').read_text() if negative['compile_exit'] == 0 else ''
        observed = [line for line in log.splitlines() if line.startswith('FAIL ')]
        ok = negative['compile_exit'] == 0 and negative.get('run_exit') == 1 and negative.get('execution') == {'scenarios': 45, 'checks': 188, 'failures': len(EXPECTED)} and observed == EXPECTED
        report['negative_control'] = {'passed': ok, 'fixture_sha256': FIXTURE_SHA,
            'fixture_provenance': 'Development intermediate after the first pause/generic-only correction and before the common pair ledger, 2026-11-09; not a released pre.5 source.',
            'expected_failures': EXPECTED, 'observed_failures': observed, 'result': negative}
        report['sources'][str(FIXTURE.relative_to(root))] = FIXTURE_SHA
        report['passed'] = report['passed'] and ok
    script = Path(__file__).resolve()
    report['sources'][str(script.relative_to(root))] = sha(script.read_bytes())
    for relative, digest in report['sources'].items():
        if sha((root / relative).read_bytes()) != digest:
            raise RuntimeError('Source changed during pairing test: ' + relative)
    (args.out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'suite': report['suite'], 'passed': report['passed'], **execution,
                      'negative_control': report.get('negative_control', {}).get('passed')}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
