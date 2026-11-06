"""Package same-commit paired binaries with hash-pinned offline resources."""
from __future__ import annotations
import argparse, hashlib, json, re, stat, subprocess, sys, tempfile, zipfile
from pathlib import Path, PurePosixPath

ROOT=Path(__file__).resolve().parents[2]
VERSION='1.0.43-pre.1'
BASELINE_COMMIT='20cfcb904e4c1bbf56b05202b903d72f5097a00f'
BINARIES=('RA2Toolbox.exe','GameRuntime.dll','GameRuntimeHost32.exe','WebView2Loader.dll')
GROUPS={'assets','profiles','engine','runtime','licenses'}
RESOURCE_REQUIRED={'runtime/python.exe','runtime/python312.dll','runtime/python312.zip','runtime/python312._pth','runtime/Lib/site-packages/cv2/cv2.pyd','assets/original.sha','assets/palette.bgra','assets/groups.json','engine/precompute.py','engine/dis_render.py'}
sys.path.insert(0,str(ROOT/'source/tools'))
from verify_windows_outputs import verify, read_release_version

def require(ok,message):
 if not ok:raise ValueError(message)
def sha(b):return hashlib.sha256(b).hexdigest()
def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def encoded(v):return (json.dumps(v,ensure_ascii=False,indent=2)+'\n').encode()
def safe(name):
 p=PurePosixPath(name)
 require(name and not p.is_absolute() and '\\'not in name and ':'not in name and not any(x in {'.','..'}for x in name.split('/')) and not any(ord(c)<32 for c in name),'Unsafe ZIP path: '+repr(name))
 return p
def restore_base(destination):
 manifest=load(ROOT/'release/v0.6.2-buildfix4/manifest.json');h=hashlib.sha256();size=0
 require(manifest['sha256']=='dedfd78d6dddfc0cd8748a32c94e37f971a66b16d47ce4132d8a160e120929f2','Unexpected resource baseline')
 with destination.open('wb')as out:
  for row in manifest['parts']:
   path=safe(row['path']);require(path.parts[:2]==('payload','v0.6.2-buildfix4-full'),'Unexpected resource part')
   data=(ROOT/path).read_bytes();require(len(data)==row['bytes']and sha(data)==row['sha256'],'Corrupt resource part: '+str(path));out.write(data);h.update(data);size+=len(data)
 require(size==manifest['bytes']and h.hexdigest()==manifest['sha256'],'Corrupt assembled resources')
 return {'bytes':size,'sha256':h.hexdigest(),'manifest':'release/v0.6.2-buildfix4/manifest.json'}
