"""Compiled manager-owned acquisition-threshold adapter, original x87 compare.

Synthetic objects/Win32. Executes the comparison and original branch, not full
target search, combat, explicit attack behavior or live multiplayer.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
for session in (0,5,3,4):
    for network in (0,1):
        m=fixture(session);site=0x6f7d44;original=bytes(m.u.mem_read(site,6))
        s=state(m);s.lowDamageFilter=1;s.lowDamageNetwork=network;save(m,s);s=tick(m)
        enabled=session in (0,5) or bool(network)
        assert (bytes(m.u.mem_read(site,6))!=original)==enabled and s.lowDamageStatus==(1 if enabled else 2)
        out=[]
        for at in (0x6f7d55,0x6f894f):
            m.u.hook_add(UC_HOOK_CODE,lambda u,at,size,_:(out.append(at),u.reg_write(UC_X86_REG_EIP,m.STOP)),begin=at,end=at)
        wh=m.alloc();assert struct.unpack('<f',m.u.mem_read(0x7f4e38,4))[0]<0.02
        for own in (True,False):
            m.w(m.unit+0x21c,m.house if own else m.enemy)
            for value in (0.,0.01,0.019999,0.02,0.020001,1.,float('nan')):
                m.u.mem_write(wh+0xa0,struct.pack('<d',value));out.clear()
                m.u.reg_write(UC_X86_REG_ECX,wh);m.u.reg_write(UC_X86_REG_EAX,0);m.u.reg_write(UC_X86_REG_EDI,m.unit);m.u.reg_write(UC_X86_REG_ESP,m.STACK+0x18000)
                m.u.emu_start(0x6f7d3d,m.STOP,count=500)
                threshold=0.02 if enabled and (own or session in (3,4)) else struct.unpack('<f',m.u.mem_read(0x7f4e38,4))[0]
                rejected=not(value>threshold)
                assert out==[0x6f894f if rejected else 0x6f7d55],(session,network,own,value,out)
                rows.append({'session':session,'network_opt_in':network,'own':own,'verses':'NaN' if value!=value else value,'rejected_at_gate':rejected,'status':'PASS'})
        s=state(m);s.lowDamageFilter=0;save(m,s);tick(m)
        assert bytes(m.u.mem_read(site,6))==original and count(m)==0
        rows.append({'session':session,'network_opt_in':network,'case':'disable_restores_original_no_event','status':'PASS'})
# A conflicting site or dependent continuation must not be overwritten.
for conflict in ('site','context'):
    m=fixture(0);at=0x6f7d44 if conflict=='site' else 0x6f7d4a;m.u.mem_write(at,b'\x90');m.codeWrites.clear()
    s=state(m);s.lowDamageFilter=1;save(m,s);s=tick(m)
    assert s.lowDamageStatus==-2 and not m.codeWrites
    rows.append({'case':conflict+'_conflict_preserved','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':SCOPE,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
print('PASS',len(rows),'native-threshold adapter, owner, session, precision and restore cases')
