"""Final DLL native Sell cursor adapters and original CanSell/constructor/OutList.
Synthetic Win32/world. Real manager installs the two fixed adapters; original
Sell cursor blocks and sale construction run, but no network sender, receiver,
refund or building destruction executes. Owner adjustment is not sale completion.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('native_sell_support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
CURSOR=0x6929e5;OWNER=0x4ac179
CURSOR_BYTES=bytes.fromhex('e8 06 8d e7 ff');OWNER_BYTES=bytes.fromhex('8b 0d 4c 3d a8 00 8b 51 30')
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as error:rows.append({'case':name,'status':'FAIL','error':repr(error)});traceback.print_exc()
def config(m,**kw):
 s=n.state(m)
 for k,v in kw.items():setattr(s,k,v)
 n.save(m,s)
def setup(mode=5,enabled=True,preserve=True,conflict=None):
 m=n.fixture(mode);m.vector(0xa8da74,[])
 for house,ident in ((m.house,100),(m.enemy,200)):
  typ=m.alloc(0x2000);m.w(house+0x34,typ);m.w(house+0x10,ident);m.u.mem_write(typ+0x24,b'Americans\0')
 m.b(m.house+0x1ec,1);m.b(m.enemy+0x1ed,mode in (3,4));m.w(0xa8b230,m.alloc(0x4000))
 m.enemyType=m.r(m.enemy+0x34);m.w(m.building+0x21c,m.enemy);m.b(m.building+0x6e9,1);m.w(m.building+0x534,1);m.b(m.buildT+0x1579,0)
 m.b(0x880999,1);m.w(0xa8b24c,1)
 if conflict:m.u.mem_write(conflict,b'\x90')
 config(m,nativeSellCursor=int(enabled),networkHousePreserve=int(preserve));n.tick(m)
 m.target=m.building
 # This separate cursor-kind flag only distinguishes Sell/SellUnit. The real
 # building CanSell at4494C0 and its current-mission getter remain unmocked.
 flag=m.r(0x7e3ebc+0x80);m.u.hook_add(UC_HOOK_CODE,lambda u,at,size,_:m.ret(0,0),begin=flag,end=flag)
 m.ctorCalls=[]
 def ctor(u,at,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);m.ctorCalls.append((m.r(sp+4),m.r(sp+8)))
 m.u.hook_add(UC_HOOK_CODE,ctor,begin=0x4c65e0,end=0x4c65e0)
 return m

def block(m,start,ends,registers=None):
 sp=m.STACK+0x1d000;m.u.mem_write(sp-0x1000,bytes(0x1800));m.u.reg_write(UC_X86_REG_ESP,sp)
 if registers:
  for reg,value in registers.items():m.u.reg_write(reg,value)
 hit=[]
 def stop(u,at,size,_):hit.append(at);u.reg_write(UC_X86_REG_EIP,m.STOP)
 hooks=[m.u.hook_add(UC_HOOK_CODE,stop,begin=at,end=at) for at in ends]
 try:m.u.emu_start(start,m.STOP,count=2000000)
 finally:
  for h in hooks:m.u.hook_del(h)
 assert len(hit)==1,('block did not stop',hex(m.u.reg_read(UC_X86_REG_EIP)))
 return hit[0],sp

def cursor(m):
 end,sp=block(m,0x6929cd,(0x692a29,0x692b06),{UC_X86_REG_ESI:m.target})
 if end==0x692b06:assert m.r(sp+0x10)==12,m.r(sp+0x10)
 return end==0x692b06

def click(m):
 before=m.r(0xa802c8);end,sp=block(m,0x4ac151,(0x4ac20c,),{UC_X86_REG_EAX:12,UC_X86_REG_ESI:m.target,UC_X86_REG_EBX:m.timer})
 assert m.u.reg_read(UC_X86_REG_ESP)==sp,'native click block stack imbalance'
 if m.r(0xa802c8)==before:return None
 assert m.r(0xa802c8)==before+1
 tail=(m.r(0xa802d0)-1)&127;return bytes(m.u.mem_read(0xa802d4+tail*111,111))

def assert_packet(m,packet,house):
 assert packet and packet[0]==0x16 and packet[2]==house and packet[7:12]==struct.pack('<IB',12,0x34),packet[:16].hex() if packet else None
 assert m.r(0x8b41f8)==0

for mode in (0,5,3,4):
 for case in ('enabled','off','network_off','ai_no_scheduler','own','allied','neutral','passive','observer','defeated','dead','wrong_kind','chat','foreground','not_sell_mode','drained','unsellable','already_selling'):
  def eligibility(mode=mode,case=case):
   m=setup(mode,case!='off',case!='network_off')
   if case=='own':m.w(m.building+0x21c,m.house)
   if case=='allied':m.w(m.house+0x5788,3)
   if case=='ai_no_scheduler':m.b(m.enemy+0x1ec,0);m.b(m.enemy+0x1ed,0)
   if case=='neutral':m.u.mem_write(m.enemyType+0x24,b'Neutral\0')
   if case=='passive':m.b(m.enemyType+0x1a6,1)
   if case=='observer':m.w(0xac1198,m.enemy)
   if case=='defeated':m.b(m.enemy+0x1f5,1)
   if case=='dead':m.w(m.building+0x6c,0);m.b(m.building+0x74,0)
   if case=='wrong_kind':m.target=m.enemyUnit
   if case=='chat':m.b(0xa8bc60+25,1)
   if case=='foreground':config(m,foreground=0)
   if case=='not_sell_mode':m.b(0x880999,0)
   if case=='drained':m.w(m.building+0x1d0,m.enemyUnit)
   if case=='unsellable':m.b(m.buildT+0x1579,1)
   if case=='already_selling':m.w(m.building+0xac,19)
   # Begin at the actual owner branch; not_sell_mode deliberately forces this
   # block to test the adapter guard although normal native UI skips it.
   allow=case in ('enabled','own') or case in ('network_off','ai_no_scheduler') and mode in (0,5)
   assert cursor(m)==allow,(case,n.state(m).nativeSellCursorStatus,n.state(m).networkHouseStatus)
   if allow:
    before=(bytes(m.u.mem_read(m.building,0x1000)),m.r(0xa83d4c));queued=n.state(m).data.queuedEvents
    packet=click(m);assert_packet(m,packet,0 if case=='own' else 1)
    assert n.state(m).nativeSellOwnerAdjusted==int(case!='own')
    assert n.state(m).data.queuedEvents==queued,'native adjustment is not toolbox queued receipt'
    assert before==(bytes(m.u.mem_read(m.building,0x1000)),m.r(0xa83d4c))
  check(f'cursor_{mode}_{case}',eligibility)

for case in ('expired_time','expired_frame','object_id','owner_id','owner_index','owner_changed','house_slot','toggle_off','preserve_off','chat_after','foreground_after','can_sell_after','queue_safe_limit','queue_full','bad_tail','repeat_without_cursor','wrong_thread'):
 def revalidate(case=case):
  m=setup(3);assert cursor(m);before=n.state(m).nativeSellOwnerAdjusted
  if case=='expired_time':m.clock+=1501;config(m,heartbeat=m.clock)
  if case=='expired_frame':m.w(0xa8ed84,m.r(0xa8ed84)+91)
  if case=='object_id':m.w(m.building+0x10,99)
  if case=='owner_id':m.w(m.enemy+0x10,201)
  if case=='owner_index':m.w(m.enemy+0x30,2)
  if case=='owner_changed':m.w(m.building+0x21c,m.house)
  if case=='house_slot':m.vector(0xa80228,[m.enemy,m.house])
  if case=='toggle_off':config(m,nativeSellCursor=0)
  if case=='preserve_off':config(m,networkHousePreserve=0)
  if case=='chat_after':m.b(0xa8bc60+25,1)
  if case=='foreground_after':config(m,foreground=0)
  if case=='can_sell_after':m.b(m.buildT+0x1579,1)
  if case=='queue_safe_limit':m.w(0xa802c8,112)
  if case=='queue_full':m.w(0xa802c8,128)
  if case=='bad_tail':
   # Execute only adapter; the deliberately invalid native queue must never
   # reach the game's unchecked OutList writer in this synthetic negative.
   m.w(0xa802d0,128)
  if case=='repeat_without_cursor':assert_packet(m,click(m),1);before+=1
  if case=='wrong_thread':m.current_thread+=1
  if case=='bad_tail':
   block(m,OWNER,(0x4ac182,),{UC_X86_REG_ESI:m.target});assert m.u.reg_read(UC_X86_REG_EDX)==0
  else:
   packet=click(m)
   if case=='queue_full':assert packet is None
   else:assert packet and packet[2]==0,(case,packet[:12].hex() if packet else None)
  assert n.state(m).nativeSellOwnerAdjusted==before
 check('revalidate_'+case,revalidate)

for site in (CURSOR,OWNER):
 def conflict(site=site):
  m=setup(5,conflict=site);assert n.state(m).nativeSellCursorStatus<0
  assert bytes(m.u.mem_read(site,1))==b'\x90'
  other=OWNER if site==CURSOR else CURSOR;original=OWNER_BYTES if other==OWNER else CURSOR_BYTES
  assert bytes(m.u.mem_read(other,len(original)))==original
 check('signature_conflict_'+hex(site),conflict)

def restored():
 m=setup(5);assert cursor(m);config(m,nativeSellCursor=0);n.tick(m)
 assert n.state(m).nativeSellCursorStatus==0
 assert bytes(m.u.mem_read(CURSOR,5))==CURSOR_BYTES and bytes(m.u.mem_read(OWNER,9))==OWNER_BYTES
 assert not cursor(m)
check('toggle_restores_both_sites',restored)

for site in (CURSOR,OWNER):
 for accept in (False,True):
  def abi(site=site,accept=accept):
   m=setup(5);assert cursor(m)
   if not accept:config(m,nativeSellCursor=0)
   initial={UC_X86_REG_EAX:0x12345678,UC_X86_REG_EBX:0x23456789,UC_X86_REG_ECX:m.enemy,UC_X86_REG_EDX:0x3456789a,UC_X86_REG_ESI:m.building,UC_X86_REG_EDI:0x456789ab,UC_X86_REG_EBP:0x56789abc}
   xmm={reg:0x0123456789abcdeffedcba9876543210+i for i,reg in enumerate((UC_X86_REG_XMM0,UC_X86_REG_XMM1,UC_X86_REG_XMM2,UC_X86_REG_XMM3,UC_X86_REG_XMM4,UC_X86_REG_XMM5,UC_X86_REG_XMM6,UC_X86_REG_XMM7))}
   for reg,val in xmm.items():m.u.reg_write(reg,val)
   m.u.reg_write(UC_X86_REG_FPCW,0x77f);m.u.reg_write(UC_X86_REG_MXCSR,0x3f80);m.u.reg_write(UC_X86_REG_EFLAGS,0x647)
   end,sp=block(m,site,(site+5 if site==CURSOR else 0x4ac182,),initial)
   assert m.u.reg_read(UC_X86_REG_ESP)==sp
   for reg,val in xmm.items():assert m.u.reg_read(reg)==val,('XMM',reg)
   assert m.u.reg_read(UC_X86_REG_FPCW)==0x77f and m.u.reg_read(UC_X86_REG_MXCSR)==0x3f80
   for reg,val in initial.items():
    if site==OWNER and reg in (UC_X86_REG_ECX,UC_X86_REG_EDX):continue
    if site==CURSOR and reg in (UC_X86_REG_EAX,UC_X86_REG_EDX):continue
    assert m.u.reg_read(reg)==val,('register',reg,hex(m.u.reg_read(reg)),hex(val))
   if site==OWNER:
    assert m.u.reg_read(UC_X86_REG_ECX)==m.house and m.u.reg_read(UC_X86_REG_EDX)==int(accept)
    assert m.u.reg_read(UC_X86_REG_EFLAGS)&0xcd5==0x647&0xcd5
   else:assert m.u.reg_read(UC_X86_REG_EAX)==int(accept)
  check(f'abi_{site:x}_{accept}',abi)
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
