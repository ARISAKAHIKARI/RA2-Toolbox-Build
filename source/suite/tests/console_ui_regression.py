import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[1];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
def block(text,needle):
 start=text.index(needle);at=text.index('{',start);depth=1;end=at+1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
main=(root/'assist/source/main.cpp').read_text(encoding='utf-8-sig')
generated=block(main,'struct PageControl {')+';\nstatic std::vector<PageControl> pageControls;\n'+block(main,'static void setConsoleRuntimeError(HWND control,bool failed){')+'\n'+block(main,'static void prepareConsoleSettingsModel(){')+'\n'
generated+='static void parameterStatus()'+block(main,'{const int p=shared?shared->techEnhanceParameterStatus:0;')+'\n'
generated+='static void worldFallback()'+block(main,'if((shared&&shared->sidebarStatus==3)||(extra.sidebarEnabled&&extra.sidebarMode==1))').replace('if(', '{if(',1)+'++shown;}\n'
inc=out/'console-metadata.inc';inc.write_text(generated,encoding='utf-8')
files=['assist/source/main.cpp','suite/source/console_model.h','suite/source/console_adapter.h','suite/source/settings_categories.h','suite/frontend/app.js','suite/frontend/app.css','suite/tests/console_store_test.mjs','suite/tests/console_model_test.cpp','suite/tests/console_adapter_windows.cpp','suite/tests/console_metadata.cpp','suite/tests/console_ui_regression.py']
def hashes():return {f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in files}
before=hashes();results={}
for arch in ('x86_64','x86'):
 result={}
 manifest=out/(arch+'-fixture.manifest');res=out/(arch+'-fixture.res');rc=out/(arch+'-fixture.rc')
 machine='amd64' if arch=='x86_64' else 'x86'
 manifest.write_text('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><assemblyIdentity type="win32" name="RA2.Console.Fixture" version="1.0.0.0" processorArchitecture="'+machine+'"/><dependency><dependentAssembly><assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="'+machine+'" publicKeyToken="6595b64144ccf1df" language="*"/></dependentAssembly></dependency></assembly>',encoding='utf-8')
 rc.write_text('1 24 "'+manifest.as_posix()+'"',encoding='utf-8')
 built=subprocess.run([str(Path(a.zig).resolve()),'rc','/fo',str(res),str(rc)],capture_output=True,text=True)
 if built.returncode:raise SystemExit(built.stdout+built.stderr)
 for name,libs in [('console_model_test',[]),('console_metadata',[]),('console_adapter_windows',['-lkernel32','-luser32','-lgdi32','-lcomctl32']),('async_ini_store_test',['-lkernel32'])]:
  exe=out/(arch+'-'+name+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O1','-UNDEBUG',str(here/(name+'.cpp')),'-o',str(exe),*libs]
  if name=='console_adapter_windows':cmd += [str(res)]
  if name=='console_metadata':cmd+=['-DCONSOLE_METADATA_ROUTES="'+inc.as_posix()+'"']
  built=subprocess.run(cmd,capture_output=True,text=True);(out/(arch+'-'+name+'-build.log')).write_text(built.stdout+built.stderr,encoding='utf-8')
  if built.returncode:raise SystemExit(built.stdout+built.stderr)
  ran=subprocess.run([str(exe)],capture_output=True,text=True,timeout=45);(out/(arch+'-'+name+'.log')).write_text(ran.stdout+ran.stderr,encoding='utf-8')
  if ran.returncode:raise SystemExit(str(ran.returncode)+' '+ran.stdout+ran.stderr)
  result[name]={'exit_code':ran.returncode,'stdout':ran.stdout.strip(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
 results[arch]=result
report={'schema':'ra2.console-ui-regression.v1','source_sha256':before,'changed_during_run':[f for f,h in hashes().items() if before[f]!=h],'assertions_enabled':True,'game_process_accessed':False,'architectures':results}
(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False,indent=2))

