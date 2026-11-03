"""Compile actual Host mapping probe routes; use Win32 mappings for this fixture's PID only."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;repo=here.parents[1];src=repo/'assist/source';out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
paths=[src/'main.cpp',src/'native_viewport_host.h',src/'protocol.h',repo/'shared/source/native_viewport_projection.h',here/'native_viewport_probe_host.cpp',Path(__file__)]
before={str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths}
main=(src/'main.cpp').read_text(encoding='utf-8-sig')
def block(text,needle):
 start=text.index(needle);at=text.index('{',start);end=at+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
generated='\n'.join(block(main,n) for n in ('static void observeNativeViewportPrior(', 'static void probeNativeViewportMapping(){'))+'\n'
bridge=block(main,'static bool ensureBridge(){');existing=block(bridge,'if(mappingError==ERROR_ALREADY_EXISTS)')
prefix=existing[existing.index('  Shared*prior='):existing.index('  bool ours=')]
generated+='static bool fixturePriorPrefix(){'+prefix+'if(prior)UnmapViewOfFile(prior);return valid;}\n'
sample=block(main,'static void sampleExternal(DWORD now){')
assert sample.index('probeNativeViewportMapping();')<sample.index('nativeviewhost::sampleTransform(')
assert bridge.index('probeNativeViewportMapping();')<bridge.index('CreateFileMappingW(')
assert existing.index('observeNativeViewportPrior(prior,valid);')<existing.index('prior->stop=1')<existing.index('UnmapViewOfFile(prior)')
inc=out/'production-probe.inc';inc.write_text(generated,encoding='utf-8');results={}
for arch in ('x86_64','x86'):
 exe=out/(arch+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O1','-UNDEBUG',str(here/'native_viewport_probe_host.cpp'),'-DVIEWPORT_PROBE_ROUTES="'+inc.as_posix()+'"','-o',str(exe)]
 built=subprocess.run(cmd,capture_output=True,text=True);(out/(arch+'-build.log')).write_text(built.stdout+built.stderr,encoding='utf-8')
 if built.returncode:raise SystemExit(built.stdout+built.stderr)
 ran=subprocess.run([str(exe)],capture_output=True,text=True);(out/(arch+'.log')).write_text(ran.stdout+ran.stderr,encoding='utf-8')
 if ran.returncode:raise SystemExit(ran.stdout+ran.stderr)
 results[arch]=json.loads(ran.stdout.strip().splitlines()[-1])
assert before=={str(f.relative_to(repo)):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths},'Source changed during probe fixture'
report={'schema':'ra2.native-viewport-probe-host.v1','passed':True,'results':results,'sources':before,'extracted_routes_sha256':hashlib.sha256(inc.read_bytes()).hexdigest(),'scope':__doc__}
(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
