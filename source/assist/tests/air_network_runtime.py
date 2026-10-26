"""Compiled Runtime patch ownership for SP and two MP aircraft experiments.

Synthetic Win32/world; native capture fixture. Verifies installed addresses,
mode changes, UI gate result and rollback. No movement or netplay certification.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
for session in (0,5,3,4):
    m=fixture(session)
    original_type=bytes(m.u.mem_read(0x7e2908,4))
    original_arrival=bytes(m.u.mem_read(0x416ab6,10))
    for experiment in (0,1,2,1,0):
        s=state(m);s.flags=0x80000000;s.nativeAirNetwork=experiment;save(m,s);s=tick(m)
        frontend=session in (0,5) or experiment in (1,2)
        arrival=session in (0,5) or experiment==2
        assert (bytes(m.u.mem_read(0x7e2908,4))!=original_type)==frontend,(session,experiment,s.data.nativeAirStatus)
        assert (bytes(m.u.mem_read(0x416ab6,10))!=original_arrival)==arrival,(session,experiment,s.data.nativeAirStatus)
        expected=1 if session in (0,5) else 2 if experiment==0 else 3 if experiment==1 else 4
        assert s.data.nativeAirStatus==expected,(session,experiment,s.data.nativeAirStatus)
        if frontend:
            gate=m.r(0x7e2908);m.run(gate,(),m.t)
            assert m.u.reg_read(UC_X86_REG_EAX)==1
        assert count(m)==0,'changing the strategy must not submit an order'
        rows.append({'session':session,'experiment':experiment,'frontend_patch':frontend,'arrival_patch':arrival,'status':'PASS'})
    s=state(m);s.flags=0;save(m,s);s=tick(m)
    assert bytes(m.u.mem_read(0x7e2908,4))==original_type
    assert bytes(m.u.mem_read(0x416ab6,10))==original_arrival
    rows.append({'session':session,'case':'disable_restores_both_sites','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':SCOPE,'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
print('PASS',len(rows),'compiled native aircraft strategy/install/gate/restore cases')
