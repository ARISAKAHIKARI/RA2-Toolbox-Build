"""Derive a runtime-only package and separate development archive from verified Full."""
from pathlib import Path
import hashlib,json,zipfile
from package_full_release import VERSION,package
R=Path(__file__).resolve().parents[1]
sha=lambda b:hashlib.sha256(b).hexdigest()
def main():
 full=R.parent/f'RA2-Toolbox-v{VERSION}-Full.zip'
 package(full)
 runtime=R.parent/f'RA2-Toolbox-v{VERSION}-Windows.zip'
 development=R.parent/f'RA2-Toolbox-v{VERSION}-Development.zip'
 records={}
 with zipfile.ZipFile(full) as src,zipfile.ZipFile(runtime,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as dst,zipfile.ZipFile(development,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as dev:
  for name in sorted(src.namelist()):
   group=name.split('/')[0];data=src.read(name)
   if group in {'source','docs','verification','third-party'}:
    dev.writestr(name,data);continue
   if group not in {'assets','profiles','engine','runtime','licenses'} and name not in {'RA2Toolbox.exe','GameRuntime.dll','README.txt','Start-Message.cmd'}:continue
   if name=='Start-Message.cmd':
    data=b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nfor %%F in (RA2Toolbox.exe GameRuntime.dll) do if not exist "%%F" (echo Missing %%F & pause & exit /b 1)\r\nset "RA2_TOOLBOX_DRIVER=message"\r\nstart "" "%~dp0RA2Toolbox.exe"\r\n'
   if name=='README.txt':data=data.replace((VERSION+' Full').encode(),(VERSION+' Windows').encode())
   if name=='licenses/THIRD-PARTY-NOTICES.txt':
    data=data.decode().replace('Complete upstream source archives: third-party/*.zip. Reproduction: source/convert_themes.py.','Complete upstream resource archives are in the separate Development package (third-party/*.zip), and the repository: https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/tree/v1.0.2/packages/v1.0.2').encode()
   dst.writestr(name,data);records[name]=sha(data)
 with zipfile.ZipFile(runtime) as z:
  assert z.testzip() is None
  assert {n for n in z.namelist() if '/' not in n and n.endswith(('.exe','.dll'))}=={'RA2Toolbox.exe','GameRuntime.dll'}
  assert not any(n.startswith(('source/','verification/','docs/','third-party/')) for n in z.namelist())
  for n,h in records.items():assert sha(z.read(n))==h
 with zipfile.ZipFile(development) as z:assert z.testzip() is None
 report={'version':VERSION,'runtime':{'name':runtime.name,'bytes':runtime.stat().st_size,'sha256':sha(runtime.read_bytes()),'files':records},'development':{'name':development.name,'bytes':development.stat().st_size,'sha256':sha(development.read_bytes())}}
 (R.parent/'DISTRIBUTION.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({k:({x:y for x,y in v.items() if x!='files'} if isinstance(v,dict) else v) for k,v in report.items()},indent=2))
if __name__=='__main__':main()
