"""Execute compiled selected-mode commands and original Event constructors.

Synthetic world and Win32. Checks serialization, session routing, stale input,
selection filters, repeated sets and Sticky isolation; events are not consumed
by a running game. Does not certify combat behavior or live multiplayer.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
def command(m,cmd,mutate=None,allow_pending=False):
    s=state(m);s.request+=1;s.command=cmd;s.requestMatch=s.data.match;s.requestTick=m.clock
    if mutate:mutate(s)
    save(m,s);s=tick(m)
    assert allow_pending or s.ack==s.request
    return s
def packet(m,n):return packets(m)[n*111:(n+1)*111]
for session in (0,5,3,4):
    for cmd,mission in ((29,0),(30,5),(31,23)):
        m=fixture(session);s=state(m);s.foreground=0;save(m,s)
        before=bytes(m.u.mem_read(m.unit,0x2000));s=command(m,cmd)
        assert s.result==1 and s.count==2 and count(m)==2
        assert [packet(m,n)[12] for n in range(2)]==[mission,mission]
        assert bytes(m.u.mem_read(m.unit,0x2000))==before and not m.codeWrites
        rows.append({'session':session,'case':'explicit_tool_button_without_game_foreground','mission':mission,'status':'PASS'})
    for cmd,mission in ((29,0),(30,5),(31,23)):
        m=fixture(session);before=[bytes(m.u.mem_read(p,0x2000)) for p in (m.unit,m.eng)]
        s=command(m,cmd)
        assert s.result==1 and s.count==2 and count(m)==2,(session,cmd,s.result,s.count,count(m))
        for n,ident in enumerate((10,11)):
            b=packet(m,n)
            assert b[0]==4 and b[2]==0 and b[7:12]==struct.pack('<IB',ident,0x34) and b[12]==mission
            assert b[13:30]==bytes(17),'idle modes carry no target, destination or planning token'
        assert [bytes(m.u.mem_read(p,0x2000)) for p in (m.unit,m.eng)]==before
        assert not m.codeWrites
        other=0xa802c8 if session in (0,5) else 0x8b41f8
        assert m.r(other)==0,'only the matching session queue may change'
        s=command(m,cmd)
        assert s.count==2 and count(m)==4 and [packet(m,n)[12] for n in range(4)]==[mission]*4
        for _ in range(3):tick(m)
        assert count(m)==4,'one-shot command must not reassert the mode'
        rows.append({'session':session,'case':'repeat_set_and_no_local_execution','mission':mission,'status':'PASS'})
    m=fixture(session)
    for cmd in (29,31,30):assert command(m,cmd).count==2
    assert [packet(m,n)[12] for n in range(6)]==[0,0,23,23,5,5]
    rows.append({'session':session,'case':'sleep_harmless_guard_order','status':'PASS'})
    for case in ('foreign','dead','loaded','temporal','deactivated','building','aircraft','not_in_world','sticky_current','sticky_queued','empty'):
        m=fixture(session);m.vector(0xa8ecb8,[m.unit])
        if case=='foreign':m.w(m.unit+0x21c,m.enemy)
        elif case=='dead':m.w(m.unit+0x6c,0)
        elif case=='loaded':m.w(m.unit+0x11c,m.eng)
        elif case=='temporal':m.w(m.unit+0x2b0,m.eng)
        elif case=='deactivated':m.b(m.unit+0x1c8,1)
        elif case=='building':m.vector(0xa8ecb8,[m.building])
        elif case=='aircraft':m.w(m.unit,0x7e22a4)
        elif case=='not_in_world':m.vector(0xa8ec78,[m.eng,m.building,m.enemyUnit])
        elif case=='sticky_current':m.w(m.unit+0xac,6)
        elif case=='sticky_queued':m.w(m.unit+0xb4,6)
        elif case=='empty':m.vector(0xa8ecb8,[])
        before=bytes(m.u.mem_read(m.unit,0x2000));s=command(m,29)
        assert s.count==0 and count(m)==0 and not m.codeWrites,(session,case,s.result,s.count)
        assert bytes(m.u.mem_read(m.unit,0x2000))==before
        assert C.c_int32(s.result).value==(-41 if case.startswith('sticky_') else 1)
        rows.append({'session':session,'case':case,'status':'PASS'})
    for case in ('full_queue','stale_match','expired','chat','unsupported'):
        m=fixture(session);prior=0
        if case=='full_queue':
            at=0x8b41f8 if session in (0,5) else 0xa802c8
            prior=0x4000 if session in (0,5) else 112;m.w(at,prior)
        elif case=='chat':m.b(0xa8bc60+25,1)
        elif case=='unsupported':m.w(0xa8b238,2)
        def mutate(s):
            if case=='stale_match':s.requestMatch+=1
            elif case=='expired':s.requestTick=m.clock-3501
        s=command(m,31,mutate,allow_pending=case=='unsupported')
        if case=='unsupported':
            assert s.ack!=s.request and s.data.commandStatus==0 and count(m)==0
        else:
            assert s.count==0 and count(m)==prior and C.c_int32(s.result).value<0,(session,case,s.result,s.count,count(m))
        assert not m.codeWrites
        rows.append({'session':session,'case':case,'status':'PASS'})
    m=fixture(session);queue=0x8b41f8 if session in (0,5) else 0xa802c8
    prior=0x3fff if session in (0,5) else 111;m.w(queue,prior)
    s=command(m,30)
    assert s.result==1 and s.count==1 and count(m)==prior+1
    assert not m.codeWrites
    rows.append({'session':session,'case':'last_queue_slot_reports_partial_acceptance','status':'PASS'})
    # A Sticky owned by the SP helper must never be released by cancelNative.
    m=fixture(session);m.vector(0xa8ecb8,[m.unit]);command(m,28)
    before=bytes(m.u.mem_read(m.unit,0x2000));prior=count(m);s=command(m,30)
    assert s.count==0 and C.c_int32(s.result).value==-41 and count(m)==prior
    assert bytes(m.u.mem_read(m.unit,0x2000))==before and not m.codeWrites
    rows.append({'session':session,'case':'tool_owned_sticky_preserved','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':SCOPE,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
print('PASS',len(rows),'compiled selected-mode routing/filter/serialization/isolation cases')
