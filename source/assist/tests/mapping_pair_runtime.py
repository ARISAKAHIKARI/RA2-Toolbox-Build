"""Use the shipped host's namespace as the only Win32 mapping visible to the DLL.
Runs actual compiled Runtime hooks; Win32/world/renderer remain synthetic.
An ABI/version-only check previously missed pre.8's mismatched namespace.
"""
from pathlib import Path
import os,sys,re
host=Path(sys.argv.pop(3));expect_disconnected='--expect-disconnected' in sys.argv
if expect_disconnected:sys.argv.remove('--expect-disconnected')
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from pe32_image import Image
host_pe=Image(host.read_bytes());exports=host_pe.exports()
if 'AssistHostMappingPrefix' in exports:
 prefix=host_pe.at_rva(exports['AssistHostMappingPrefix'],44).decode('utf-16le').split('\0',1)[0]
else:
 # Historical pre.8 creates its mapping from this actual compiled format string.
 values=set(re.findall(r'Local\\RA2Assist_v[0-9]{3}_%lu',host.read_bytes().decode('utf-16le',errors='ignore')))
 assert len(values)==1,values
 prefix=values.pop()[:-3]
os.environ['RA2_FIXTURE_MAPPING_NAME']=prefix+'123'
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
for mode in (0,5,3,4):
 m=fixture(mode);s=state(m);connected=bool(s.data.match and s.data.commandStatus==1)
 assert connected!=expect_disconnected,(mode,prefix,s.data.match,s.data.commandStatus)
 if connected:
  s=pointer(m);assert s.pointer.ack==s.pointer.request and s.pointer.accepted==1 and count(m)==1
  s.request+=1;s.command=18;s.requestMatch=s.data.match;s.requestTick=m.clock;save(m,s);s=tick(m);assert s.ack==s.request and s.result>0
  assert not m.codeWrites
 rows.append({'mode':mode,'connected':connected,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'host_namespace':prefix,'expect_disconnected':expect_disconnected,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in [str(host),a.dll,a.game]},'results':rows},indent=2)+'\n')
print('PASS',len(rows),'actual EXE namespace / Runtime mapping, readiness, pointer and command acknowledgements; historical disconnected='+str(expect_disconnected))
