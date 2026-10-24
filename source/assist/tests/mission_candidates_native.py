"""Native task candidates and camera center: queue/readiness and Unit handlers.
The locomotor/map queries are controlled doubles. This does NOT prove whole-game
Hunt/Patrol/Retreat behavior, network compatibility, or state effects in a mod.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('sticky_mission_native.py').read_text().split('rows = []',1)[0],str(Path(__file__).with_name('sticky_mission_native.py')),'exec'))
rows=[]
for mission,name in ((0,'Sleep'),(4,'Retreat'),(14,'Ambush'),(15,'Hunt'),(23,'Harmless'),(25,'Patrol'),(28,'Wait')):
    u.mem_write(unit,bytes(0x2000));w(unit,0x7f5c70);w(unit+0x674,loco);w(loco,vt);w(vt+0x80,stop+16);w(unit+0xac,5);w(unit+0xb4,-1);w(0xa8ed84,100)
    run(0x5b35e0,mission,0);assert r(unit+0xb4)==mission
    assert run(0x744270)==1;run(0x5b3570);assert r(unit+0xac)==mission
    run(0x5b35e0,5,0);ready=run(0x744270)
    if ready:run(0x5b3570)
    assert r(unit+0xac)==(28 if mission==28 else 5)
    rows.append({'case':name,'mission':mission,'guard_exit_ready_in_fixture':bool(ready),'guard_exit_completed_in_fixture':r(unit+0xac)==5,'status':'PASS'})
for name,offset,address in (('Sleep',0x204,0x5b2e10),('Harmless',0x208,0x5b2e20),('Ambush',0x20c,0x5b2e30),('Wait',0x268,0x5b2fa0)):
    assert r(0x7f5c70+offset)==address
    before=bytes(u.mem_read(unit,0x800));run(address)
    assert u.reg_read(UC_X86_REG_EAX)==450 and before==bytes(u.mem_read(unit,0x800))
    rows.append({'case':name+'_handler','address':hex(address),'return':450,'unit_unchanged':True,'status':'PASS'})
# 6D8B30 derives the visible origin from the center using the live viewport.
def stop_after_origin(uc,address,size,unused):
    if address==0x6d8b96:uc.emu_stop()
origin_hook=u.hook_add(UC_HOOK_CODE,stop_after_origin,begin=0x6d8b96,end=0x6d8b96)
for width,height,cx,cy in ((640,600,300,1020),(1280,720,-200,2200),(641,601,300,1020)):
    w(unit+0xd64,cx);w(unit+0xd68,cy);w(0xb0ce30,width);w(0xb0ce34,height);w(stack,stop)
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,unit);u.emu_start(0x6d8b30,0x6d8b97,count=100)
    assert u.reg_read(UC_X86_REG_EIP)==0x6d8b96
    assert r(unit+0xb0)==((cx-width//2)&0xffffffff) and r(unit+0xb4)==((cy-height//2)&0xffffffff)
    rows.append({'case':'native_camera_center_to_origin','viewport':[width,height],'center':[cx,cy],'status':'PASS'})
u.hook_del(origin_hook)
Path(a.output).write_text(json.dumps({'scope':SCOPE,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
print('PASS',len(rows),'native candidate transitions, inert handlers and camera center; state effects unproven')
