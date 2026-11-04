"""Compiled Destruct command, original NoOptions constructor and partial receiver.

Synthetic world/Win32. Queue tests execute the actual DLL and native 4C66C0;
separate receiver tests consume its packet through 4C6CB0 and complete 4FC980.
MP permits only the current House: foreign requests must fail before constructing
anything, because the native network packer replaces Event.House with the sender.
No full DoList scheduler, House AI/DestroyAll, damage, victory, observer UI or
remote transport runs. Queued does not mean destroyed or network accepted.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, struct, sys, traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import *

p=argparse.ArgumentParser()
p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True)
p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('destruct_support',Path(__file__).with_name('no_interception_runtime.py'))
n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helpers.json'))]
spec.loader.exec_module(n);sys.argv=args
COMMAND=32

def check(name,fn):
    if a.filter and a.filter not in name:return
    try:
        fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
    except Exception as error:
        rows.append({'case':name,'status':'FAIL','error':repr(error)});traceback.print_exc()

def setup(mode):
    m=n.fixture(mode);m.vector(0xa8da74,[])
    # These synthetic House IDs/types are independent of Techno IDs/types.
    for house,ident in ((m.house,100),(m.enemy,200)):
        typ=m.alloc(0x2000);m.w(house+0x34,typ);m.w(house+0x10,ident)
        m.u.mem_write(typ+0x24,b'Americans\0')
    m.enemyType=m.r(m.enemy+0x34);m.target=m.enemy
    n.tick(m);m.ctorCalls=[];m.simWrites=[]
    def ctor(uc,at,size,_):
        sp=uc.reg_read(UC_X86_REG_ESP)
        m.ctorCalls.append((uc.reg_read(UC_X86_REG_ECX),m.r(sp+4),m.r(sp+8)))
    m.u.hook_add(UC_HOOK_CODE,ctor,begin=0x4c66c0,end=0x4c66c0)
    def write(uc,access,at,size,value,_):m.simWrites.append((at,size,value))
    for start,size in ((m.house,0x20000),(m.enemy,0x20000),(m.unit,0x2000),
                       (m.eng,0x2000),(m.building,0x2000),(m.enemyUnit,0x2000),(0xa83d4c,4)):
        m.u.hook_add(UC_HOOK_MEM_WRITE,write,begin=start,end=start+size-1)
    return m

def prepare(m,mutate=None):
    s=n.state(m);s.request+=1;s.command=COMMAND;s.arg=m.target
    s.arg2=m.r(m.target+0x10);s.requestToken=m.r(m.target+0x30)
    s.requestMatch=s.data.match;s.requestTick=m.clock
    if mutate:mutate(s)
    n.save(m,s);return s

def issue(m,mutate=None):
    prepare(m,mutate);return n.tick(m)

def snapshot(m):
    return {p:bytes(m.u.mem_read(p,size)) for p,size in
            ((m.house,0x20000),(m.enemy,0x20000),(m.unit,0x2000),(m.eng,0x2000),
             (m.building,0x2000),(m.enemyUnit,0x2000),(0xa83d4c,4))}

def unchanged(m,before):
    assert all(bytes(m.u.mem_read(p,len(data)))==data for p,data in before.items()),'simulation memory changed'
    assert not m.simWrites,m.simWrites
    assert not m.codeWrites,m.codeWrites

def packet(m,mode,index=0):
    base=0x8b4204 if mode in (0,5) else 0xa802d4
    return bytes(m.u.mem_read(base+111*index,111))

def accepted(m,s,mode,expected=1,house=1):
    assert s.ack==s.request and s.result==1 and s.count==1,(s.ack,s.request,s.result,s.count)
    assert n.count(m)==expected,n.count(m)
    b=packet(m,mode,expected-1)
    assert b[:3]==bytes((8,0,house)) and b[7:]==bytes(104),b.hex()
    assert struct.unpack_from('<I',b,3)[0]==m.r(0xa8ed84)+int(mode in (0,5)),b[:7].hex()
    assert m.r(0xa802c8 if mode in (0,5) else 0x8b41f8)==0
    assert m.ctorCalls and m.ctorCalls[-1][1:]==(house,8),m.ctorCalls

CASES=('enemy','own_index_zero','allied','tool_foreground','repeat_request',
       'null_pointer','missing_house','stale_id','wrong_index','negative_index','oversized_index',
       'reordered_array','neutral','special','passive','observer','observer_node',
       'defeated','game_over','winner','loser','stale_match','expired','chat','queue_full','bad_tail',
       'constructor_mismatch','already_acked')
for mode in (0,5,3,4):
    for case in CASES:
        def queue_case(mode=mode,case=case):
            m=setup(mode);queue=0x8b41f8 if mode in (0,5) else 0xa802c8
            initial=0;expected=case in ('enemy','own_index_zero','allied','tool_foreground','repeat_request')
            if mode in (3,4) and case in ('enemy','allied'):expected=False
            if mode in (3,4) and case in ('tool_foreground','repeat_request','queue_full','bad_tail','constructor_mismatch','already_acked'):m.target=m.house
            if case=='own_index_zero':m.target=m.house
            if case=='allied':m.w(m.house+0x5788,3)
            if case=='tool_foreground':s=n.state(m);s.foreground=0;n.save(m,s)
            if case=='missing_house':m.vector(0xa80228,[m.house])
            if case=='negative_index':m.w(m.enemy+0x30,0xffffffff)
            if case=='oversized_index':m.w(m.enemy+0x30,32)
            if case=='reordered_array':m.vector(0xa80228,[m.enemy,m.house])
            if case=='neutral':m.u.mem_write(m.enemyType+0x24,b'Neutral\0')
            if case=='special':m.u.mem_write(m.enemyType+0x24,b'Special\0')
            if case=='passive':m.b(m.enemyType+0x1a6,1)
            if case=='observer':m.w(0xac1198,m.enemy)
            if case=='observer_node':
                node=m.alloc();m.w(node+0x6b,0xffffffff);m.w(node+0x6f,1);m.vector(0xa8da74,[node])
            if case in ('defeated','game_over','winner','loser'):m.b(m.enemy+{'defeated':0x1f5,'game_over':0x1f6,'winner':0x1f7,'loser':0x1f8}[case],1)
            if case=='chat':m.b(0xa8bc60+25,1)
            if case=='queue_full':initial=16384 if mode in (0,5) else 112;m.w(queue,initial)
            if case=='bad_tail':m.w(queue+8,0xffffffff if mode in (0,5) else 128)
            if case=='constructor_mismatch':m.u.mem_write(0x4c66c0,b'\x90')
            def mutate(s):
                if case=='null_pointer':s.arg=0
                if case=='stale_id':s.arg2+=1
                if case=='wrong_index':s.requestToken=0
                if case=='stale_match':s.requestMatch+=1
                if case=='expired':s.requestTick=(m.clock-3501)&0xffffffff
                if case=='already_acked':s.ack=s.request
            before=snapshot(m);s=issue(m,mutate)
            if expected:
                accepted(m,s,mode,house=m.r(m.target+0x30))
                assert len(m.ctorCalls)==1
            else:
                assert n.count(m)==initial,(case,n.count(m),s.result,s.count)
                assert not m.ctorCalls,m.ctorCalls
                if case!='already_acked':
                    assert s.ack==s.request and C.c_int32(s.result).value<0 and s.count==0,(case,s.ack,s.request,s.result,s.count)
                    if case in ('stale_id','wrong_index','reordered_array','stale_match','expired'):assert C.c_int32(s.result).value==-7
                    if case=='chat':assert C.c_int32(s.result).value==-8
                    if case=='constructor_mismatch':assert C.c_int32(s.result).value==-12
                    if case in ('queue_full','bad_tail'):assert C.c_int32(s.result).value==-6
                    if mode in (3,4) and case in ('enemy','allied'):
                        assert C.c_int32(s.result).value==-42 and s.data.lastSubmitError==42
            for _ in range(3):n.tick(m)
            assert n.count(m)==initial+int(expected),'same mailbox request repeated'
            if case=='queue_full':
                # Commands acknowledge a failed one-shot; freeing room alone
                # must not replay a destructive action in a later frame.
                m.w(queue,0);n.tick(m);assert n.count(m)==0
                s=issue(m);accepted(m,s,mode,house=m.r(m.target+0x30));n.tick(m);assert n.count(m)==1
            unchanged(m,before)
            if case!='constructor_mismatch':assert bytes(m.u.mem_read(m.textAt,m.textSize))==m.textBefore
        check(f'queue_{mode}_{case}',queue_case)

# An engine suspension retains a mailbox request, but never submits during it.
# Resume must revalidate age, match and target identity before a single append.
for mode in (0,5,3,4):
    for case in ('paused','menu','dialog','inactive_tactical','unsupported_session',
                 'expired_while_paused','id_changed_while_paused','target_removed_while_paused','new_match'):
        def pending_case(mode=mode,case=case):
            m=setup(mode)
            if mode in (3,4):m.target=m.house
            prepare(m);before=snapshot(m)
            if case=='menu':m.w(0xa8eda0,1)
            elif case=='dialog':m.w(0xa8ed8c,1)
            elif case=='inactive_tactical':m.b(0xa8e378,0)
            elif case=='unsupported_session':m.w(0xa8b238,2)
            else:m.b(0xa8e9a0,0)
            s=n.tick(m)
            assert s.ack!=s.request and not m.ctorCalls and m.r(0xa802c8)==m.r(0x8b41f8)==0
            assert s.data.commandStatus==0
            if case=='expired_while_paused':m.clock+=4000
            if case=='id_changed_while_paused':m.w(m.target+0x10,m.r(m.target+0x10)+1);before=snapshot(m)
            if case=='target_removed_while_paused':m.vector(0xa80228,[m.enemy] if m.target==m.house else [m.house])
            m.w(0xa8eda0,0);m.w(0xa8ed8c,0);m.b(0xa8e378,1);m.b(0xa8e9a0,1);m.w(0xa8b238,mode)
            if case=='new_match':m.w(0xa8ed84,1)
            s=n.tick(m)
            if case in ('expired_while_paused','id_changed_while_paused','target_removed_while_paused','new_match'):
                assert s.ack==s.request and C.c_int32(s.result).value<0 and s.count==0
                assert not m.ctorCalls and n.count(m)==0
            else:accepted(m,s,mode,house=m.r(m.target+0x30))
            sent=n.count(m)
            for _ in range(3):n.tick(m)
            assert n.count(m)==sent
            unchanged(m,before)
        check(f'pending_{mode}_{case}',pending_case)

# The native send clock is an external call. Target or request replacement at
# that boundary must not publish an index-only packet for a different House.
for mode in (0,3):
    for case in ('target_id','array_slot','target_index','target_winner','current_player','request_match',
                 'request_token','request_expired','request_ack','request_command'):
        def timing_case(mode=mode,case=case):
            m=setup(mode)
            if mode in (3,4):m.target=m.house
            before=snapshot(m);injected=[]
            def inject():
                assert not injected;injected.append(case)
                if case=='target_id':m.w(m.target+0x10,m.r(m.target+0x10)+1)
                elif case=='array_slot':m.w(m.r(0xa8022c)+4*m.r(m.target+0x30),m.enemy if m.target==m.house else m.house)
                elif case=='target_index':m.w(m.target+0x30,1-m.r(m.target+0x30))
                elif case=='target_winner':m.b(m.target+0x1f7,1)
                elif case=='current_player':m.w(0xa83d4c,m.enemy)
                else:
                    s=n.state(m)
                    if case=='request_match':s.requestMatch+=1
                    elif case=='request_token':s.requestToken=1-s.requestToken
                    elif case=='request_expired':s.requestTick=(m.clock-3501)&0xffffffff
                    elif case=='request_ack':s.ack=s.request
                    elif case=='request_command':s.command=0
                    n.save(m,s)
                before.update(snapshot(m)) # expected fixture-owned mutation
            m.timer_side_effect=inject;s=issue(m);m.timer_side_effect=None
            assert injected==[case] and len(m.ctorCalls)==1
            assert s.ack==s.request and C.c_int32(s.result).value==-6 and s.count==0
            assert n.count(m)==0
            if case!='current_player':assert s.data.lastSubmitError==1
            for _ in range(3):n.tick(m)
            assert n.count(m)==0
            unchanged(m,before)
        check(f'timing_boundary_{mode}_{case}',timing_case)

# Native receiver consumes the final DLL's event. End flags are changed only
# here, by the original 4FC980, after all sender-only no-write checks passed.
for mode in (0,3):
    for target in (0,1):
        if mode==3 and target!=0:continue # Runtime deliberately forbids foreign MP construction.
        for state,offset in (('active',None),('game_over',0x1f6),('winner',0x1f7),('loser',0x1f8)):
            def receiver(mode=mode,target=target,state=state,offset=offset):
                m=setup(mode);m.target=m.house if target==0 else m.enemy
                s=issue(m);accepted(m,s,mode,house=target)
                event=m.alloc();m.u.mem_write(event,packet(m,mode));m.w(0xac1198,m.house)
                if offset:m.b(m.target+offset,1)
                before=snapshot(m);calls=[]
                m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:calls.append(uc.reg_read(UC_X86_REG_ECX)),begin=0x4fc980,end=0x4fc980)
                m.run(0x4c6cb0,(),event)
                assert calls==[m.target],calls
                other=m.enemy if target==0 else m.house
                assert bytes(m.u.mem_read(other,0x20000))==before[other]
                assert m.r(0xa83d4c)==m.house
                for obj in (m.unit,m.eng,m.building,m.enemyUnit):assert bytes(m.u.mem_read(obj,0x2000))==before[obj]
                if state=='active':
                    assert bytes(m.u.mem_read(m.target+0x1f6,1))==b'\x01'
                    assert m.r(m.target+0x298)==m.r(0xa8ed84) and m.r(m.target+0x2a0)==0
                    expected=bytearray(before[m.target]);expected[0x1f6]=1
                    struct.pack_into('<I',expected,0x298,m.r(0xa8ed84));struct.pack_into('<I',expected,0x2a0,0)
                    # Native code copies a scratch stack word into timer+4.
                    # It is not a deterministic fixture assertion or a field
                    # written by the sending DLL.
                    expected[0x29c:0x2a0]=m.u.mem_read(m.target+0x29c,4)
                    assert bytes(m.u.mem_read(m.target,0x20000))==bytes(expected)
                else:assert bytes(m.u.mem_read(m.target,0x20000))==before[m.target]
                assert not m.codeWrites
            check(f'receiver_{mode}_house_{target}_{state}',receiver)

Path(a.output).write_text(json.dumps({'scope':__doc__,
    'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
    'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),
    'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(row['status']!='PASS' for row in rows))
