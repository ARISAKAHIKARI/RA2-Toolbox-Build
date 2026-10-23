"""Verify prebuilt, tested experimental binaries and package tracked source."""
from pathlib import Path
import hashlib,json,os,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[2]
VERSION='v1.0.37-zoom.1'
PACKAGE=ROOT/'packages'/VERSION
DIST=ROOT/'dist'/VERSION
UPDATE=f'RA2-Toolbox-{VERSION}-Update.zip'
SOURCE=f'RA2-Toolbox-{VERSION}-Source.zip'
sha=lambda data:hashlib.sha256(data).hexdigest()

def prepare():
 manifest=json.loads((PACKAGE/'manifest.json').read_text())
 assert manifest['version']==VERSION and manifest['baseline_commit']=='78c3deafa3c73b7c1c3a3c50ed362dbeed8fa34f'
 asset=manifest['assets'][0];raw=(PACKAGE/UPDATE).read_bytes()
 assert asset['name']==UPDATE and len(raw)==asset['bytes'] and sha(raw)==asset['sha256']
 with zipfile.ZipFile(PACKAGE/UPDATE) as z:
  assert z.testzip() is None and set(z.namelist())=={'RA2Toolbox.exe','GameRuntime.dll','README-Windows.txt','RELEASE_NOTES.md'}
  for name,item in asset['files'].items():
   data=z.read(name);assert len(data)==item['bytes'] and sha(data)==item['sha256']
  dllsha=sha(z.read('GameRuntime.dll'))
 for name in ('zoom-runtime-cnc.json','zoom-runtime-mrga.json','zoom-scroll.json'):
  evidence=json.loads((ROOT/'source/verification/zoom-1'/name).read_text())
  assert evidence['dll_sha256']==dllsha and evidence['results'] and all(x['status']=='PASS' for x in evidence['results'])
 DIST.mkdir(parents=True,exist_ok=True);(DIST/UPDATE).write_bytes(raw)
 paths=subprocess.check_output(['git','ls-files','-z','--','source','AGENTS.md','README.md',
  '.github/workflows/publish-zoom.yml',str((PACKAGE/'README-Windows.txt').relative_to(ROOT)),
  str((PACKAGE/'RELEASE_NOTES.md').relative_to(ROOT)),str((PACKAGE/'manifest.json').relative_to(ROOT))],cwd=ROOT).decode().strip('\0').split('\0')
 assert len(paths)>600
 with zipfile.ZipFile(DIST/SOURCE,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name in sorted(paths):
   p=Path(name);assert not p.is_absolute() and '..' not in p.parts and not (ROOT/p).is_symlink()
   assert p.suffix.lower() not in {'.exe','.dll','.pdb','.zip','.7z','.o','.lib','.pyc','.sqlite'}
   info=zipfile.ZipInfo(name,(2026,10,23,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
   z.writestr(info,(ROOT/p).read_bytes())
 with zipfile.ZipFile(DIST/SOURCE) as z:assert z.testzip() is None and set(z.namelist())==set(paths)
 source=(DIST/SOURCE).read_bytes()
 result={**manifest,'source_commit':os.environ.get('GITHUB_SHA') or subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
  'assets':[asset,{'name':SOURCE,'bytes':len(source),'sha256':sha(source),'tracked_files':len(paths)}]}
 (DIST/'release-manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(result,ensure_ascii=False,indent=2))
if __name__=='__main__':prepare()