def write_zip(path,current,base=None):
 records={};seen=set()
 def put(z,name,data):
  safe(name);key=name.casefold();require(key not in seen,'Duplicate package path: '+name);seen.add(key)
  item=zipfile.ZipInfo(name,date_time=(2026,11,6,0,0,0));item.compress_type=zipfile.ZIP_DEFLATED;item.external_attr=0o100644<<16;z.writestr(item,data,compresslevel=6);records[name]={'bytes':len(data),'sha256':sha(data)}
 with zipfile.ZipFile(path,'w')as out:
  if base:
   with zipfile.ZipFile(base)as original:
    require(original.testzip()is None,'Resource ZIP CRC failed')
    for item in original.infolist():
     if item.is_dir():continue
     p=safe(item.filename);require(not stat.S_ISLNK(item.external_attr>>16),'Symlink in resources')
     parts=p.parts
     if parts[0]not in GROUPS and len(parts)>1 and parts[1]in GROUPS:parts=parts[1:]
     if parts[0]not in GROUPS:continue
     name='/'.join(parts)
     if name in current or '__pycache__'in parts or p.suffix.lower()in {'.pyc','.pyo','.whl','.pdb','.obj','.o','.dmp','.dump','.sqlite','.db'}:continue
     require(not any(x.lower().startswith('gamemd')for x in parts),'Private game content in resource package')
     put(out,name,original.read(item))
  for name,data in sorted(current.items()):put(out,name,data)
 with zipfile.ZipFile(path)as z:
  require(z.testzip()is None,'Package ZIP CRC failed')
  for name,row in records.items():require(sha(z.read(name))==row['sha256'],'Package verification failed: '+name)
  require({n for n in records if '/'not in n and n.lower().endswith(('.exe','.dll'))}==set(BINARIES),'Root program set differs')
  require(not any(n.startswith(('source/','docs/','verification/','third-party/'))for n in records),'Development content in user package')
 if base:
  require(RESOURCE_REQUIRED<=records.keys(),'Missing offline resources: '+str(RESOURCE_REQUIRED-records.keys()))
  require(len([n for n in records if n.startswith('profiles/')and n.endswith('/profile.ini')])==3,'Missing bundled cursor profiles')
 return {'name':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes()),'files':records}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binaries',required=True,type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--commit',required=True);a=p.parse_args()
 require(re.fullmatch('[0-9a-f]{40}',a.commit)is not None,'Exact source commit required')
 require(subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()==a.commit,'Checkout commit differs')
 require(read_release_version()==('1.0.43.1',VERSION),'Wrong prerelease header')
 source_identity=load(ROOT/'source/BUILD_SOURCE_MANIFEST.json')
 require(source_identity['version']==VERSION and source_identity['build_protocol']==97,'Source identity/protocol differs')
 identity=load(a.binaries/'CONSOLE_COMPILE.json');require(identity['commit']==a.commit and identity['host']=='x64'and identity['runtime']=='x86'and identity['broker']=='x86','Binary artifact commit/architecture differs')
 require(identity['game_tested']is False and identity['multiplayer_tested']is False,'Controlled build cannot certify a game')
 require((identity['version'],identity['file_version'],identity['assist_protocol'],identity['assist_shared_bytes'],identity['controlled_windows_checks'])==(VERSION,'1.0.43.1',97,23141856,True),'Incomplete controlled Windows validation or wrong version/protocol')
 current={n:(a.binaries/n).read_bytes()for n in BINARIES}
 require({r['name']for r in identity['files']}==set(BINARIES),'Artifact file membership differs')
 for row in identity['files']:require(row['bytes']==len(current[row['name']])and row['sha256']==sha(current[row['name']]),'Artifact digest differs')
 target=load(a.binaries/'BUILD_TARGET.json');require((target['host'],target['runtime'],target['broker'],target['assist_protocol'],target['cursor_protocol'],target['runtime_protocol'],target['console_ui'])==('x64','x86','x86',97,155,4,'webview2'),'Wrong build profile')
 require(load(a.binaries/'BUILD_SOURCE_MANIFEST.json')==source_identity,'Artifact source manifest differs from checkout')
 actual,errors=verify(a.binaries,host_arch='x64',webview2=True);require(not errors,'; '.join(errors))
 for n in ('LICENSE.txt','NOTICE.txt'):current['licenses/WebView2/'+n]=(a.binaries/'licenses/WebView2'/n).read_bytes()
 current['licenses/LICENSE-GPL.txt']=(ROOT/'LICENSE-GPL').read_bytes();current['licenses/THIRD_PARTY_NOTICES.md']=(ROOT/'THIRD_PARTY_NOTICES.md').read_bytes();current['licenses/xbrz-License.txt']=(ROOT/'source/third_party/xbrz/License.txt').read_bytes()
 current['README.txt']=Path(__file__).with_name('README.txt').read_bytes()
 current['Start.cmd']=b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nstart "" "%~dp0RA2Toolbox.exe"\r\n'
 a.out.mkdir(parents=True,exist_ok=True)
 update=write_zip(a.out/f'RA2Toolbox-{VERSION}-x64-Update.zip',current)
 full_files=dict(current)
 for path in (ROOT/'source/engine').rglob('*'):
  if path.is_file()and '__pycache__'not in path.parts and path.suffix in {'.py','.html'}:full_files['engine/'+path.relative_to(ROOT/'source/engine').as_posix()]=path.read_bytes()
 with tempfile.TemporaryDirectory()as temporary:
  base=Path(temporary)/'resources.zip';provenance=restore_base(base);full=write_zip(a.out/f'RA2Toolbox-{VERSION}-x64-Full.zip',full_files,base)
 report={'version':VERSION,'source_commit':a.commit,'baseline_commit':BASELINE_COMMIT,'integrated_release_source_commit':'e00b842e81676b41e85a2db4e8ad67cf60ca424b','assist_protocol':97,'assist_shared_bytes':23141856,'file_version':'1.0.43.1','resources':provenance,'actual_pe_verified':actual,'packages':[full,update],'full_game_tested':False,'multiplayer_tested':False}
 (a.out/'release-manifest.json').write_bytes(encoded(report))
 files=[a.out/full['name'],a.out/update['name'],a.out/'release-manifest.json']
 (a.out/'SHA256SUMS.txt').write_text(''.join(sha(f.read_bytes())+'  '+f.name+'\n'for f in files),encoding='ascii')
 print(json.dumps({k:v for k,v in report.items()if k not in {'actual_pe_verified','packages'}},indent=2));print('Verified packages:',full['name'],update['name'])
if __name__=='__main__':main()

