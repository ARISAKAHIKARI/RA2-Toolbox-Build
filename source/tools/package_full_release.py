#!/usr/bin/env python3
"""Build a clean 0.7 Full package only from verified current binaries/evidence."""
from pathlib import Path
import hashlib,json,sys,zipfile
sys.path.insert(0,str(Path(__file__).resolve().parent))
from verify_windows_outputs import verify,NAMES
ROOT=Path(__file__).resolve().parent.parent
RESOURCE_ROOT=ROOT if (ROOT/'assets').is_dir() else ROOT.parent
VERSION='1.0'
SOURCE_DIRS=('assist','cursor','unit_monitor','suite','external_reader','shared','game_runtime','tests','tools','engine')
EXT={'.h','.hpp','.cpp','.c','.py','.rc','.manifest','.def','.cmd','.sh','.asm','.html'}
TOP=('build-zig.sh','validate-logic.py','BUILD_ZIG.cmd','BUILD_WINDOWS.cmd','BUILD-1.0-ON-WINDOWS.cmd','BUILD-AND-PACK-FULL.cmd','BUILD_SOURCE_MANIFEST.json','TECHNOLOGY_RELEASE_GATE.json','README-1.0.txt')
LOGS=('validation-single.log','pe-single.log','build-single.log','architecture-current.log','current-regressions.log')
RESULTS={'feature-types-current.json':130,'config-current.json':26,'startup-user.json':90,'startup-reference.json':90,'no-interception-user.json':149,'no-interception-reference.json':149,'ore-priority-user.json':28,'ore-priority-reference.json':28,'binding-current.json':3,'selection-current.json':12,'bridge-current.json':13,'timing-current.json':12,'robustness-current.json':15,'two-peer-current.json':55}
EVIDENCE=('TEST_ENVIRONMENT_CURRENT.json','UPSTREAM_REFERENCES.json','MO_UPSTREAM_AUDIT.json','ui-geometry.json','FEATURE_CATALOG.json','INTERCEPTION_INVENTORY.json','WINDOWS_BUILD_ARTIFACTS.json','BUILD-VERIFIED-v100.tag','RESOURCE_PROVENANCE.json','ARCHITECTURE_AUDIT.json','RETIRED_TEST_CONTRACTS.json','CURRENT_REGRESSION_RUN.json')
DOCS=('FEATURES.md','FULL-1.0-Architecture-and-Features.html','INTERCEPTION-INVENTORY.html','TEST-REPORT.html')
def sha(b):return hashlib.sha256(b).hexdigest()
def read(n):
 for p in (ROOT/n,ROOT/'verification'/n,ROOT.parent/'verification'/n):
  if p.is_file():return p.read_bytes()
 raise FileNotFoundError(n)
