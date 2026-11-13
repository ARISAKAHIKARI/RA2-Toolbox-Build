"""Package an unpublished, paired update candidate without user configuration."""
import argparse, hashlib, json, os, zipfile
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',required=True,type=Path);p.add_argument('--out',required=True,type=Path)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
names=['RA2Toolbox.exe','GameRuntime.dll','GameRuntimeHost32.exe','WebView2Loader.dll',
 'licenses/WebView2/LICENSE.txt','licenses/WebView2/NOTICE.txt','BUILD_SOURCE_MANIFEST.json','BUILD_TARGET.json','WINDOWS_BUILD_ARTIFACTS.json']
files=[]
for name in names:
 path=a.root/name
 if not path.is_file():raise SystemExit('Missing paired update file: '+name)
 files.append({'name':name,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
manifest={'version':'1.0.50','candidate':'SamplingFix-20261113','source_commit':os.environ['GITHUB_SHA'],
 'host':'x64','runtime':'x86','public_release_replaced':False,'game_tested':False,'files':files}
raw=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
archive=a.out/'RA2Toolbox-1.0.50-SamplingFix-x64-Update.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for name in names:z.write(a.root/name,name)
 z.writestr('SAMPLING_FIX_BUILD.json',raw)
manifest['archive']={'name':archive.name,'bytes':archive.stat().st_size,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(a.out/'sampling-fix-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(manifest['archive']))
