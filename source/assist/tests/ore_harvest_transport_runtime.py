"""Independent 8-way native Harvest transport/gateway interaction probe.
Uses the actual compiled Runtime plus its current shared regression world.
No game process is accessed; original constructors and event transports run in
isolated x86 emulation with explicit world/Win32 seams.
"""
from pathlib import Path
import argparse,ctypes as C,hashlib,json,struct,sys,traceback
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--fixture',type=Path,default=Path(__file__).with_name('ore_harvest_runtime.py'));p.add_argument('--output','--out',dest='out',type=Path,required=True);a=p.parse_args()
text=a.fixture.read_text(encoding='utf-8');prefix=text[:text.index("check('native-unload-before-cadence")]
saved=sys.argv;sys.argv=[str(a.fixture),a.game,a.dll,'--output',str(a.out)];ns={'__file__':str(a.fixture),'__name__':'independent_ore_core'}
try:exec(compile(prefix,str(a.fixture),'exec'),ns)
finally:sys.argv=saved
results=[];initial_dll=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest();initial_game=hashlib.sha256(Path(a.game).read_bytes()).hexdigest()
for session in (0,3):
 for backend in (0,1):
  for gateway in (0,1):
   name=f'session-{session}-backend-{backend}-gateway-{gateway}'
   try:
    m=ns['fixture']();s=ns['state'](m);s.megaQueueMode=backend;s.nativeMegaGateway=gateway;ns['save'](m,s);m.w(0xA8B238,session);s=ns['wake'](m,1100)
    assert s.data.commandStatus==1 and ns['queued'](m)==0,(s.data.commandStatus,ns['queued'](m))
    assert s.nativeMegaGatewayStatus==(1 if gateway else 0),(gateway,s.nativeMegaGatewayStatus)
    before=s.nativeMegaGatewayAccepted;ns['start'](m);s=ns['finish'](m);assert ns['queued'](m)==1,(ns['queued'](m),s.data.lastSubmitError,s.data.megaQueueStatus)
    at=0x8B4204 if session==0 else 0xA802D4;packet=bytes(m.u.mem_read(at,111));assert packet[0]==4 and packet[0xC]==10 and packet[0x12]==0 and packet[0x17]==11 and packet[0x1C]==0
    assert struct.unpack_from('<I',packet,7)[0]==10 and struct.unpack_from('<I',packet,0x13)[0]==102106
    assert s.nativeMegaGatewayAccepted==before,'own automatic event was misclassified as fresh manual input'
    assert m.r(0x8B41F8)==int(session==0) and m.r(0xA802C8)==int(session!=0)
    for n in range(6):ns['wake'](m)
    assert ns['queued'](m)==1
    row={'case':name,'status':'PASS','session':session,'backend':backend,'gateway':gateway,'gateway_active_verified':bool(gateway),'one_native_Harvest_event':True,'correct_SP_DoList_or_MP_OutList':True,'own_event_not_manual_recaptured':True,'mega_queue_status':s.data.megaQueueStatus,'gateway_own_skips':s.nativeMegaGatewaySkipped}
    results.append(row);print('PASS',name,flush=True)
   except Exception as ex:
    results.append({'case':name,'status':'FAIL','error':repr(ex)});traceback.print_exc()
assert hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()==initial_dll and hashlib.sha256(Path(a.game).read_bytes()).hexdigest()==initial_game
report={'schema':'ra2.ore-harvest-transport.v1','case_count':len(results),'required_case_count':8,'scope':__doc__,'dll_sha256':initial_dll,'game_sha256':initial_game,'input_hashes_stable':True,'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'source_fixture_sha256':hashlib.sha256(a.fixture.read_bytes()).hexdigest(),'shared_model_sha256':hashlib.sha256(a.fixture.with_name('entry_runtime_regression.py').read_bytes()).hexdigest(),'passed':len(results)==8 and all(r['status']=='PASS'for r in results),'game_process_operated':False,'results':results}
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise SystemExit(not report['passed'])
