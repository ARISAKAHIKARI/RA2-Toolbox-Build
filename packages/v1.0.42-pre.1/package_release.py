"""Package the exact locally built/tested render fixes. Does not publish or compile."""
from __future__ import annotations
from pathlib import Path, PurePosixPath
import argparse, ast, hashlib, importlib.util, json, re, subprocess, zipfile

VERSION='1.0.42-pre.1';FILE_VERSION='1.0.42.1';TAG='v'+VERSION
BRANCH='codex/building-animation-insignia-fix-20261105'
BASE='513aeb4cf03238cce956bd35e0c9381c2a7cc484'
STEM='RA2Toolbox-'+VERSION+'-x64'
BINARIES=('RA2Toolbox.exe','GameRuntime.dll','GameRuntimeHost32.exe','WebView2Loader.dll')
REQUIRED_REPORTS={'building_animation','insignia_scale','watch_tracking','review'}
HERE=Path(__file__).resolve().parent

def require(ok,why):
 if not ok:raise ValueError(why)
def sha(data):return hashlib.sha256(data).hexdigest()
def load(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def encoded(value):return (json.dumps(value,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
def safe(name):
 p=PurePosixPath(name)
 require(name and not p.is_absolute() and '\\' not in name and ':' not in name and not any(x in {'','.', '..'} for x in name.split('/')) and not any(ord(c)<32 for c in name),'Unsafe package path: '+repr(name))
 return p
def zip_write(path,files):
 seen=set()
 with zipfile.ZipFile(path,'w') as z:
  for name,data in sorted(files.items()):
   safe(name);key=name.casefold();require(key not in seen,'Duplicate package member: '+name);seen.add(key)
   item=zipfile.ZipInfo(name,date_time=(2026,11,5,0,0,0));item.compress_type=zipfile.ZIP_DEFLATED;item.external_attr=0o100644<<16
   z.writestr(item,data,compresslevel=9)
 with zipfile.ZipFile(path) as z:
  require(z.testzip() is None,'Archive CRC failed: '+path.name)
  require(set(z.namelist())==set(files),'Archive membership differs')
  for name,data in files.items():require(z.read(name)==data,'Archive bytes differ: '+name)
 return {'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())}
def source_files(root):
 omitted={'.git','node_modules','__pycache__','.deps','build','build-x64','dist','projection-bin'}
 root_files={'AGENTS.md','LICENSE-GPL','THIRD_PARTY_NOTICES.md','README.md','.gitattributes','.gitignore'}
 for p in sorted(root.rglob('*')):
  rel=p.relative_to(root)
  if not p.is_file() or (rel.parts[0] not in {'source','docs','.github'} and rel.as_posix() not in root_files) or any(x in omitted for x in rel.parts) or p.suffix.lower() in {'.exe','.dll','.pdb','.obj','.o','.pyc','.pyo','.dmp','.dump','.sqlite','.db','.zip','.7z','.whl'}:continue
  require(not p.is_symlink(),'Symlink in source inventory: '+rel.as_posix())
  if any(x.lower().startswith('gamemd') for x in rel.parts):raise ValueError('Private game content in source inventory')
  yield p,rel.as_posix()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--source-root',type=Path,required=True,help='Checkout root containing source/, LICENSE-GPL and THIRD_PARTY_NOTICES.md')
 p.add_argument('--binaries',type=Path,required=True);p.add_argument('--build-logs',type=Path,required=True)
 p.add_argument('--inputs',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--adapter',type=Path,help='Optional local Windows build adapter to relocate into the source archive')
 a=p.parse_args();root=a.source_root.resolve();build=a.binaries.resolve();out=a.out.resolve();inputs=load(a.inputs)
 require(inputs.get('ready') is True,'Release inputs still contain placeholders')
 require((inputs.get('version'),inputs.get('file_version'),inputs.get('tag'),inputs.get('branch'),inputs.get('baseline_commit'))==(VERSION,FILE_VERSION,TAG,BRANCH,BASE),'Release identity differs')
 commit=inputs.get('compiled_source_commit','');require(isinstance(commit,str) and re.fullmatch('[0-9a-f]{40}',commit),'Exact compiled source commit required')
 # The local tree is a SHA-verified hydrated snapshot, not a Git checkout.
 # Root supplies the real remote source-only commit; every compiled byte is
 # checked here, and Actions checks commit ancestry before publication.
 source_hashes=load(a.build_logs/'source-before.json')
 require(source_hashes and inputs.get('compiled_source_sha256')==source_hashes,'Final source fingerprints must equal the recorded build inputs')
 for name,digest in source_hashes.items():
  safe(name);path=root/'source'/name
  require(path.is_file() and sha(path.read_bytes())==digest,'Compiled source changed: '+name)
 commands=load(a.build_logs/'commands.json');require(commands and all(x.get('exit_code')==0 for x in commands),'A compile/link command failed')
 for name in BINARIES:require((build/name).is_file(),'Missing paired binary: '+name)
 target=load(build/'BUILD_TARGET.json')
 require(target.get('version')==VERSION and target.get('file_version')==FILE_VERSION,'Build version differs')
 require((target.get('host'),target.get('runtime'),target.get('broker'),target.get('console_ui'))==('x64','x86','x86','webview2'),'Wrong build profile')
 require(target.get('game_tested') is False,'Offline packaging must not certify game execution')
 reports={};paths=inputs.get('reports',{})
 require(set(paths)==REQUIRED_REPORTS,'Only this round building-animation, insignia-scale, watch-tracking and review reports are accepted')
 for name,path in paths.items():
  safe(name);require(isinstance(path,str) and path,'Placeholder validation report: '+name)
  report_path=Path(path);report_path=report_path if report_path.is_absolute() else a.inputs.resolve().parent/report_path
  report=load(report_path);require(report.get('passed') is True or report.get('pass') is True or report.get('status')=='PASS','Validation report did not pass: '+name)
  tested=report.get('tested_source_sha256');require(isinstance(tested,dict) and tested,'Report must bind its tested production sources: '+name)
  for relative,wanted in tested.items():require(relative in source_hashes and source_hashes[relative]==wanted,'Report is stale for compiled source: '+name+'/'+relative)
  reports[name]={'path':report_path,'value':report,'sha256':sha(report_path.read_bytes())}
 verifier=root/'source/tools/verify_windows_outputs.py'
 spec=importlib.util.spec_from_file_location('render_fix_pe_verifier',verifier);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 actual,errors=module.verify(build,host_arch='x64',webview2=True);require(not errors,'; '.join(errors))
 verified=load(build/'WINDOWS_BUILD_ARTIFACTS.json')
 require((verified.get('file_version'),verified.get('host_architecture'),verified.get('console_ui'))==(FILE_VERSION,'x64','webview2'),'Saved PE verification identity differs')
 for name in ('RA2Toolbox.exe','GameRuntime.dll','GameRuntimeHost32.exe'):
  require(verified['verified'][name]['sha256']==sha((build/name).read_bytes()),'Saved PE digest differs: '+name)
 pe_report={'passed':True,'scope':'Fresh static PE architecture/version/ABI/dependency/WebView2 verification of this paired build; does not execute the game.',
            'compiled_source_commit':commit,'tested_source_sha256':source_hashes,'file_version':FILE_VERSION,'verified':actual,'game_tested':False,'multiplayer_tested':False}
 current={name:(build/name).read_bytes() for name in BINARIES}
 for name in ('BUILD_TARGET.json','WINDOWS_BUILD_ARTIFACTS.json','BUILD_SOURCE_MANIFEST.json'):
  current[name]=(build/name).read_bytes()
 for pth in sorted((build/'licenses').rglob('*')):
  if pth.is_file():current['licenses/'+pth.relative_to(build/'licenses').as_posix()]=pth.read_bytes()
 for name in ('LICENSE-GPL','THIRD_PARTY_NOTICES.md'):current[name]=(root/name).read_bytes()
 for folder in ('docs/licenses','source/third_party/xbrz','source/third_party/vulkan'):
  for pth in sorted((root/folder).rglob('*')):
   if pth.is_file() and ('LICENSE' in pth.name.upper() or pth.name in {'README.ra2toolbox.md','UPSTREAM.json'} or 'LICENSES' in pth.parts):current[pth.relative_to(root).as_posix()]=pth.read_bytes()
 notes=(HERE/'RELEASE_NOTES.md').read_bytes();current['README.txt']=notes
 for name,report in reports.items():current['diagnostics/'+name+'.json']=report['path'].read_bytes()
 current['diagnostics/pe_package.json']=encoded(pe_report)
 integration={'version':VERSION,'file_version':FILE_VERSION,'baseline_commit':BASE,'compiled_source_commit':commit,'compiled_source_sha256':source_hashes,
              'reports':{**{n:{'sha256':r['sha256'],'passed':True} for n,r in reports.items()},'pe_package':{'sha256':sha(encoded(pe_report)),'passed':True}},'game_tested':False,'multiplayer_tested':False}
 current['INTEGRATION.json']=encoded(integration)
 current['SHA256SUMS.txt']=''.join(sha(data)+'  '+name+'\n' for name,data in sorted(current.items())).encode('ascii')
 sources={rel:path.read_bytes() for path,rel in source_files(root)}
 for name,digest in source_hashes.items():require(sha(sources['source/'+name])==digest,'Source archive inventory differs: '+name)
 for name in ('package_release.py','publish_release.py','publish-v142-pre1.yml','RELEASE_NOTES.md'):
  sources['integration/'+name]=(HERE/name).read_bytes()
 for name,report in reports.items():sources['integration/reports/'+name+'.json']=report['path'].read_bytes()
 sources['integration/reports/pe_package.json']=encoded(pe_report)
 sources['integration/INTEGRATION.json']=encoded(integration)
 if a.adapter:
  adapter=a.adapter.read_text(encoding='utf-8-sig')
  require("SOURCE = HERE / 'latest' / 'source'" in adapter,'Unexpected adapter source root')
  adapter=adapter.replace("SOURCE = HERE / 'latest' / 'source'","SOURCE = HERE.parent / 'source'",1)
  adapter=re.sub(r'^TOOLCHAIN = .*$',"TOOLCHAIN = Path(os.environ['RA2_TOOLCHAIN'])",adapter,count=1,flags=re.M);ast.parse(adapter)
  sources['integration/build_render_fixes.py']=adapter.encode('utf-8')
 sources['README-SOURCE.txt']=b'Corresponding source and offline regression evidence. Canonical build: source/build-zig-x64.sh (Zig 0.13.0). Optional Windows adapter: integration/build_render_fixes.py; set RA2_TOOLCHAIN to a directory containing zig-windows-x86_64-0.13.0, python, and webview2/1.0.4314.35. No game executable or game assets included.\n'
 out.mkdir(parents=True,exist_ok=True)
 assets=[zip_write(out/(STEM+'-Update.zip'),current),zip_write(out/(STEM+'-Source.zip'),sources)]
 manifest={'version':VERSION,'file_version':FILE_VERSION,'tag':TAG,'baseline_commit':BASE,'compiled_source_commit':commit,'compiled_source_sha256':source_hashes,'artifacts':assets,
           'paired_binaries':[{ 'name':n,'bytes':len(current[n]),'sha256':sha(current[n])} for n in BINARIES],
           'source_files':{name:{'bytes':len(data),'sha256':sha(data)} for name,data in sorted(sources.items())},'actual_pe_verified':actual,'reports':integration['reports'],'game_tested':False,'multiplayer_tested':False}
 manifest_path=out/'release-manifest.json';manifest_path.write_bytes(encoded(manifest))
 assets.append({'name':manifest_path.name,'bytes':manifest_path.stat().st_size,'sha256':sha(manifest_path.read_bytes())})
 ready={'tag':TAG,'version':VERSION,'file_version':FILE_VERSION,'branch':BRANCH,'baseline_commit':BASE,'compiled_source_commit':commit,'compiled_source_sha256':source_hashes,'assets':assets,'required_reports':sorted(REQUIRED_REPORTS|{'pe_package'}),'game_tested':False,'multiplayer_tested':False}
 (out/'publish-ready.json').write_bytes(encoded(ready));(out/'RELEASE_NOTES.md').write_bytes(notes)
 print(json.dumps({'passed':True,'assets':assets,'ready':str(out/'publish-ready.json')},indent=2))

if __name__=='__main__':main()
