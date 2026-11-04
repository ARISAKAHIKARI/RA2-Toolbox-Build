"""Inventory captured MissionClass dispatcher, four class vtables and flags.

Executes only the original base empty handlers and AttackMove normalization.
The table is evidence of dispatch, not proof of every mission's full gameplay.
Requires the owner's private .mem/.valid capture, never distributed here.
"""
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
SCOPE=__doc__
exec(compile(Path(__file__).with_name('network_state_capture.py').read_text().split('rows=[crc_case',1)[0],str(Path(__file__).with_name('network_state_capture.py')),'exec'))
names='Sleep Attack Move QMove Retreat Guard Sticky Enter Capture Eaten Harvest AreaGuard Return Stop Ambush Hunt Unload Sabotage Construction Selling Repair Rescue Missile Harmless Open Patrol ParadropApproach ParadropOverfly Wait AttackMove SpyplaneApproach SpyplaneOverfly'.split()
classes={'Unit':0x7f5c70,'Infantry':0x7eb058,'Building':0x7e3ebc,'Aircraft':0x7e22a4}
flags='NoThreat Zombie Recruitable Paralyzed Retaliate Scatter'.split()
q=Probe();cs=Cs(CS_ARCH_X86,CS_MODE_32);cs.detail=True;rows=[]
for mission,name in enumerate(names):
    assert all(valid[0x5b34e8-BASE+mission*4:0x5b34e8-BASE+mission*4+4])
    branch=q.r(0x5b34e8+mission*4)
    code=bytes(q.u.mem_read(branch,32))
    calls=[i for i in cs.disasm(code,branch) if i.mnemonic=='call']
    assert len(calls)==1 and calls[0].operands[0].type==3
    slot=calls[0].operands[0].mem.disp
    assert 0x204<=slot<=0x270
    assert all(valid[branch-BASE:calls[0].address-BASE+calls[0].size])
    assert all(all(valid[vtable+slot-BASE:vtable+slot-BASE+4]) for vtable in classes.values())
    handlers={name:hex(q.r(vtable+slot)) for name,vtable in classes.items()}
    control=bytes(q.u.mem_read(0xa8e3a8+mission*0x20,0x20))
    assert all(valid[0xa8e3a8-BASE+mission*0x20:0xa8e3a8-BASE+(mission+1)*0x20])
    rows.append({'mission':mission,'name':name,'dispatcher_branch':hex(branch),'vtable_slot':hex(slot),'handlers':handlers,'flags':dict(zip(flags,control[4:10]))})
assert rows[0]['flags']==dict(zip(flags,(0,1,0,0,0,0)))
assert rows[5]['flags']==dict(zip(flags,(0,0,1,0,1,1)))
assert rows[6]['flags']==dict(zip(flags,(0,0,0,1,1,0)))
assert rows[23]['flags']==dict(zip(flags,(1,0,0,0,0,1)))
base=[]
for handler in range(0x5b2e10,0x5b2fd0,0x10):
    q=Probe();before=bytes(q.u.mem_read(S,0x2000))
    assert q.run(handler,S)==450 and bytes(q.u.mem_read(S,0x2000))==before
    base.append({'address':hex(handler),'return':450,'object_unchanged':True,'status':'PASS'})
normalization=[]
for mission,target,expected in ((0,0,0),(5,0,5),(23,0,23),(29,0,2),(29,1,1)):
    q=Probe();q.vt(S,classes['Unit'],{0x4c0:(0,0)})
    event=S+0x4000;q.b(event+12,mission);q.b(event+18,target)
    actual=q.run(0x4df0e0,S,(event,));assert actual==expected
    normalization.append({'mission':mission,'target_present':bool(target),'supports_attack_move':False,'normalized_mission':actual,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':SCOPE,'supported_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6','game_tested':False,'multiplayer_tested':False,'inventory':rows,'base_handler_checks':base,'normalization_checks':normalization},ensure_ascii=False,indent=2)+'\n')
print('PASS',len(rows),'mission dispatch entries;',len(base),'native empty handlers;',len(normalization),'native mission normalization cases')
