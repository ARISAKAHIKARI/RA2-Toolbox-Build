#!/usr/bin/env python3
"""Package one fresh, source-verified build over the released 1.0.47 resources."""
from pathlib import Path
import argparse,hashlib,json,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--logs',type=Path,required=True);p.add_argument('--checks',type=Path,required=True);p.add_argument('--base',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
if any(out.iterdir()):raise SystemExit('Package output must be empty')
def sha(data):return hashlib.sha256(data).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(a.repo),*args]).decode().strip()
version='1.0.48-pre.1';head=git('rev-parse','HEAD');checks=json.loads((a.checks/'checks.json').read_text());build=json.loads((a.logs/'build-provenance.json').read_text())
assert checks['passed'] and checks['source_unchanged'] and checks['source_commit']==head and checks['source_tree_sha']==git('rev-parse','HEAD:source')
assert build['commit']==head and build['source_tree_sha']==checks['source_tree_sha']
assert build['build_returncode']==build['static_pe_verification_returncode']==0
assert all(build[k] for k in ['source_inputs_unchanged_after_build','git_head_verified_at_finish','output_directory_was_empty','build_artifacts_unchanged_after_pe_verification'])
for name,value in checks['source_sha256'].items():assert sha((a.repo/name).read_bytes())==value,name
for name,value in build['build_artifacts'].items():assert sha((a.build/name).read_bytes())==value['sha256'],name
base=a.base.read_bytes();assert sha(base)=='cfeb5d0942455a6a1657153b849e61d5be9aa72ef868a7f3e2f56d8a28325ed4','Wrong released resource base'
with zipfile.ZipFile(a.base) as z:full={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
assert 'RA2Toolbox.exe' in full and 'GameRuntime.dll' in full
# Replace every executable component and metadata from the same fresh build.
replace={}
for name in ['RA2Toolbox.exe','GameRuntime.dll','GameRuntimeHost32.exe','WebView2Loader.dll','BUILD_SOURCE_MANIFEST.json','BUILD_TARGET.json','WINDOWS_BUILD_ARTIFACTS.json']:
 replace[name]=(a.build/name).read_bytes()
replace['README.txt']=(a.repo/'packages/v1.0.48-pre.1/README.txt').read_bytes()
for name in list(full):
 if name.lower().endswith(('.exe','.dll','.tag')) or name in replace or name in ['release-manifest.json','SHA256SUMS.txt']:del full[name]
full.update(replace)
for name in full:
 assert not any(part in ['source','__pycache__','logs'] for part in Path(name).parts),name
 assert not name.lower().endswith(('.pdb','.obj','.o','.pyc','.log')),name

def archive(path,files):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,data in sorted(files.items()):z.writestr(name,data)
 with zipfile.ZipFile(path) as z:
  assert z.testzip() is None and len(z.namelist())==len(set(z.namelist()))
  for name,data in files.items():assert z.read(name)==data
for suffix,files in [('x64-Full',full),('x64-Update',replace)]:archive(out/f'RA2Toolbox-{version}-{suffix}.zip',files)
evidence={'build/build-provenance.json':(a.logs/'build-provenance.json').read_bytes(),'build/cross-build.log':(a.logs/'cross-build.log').read_bytes(),'build/pe-verification.log':(a.logs/'pe-verification.log').read_bytes()}
for path in sorted(a.checks.rglob('*')):
 if path.is_file() and path.suffix in ['.json','.log'] and 'snapshot' not in path.parts:evidence['checks/'+path.relative_to(a.checks).as_posix()]=path.read_bytes()
archive(out/f'RA2Toolbox-{version}-verification.zip',evidence)
artifacts={p.name:{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in out.iterdir()}
manifest={'version':version,'release_status':'prerelease','source_commit':head,'source_tree_sha':checks['source_tree_sha'],'assist_protocol':104,'assist_shared_bytes':23162608,'build':build,'validation':{'focused_groups':len(checks['checks']),'passed':True,'real_game_tested':False,'multiplayer_tested':False,'native_aircraft_arrival_tested':False,'trigger_lock_binary_disassembly_verified':False},'resource_base':{'release':'1.0.47','sha256':sha(base)},'artifacts':artifacts}
(out/'release-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
(out/'SHA256SUMS.txt').write_text(''.join(sha(p.read_bytes())+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
print(json.dumps({'version':version,'source_commit':head,'files':[{'name':p.name,'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(out.iterdir())]},indent=2))
