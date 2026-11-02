"""Execute the compiled Runtime message boundary, observing actual service entries.

Win32/world are mocked by the established Runtime harness. The real DLL's
GameRuntimeHook and AssistRuntimeFilter execute; no game process is opened.
This proves dispatch reachability/gating, not gesture execution or multiplayer.
"""
import argparse, hashlib, importlib.util, json, struct, traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from pdb_fixture_symbol import function_address

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True)
p.add_argument('--fixture-module',type=Path,default=Path(__file__).with_name('entry_runtime_regression.py'))
a=p.parse_args();spec=importlib.util.spec_from_file_location('dispatch_fixture',a.fixture_module)
m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
m.hook()
trace=[];pdb=Path(a.dll).with_suffix('.pdb')
def resolve(names):
    for name in names:
        try:return function_address(m.dll,pdb,name)
        except AssertionError:pass
    raise AssertionError(('Missing actual service symbol',names))
for label,names in [('Cursor',[b'_CursorRuntimeMessage@12',b'CursorRuntimeMessage']),
                    ('Tactical',[b'_AssistRuntimeMessage@12',b'AssistRuntimeMessage']),
                    ('Filter',[b'_AssistRuntimeFilter',b'AssistRuntimeFilter'])]:
    at=resolve(names)
    m.u.hook_add(UC_HOOK_CODE,lambda uc,pc,size,unused,label=label:trace.append(label),begin=at,end=at)
control=m.SH+0x700000;message=m.SH+0x700200
rows=[]
def check(name,fn):
    try:fn();rows.append(dict(case=name,status='PASS'))
    except Exception as ex:rows.append(dict(case=name,status='FAIL',error=repr(ex)));traceback.print_exc()
def dispatch(kind,button=0,enabled=1,ready=0,code=0,remove=1,has_message=True):
    trace.clear();m.w(control+16,enabled);m.w(control+20,ready)
    m.u.mem_write(message,struct.pack('<7I',0,kind,button,0,0,0,0))
    if has_message:
        m.run(m.entry,(code&0xffffffff,remove,message))
    else:
        # The shared helper deliberately expands (0,1,0) into a Host wake.
        # Use the raw ABI here so this case truly passes a null MSG pointer.
        sp=m.STACK+0x1e000
        m.u.mem_write(sp,struct.pack('<4I',m.STOP,code&0xffffffff,remove,0))
        m.u.reg_write(m.UC_X86_REG_ESP,sp);m.u.reg_write(m.UC_X86_REG_ECX,0)
        m.u.emu_start(m.entry,m.STOP,count=2000000)
        assert m.u.reg_read(m.UC_X86_REG_EIP)==m.STOP and m.u.reg_read(m.UC_X86_REG_ESP)==sp+16
    return trace[:]
def expected_filter(kind,button=0):
    actual=dispatch(kind,button)
    assert actual==['Filter'],actual
for kind in (0x200,0x100,0x101,0x104,0x105,0x201,0x202,0x203,0x204,0x205,0x206,0x207,0x208,0x209):
    check('route_'+hex(kind),lambda kind=kind:expected_filter(kind))
for button in (1,2):
    for kind in (0x20b,0x20c,0x20d):
        check('route_side_'+str(button)+'_'+hex(kind),lambda kind=kind,button=button:expected_filter(kind,button<<16))
for kind in (0,0x20,0x20a,0x20e,0xf):
    check('unrelated_'+hex(kind),lambda kind=kind:(_ for _ in ()).throw(AssertionError(trace[:])) if dispatch(kind) else None)
for enabled in (0,2):
    check('no_tactical_'+str(enabled),lambda enabled=enabled:(_ for _ in ()).throw(AssertionError(trace[:])) if dispatch(0x207,enabled=enabled) else None)
check('negative_hook_code',lambda:(_ for _ in ()).throw(AssertionError(trace[:])) if dispatch(0x207,code=-1) else None)
check('peek_without_remove',lambda:(_ for _ in ()).throw(AssertionError(trace[:])) if dispatch(0x207,remove=0) else None)
check('null_message',lambda:(_ for _ in ()).throw(AssertionError(trace[:])) if dispatch(0x207,has_message=False) else None)
def ordering():
    actual=dispatch(0x207,enabled=3,ready=3)
    assert actual==['Cursor','Tactical','Filter'],actual
check('cursor_before_tactical_before_filter',ordering)
result=dict(scope=__doc__,game_sha256=hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),
            dll_sha256=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
            checks=len(rows),passed=all(x['status']=='PASS' for x in rows),results=rows,
            game_tested=False,multiplayer_tested=False)
Path(a.output).write_text(json.dumps(result,indent=2)+'\n')
print(('PASS' if result['passed'] else 'FAIL')+' compiled Runtime dispatch '+str(len(rows))+' checks; middle/side edges, existing buttons/keys, gates and service order')
raise SystemExit(not result['passed'])