def package(output):
 gate=json.loads((ROOT/'TECHNOLOGY_RELEASE_GATE.json').read_text())
 assert gate['release_allowed'] and not gate['technology_fix_released']
 assert 'techMode=false;' in (ROOT/'assist/source/main.cpp').read_text()
 assert 'call<bool>(0x6A6300' not in (ROOT/'assist/source/sidebar.h').read_text()
 records,errors=verify(ROOT)
 if errors:raise RuntimeError('; '.join(errors))
 meta=json.loads(read('WINDOWS_BUILD_ARTIFACTS.json'))
 assert all(meta['verified'][n]['sha256']==records[n]['sha256'] for n in NAMES),'build manifest mismatch'
 validation=read(LOGS[0]).decode();assert ': FAIL' not in validation and 'TOTAL: 57 native tests + 23 source/UI checks + 2 mocked Win32 read-path tests' in validation
 for n,count in RESULTS.items():
  r=json.loads((ROOT/n).read_text());assert len(r['results'])==count and all(x['status']=='PASS' for x in r['results']),n
  assert r['inputs']['GameRuntime.dll']==records['GameRuntime.dll']['sha256'],n+' used stale DLL'
 run=json.loads(read('CURRENT_REGRESSION_RUN.json'));assert run['dll_sha256']==records['GameRuntime.dll']['sha256'] and len(run['runs'])==14 and all(x['exit_code']==0 for x in run['runs'])
 audit=json.loads(read('ARCHITECTURE_AUDIT.json'));assert audit['version']==VERSION
 for n,h in audit['source_sha256'].items():assert sha((ROOT/n).read_bytes())==h,'stale architecture audit: '+n
 # Historical baseline comparisons remain in prior releases; all current suites above must match this DLL.
 assert len(json.loads(read('FEATURE_CATALOG.json'))['features'])==68
 g=json.loads(read('ui-geometry.json'));assert (g['pages'],g['controls'],g['viewport_sizes'],g['fully_reachable_checks'])==(1,195,12,2244)
 files={n:(ROOT/n).read_bytes() for n in NAMES};files['README.txt']=read('README-1.0.txt')
 for n in ('Start-Message.cmd','Start-Frame.cmd','Test-Screen-Capture.cmd'):files[n]=(ROOT/n).read_bytes()
 for d in SOURCE_DIRS:
  for p in sorted((ROOT/d).rglob('*')):
   if p.is_file() and p.suffix in EXT and '__pycache__' not in p.parts:files['source/'+p.relative_to(ROOT).as_posix()]=p.read_bytes()
 for n in TOP:files['source/'+n]=read(n)
 resources={}
 for d in ('assets','profiles','runtime','engine','third-party','licenses'):
  folder=RESOURCE_ROOT/d
  if not folder.is_dir():raise FileNotFoundError('Full resources: '+str(folder))
  for p in sorted(folder.rglob('*')):
   if not p.is_file() or '__pycache__' in p.parts or p.suffix in ('.pyc','.whl'):continue
   n=p.relative_to(RESOURCE_ROOT).as_posix();b=p.read_bytes();files[n]=b;resources[n]={'bytes':len(b),'sha256':sha(b)}
 for n in ('runtime/python.exe','runtime/python312.dll','runtime/python312.zip','runtime/python312._pth','runtime/Lib/site-packages/cv2/cv2.pyd','assets/original.sha','assets/palette.bgra','assets/groups.json','engine/precompute.py','engine/dis_render.py'):
  assert n in resources,n
 assert len([n for n in resources if n.startswith('profiles/') and n.endswith('/profile.ini')])==3
 provenance=json.loads(read('RESOURCE_PROVENANCE.json'))
 for n,d in provenance['preserved_files'].items():assert resources[n]==d,'resource mismatch: '+n
 for n in DOCS:files['docs/'+n]=((ROOT/'docs'/n) if (ROOT/'docs'/n).is_file() else (ROOT.parent/'docs'/n)).read_bytes()
 for n in LOGS+EVIDENCE+tuple(RESULTS):files['verification/'+n]=read(n)
 for n in RESULTS:
  log=Path(n).with_suffix('.log').name;files['verification/'+log]=(ROOT/log).read_bytes()
 files['verification/FULL_RESOURCES_MANIFEST.json']=(json.dumps({'version':VERSION,'files':resources},ensure_ascii=False,indent=2)+'\n').encode()
 files['verification/CURRENT_SOURCE_SHA256.json']=(json.dumps({'version':VERSION,'sha256':{n:sha(b) for n,b in files.items() if n.startswith('source/')}},indent=2)+'\n').encode()
 release={'version':VERSION,'base_version':'0.7.20','base_user_netplay_feedback':'User reports no more disconnects with 0.7.15; 1.0 not yet field-tested','type':'Full','release_status':'configurable_feature_types','assist_protocol':41,'runtime_protocol':3,'entry_api':3,'engine_code_patching':False,'remaining_hooks':['manager WH_GETMESSAGE','manager WH_CALLWNDPROCRET'],'retired_tests':'RETIRED_TEST_CONTRACTS.json','feature_review_count':68,'portable_native_cases':57,'source_ui_checks':23,'mock_read_checks':2,'dll_regression_suites':RESULTS,'dll_regression_cases':sum(RESULTS.values()),'ui_controls':195,'ui_viewports':12,'ui_reachability_checks':2244,'project_binaries':list(NAMES),'injected_project_dlls':['GameRuntime.dll'],'windows_ui_tested':False,'windows_overlay_fps_measured':False,'full_game_tested':False,'real_netplay_tested':False,'real_platform_disconnect_fixed_confirmed':False,'real_latency_measured':False,'model':'actual x86 DLL/constructors; synthetic world/Win32. Two-peer receiver/transport and frame barriers modeled; no movement/combat/economy simulation. Sell receiver branch actual with final sale mocked.','preserved_baseline_resource_files':len(provenance['preserved_files']),'bundled_resource_files':len(resources),'full_python_runtime':'Windows x64 offline preprocessing, not injected'}
 files['verification/RELEASE_VALIDATION.json']=(json.dumps(release,ensure_ascii=False,indent=2)+'\n').encode()
 files['verification/PACKAGE_SHA256.json']=(json.dumps({n:sha(b) for n,b in sorted(files.items())},indent=2)+'\n').encode()
 with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for n,b in sorted(files.items()):z.writestr(n,b)
 with zipfile.ZipFile(output) as z:
  assert z.testzip() is None
  for n,h in json.loads(z.read('verification/PACKAGE_SHA256.json')).items():assert sha(z.read(n))==h,n
  assert sorted(n for n in z.namelist() if '/' not in n and n.endswith(('.dll','.exe')))==sorted(NAMES)
  assert not any(n.lower().endswith(('gamemd.exe','gamemd-spawn.exe')) for n in z.namelist())
 print(json.dumps({'file':str(output),'files':len(files),'bytes':output.stat().st_size,'sha256':sha(output.read_bytes()),'resource_files':len(resources)},indent=2))
if __name__=='__main__':package(Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT.parent/'RA2-Toolbox-v1.0-Full.zip')
