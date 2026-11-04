"""Own-process x86 DR retirement plus source-bound native UI diagnostics.

Actual Windows Suspend/Get/Set/readback/Resume and production Service; only
RemoveVectoredExceptionHandler failure is injected. No game process touched.
"""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;source=here.parent/'source';out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True);baseline=here/'fixtures/tech_ui_publish_baseline'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
core=source/'tech_ui_breakpoint_core.h';bridge=source/'tech_ui_breakpoint_bridge.h';manifest=json.loads((baseline/'manifest.json').read_text(encoding='utf-8'));assert sha(baseline/'publish.inc')==manifest['publish_inc_sha256'],'historical baseline extract changed'
tracked=[core,bridge,here/'tech_ui_publish_windows.cpp',Path(__file__),baseline/'publish.inc',baseline/'manifest.json'];before={str(p.relative_to(here.parents[1])):sha(p)for p in tracked}
s=bridge.read_text(encoding='utf-8-sig')
def block(text,needle):
 start=text.index(needle);at=text.index('{',start);depth=1;end=at+1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
need=['static bool healthy(){','static bool ready(uint32_t group){']
need+=['static uint32_t enhancementTags(){','static bool enhancementReady(){']
if 'static bool groupOff(uint32_t group){' in s:need.append('static bool groupOff(uint32_t group){')
need.append('static void publish(){')
current=out/'current-publish.inc';current.write_text('\n'.join(block(s,n)for n in need),encoding='utf-8')
results={}
for phase,inc in [('baseline',baseline/'publish.inc'),('current',current)]:
 exe=out/(phase+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target','x86-windows-gnu','-std=c++17','-O1','-UNDEBUG','-DEXPECT_HIDDEN='+str(int(phase=='baseline')),'-DTECH_CORE_HEADER="'+core.resolve().as_posix()+'"','-DTECH_PUBLISH_ROUTES="'+inc.resolve().as_posix()+'"',str(here/'tech_ui_publish_windows.cpp'),'-o',str(exe),'-lkernel32']
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(phase+'-build.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:raise SystemExit(r.stdout+r.stderr)
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20);(out/(phase+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:raise SystemExit(str(r.returncode)+r.stdout+r.stderr)
 results[phase]={'exit_code':r.returncode,'exe_sha256':sha(exe),'extract_sha256':sha(inc),**json.loads(r.stdout.strip())}
after={str(p.relative_to(here.parents[1])):sha(p)for p in tracked};assert before==after,'source changed during test'
report={'schema':'ra2.tech-ui-publish-windows.v1','scope':__doc__,'architecture':'x86','source_sha256':before,'changed_during_run':[],'baseline':manifest,'results':results,'pass':all(r['pass']for r in results.values()),'game_process_accessed':False,'assertions_enabled':True}
(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,ensure_ascii=False,indent=2))
