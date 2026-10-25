"""Package the validated Windows pair and the corresponding tracked source."""
from pathlib import Path
import hashlib,json,os,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[2]
DIST=ROOT/'dist/v1.0.38-pre.9'
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
    build=json.loads((ROOT/'source/WINDOWS_BUILD_ARTIFACTS.json').read_text())
    ui=json.loads((ROOT/'source/WINDOWS_SETTINGS_SMOKE.json').read_text())
    assert build['file_version']=='1.0.38.9' and ui['status']=='PASS' and ui['version']=='1.0.38-pre.9'
    names=['RA2Toolbox.exe','GameRuntime.dll']
    for name in names:
        info=build['verified'][name]
        actual=(ROOT/'source'/name).read_bytes()
        assert info['sha256']==sha(actual)
        assert info['assist_protocol']==72
    assert build['verified'][names[0]]['assist_shared_bytes']==build['verified'][names[1]]['assist_shared_bytes']
    DIST.mkdir(parents=True,exist_ok=True)
    assets=[]
    update=DIST/'RA2-Toolbox-v1.0.38-pre.9-Update.zip'
    contents={name:(ROOT/'source'/name).read_bytes() for name in names}
    with zipfile.ZipFile(update,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in contents.items():z.writestr(name,data)
    with zipfile.ZipFile(update) as z:
        assert z.testzip() is None and set(z.namelist())==set(contents)
        for name,data in contents.items():assert z.read(name)==data
    assets.append({'name':update.name,'bytes':update.stat().st_size,'sha256':sha(update.read_bytes()),
                   'files':{name:{'bytes':len(data),'sha256':sha(data)} for name,data in contents.items()}})
    paths=subprocess.check_output(['git','ls-files','-z','--','source','AGENTS.md','README.md','LICENSE-GPL','THIRD_PARTY_NOTICES.md','docs',
          'packages/v1.0.38-pre.9','.github/workflows/validate-v138-pre2.yml','.github/workflows/publish-v138-pre9.yml'],
          cwd=ROOT).decode().strip('\0').split('\0')
    assert len(paths)>600
    assert {'README.md','LICENSE-GPL','THIRD_PARTY_NOTICES.md','docs/CHANGELOG-archive.md',
            'docs/licenses/Ares-LICENSE.txt','docs/licenses/FA2sp-LICENSE.txt'}.issubset(paths)
    src=DIST/'RA2-Toolbox-v1.0.38-pre.9-Source.zip'
    evidence={
      'release-verification/WINDOWS_BUILD_ARTIFACTS.json':(ROOT/'source/WINDOWS_BUILD_ARTIFACTS.json').read_bytes(),
      'release-verification/WINDOWS_SETTINGS_SMOKE.json':(ROOT/'source/WINDOWS_SETTINGS_SMOKE.json').read_bytes(),
    }
    with zipfile.ZipFile(src,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name in sorted(paths):
            p=Path(name);assert not p.is_absolute() and '..' not in p.parts and not (ROOT/p).is_symlink()
            assert p.suffix.lower() not in {'.exe','.dll','.pdb','.zip','.7z','.o','.lib'}
            # Archive the committed source, not reports regenerated in the build workspace.
            z.writestr(name,subprocess.check_output(['git','show','HEAD:'+name],cwd=ROOT))
        for name,data in evidence.items():z.writestr(name,data)
    with zipfile.ZipFile(src) as z:
        assert z.testzip() is None and set(z.namelist())==set(paths)|set(evidence)
        for name,data in evidence.items():assert z.read(name)==data
    assets.append({'name':src.name,'bytes':src.stat().st_size,'sha256':sha(src.read_bytes()),'tracked_files':len(paths)})
    report={'version':'1.0.38-pre.9','file_version':'1.0.38.9','assist_protocol':72,
       'source_commit':os.environ['GITHUB_SHA'],'validated_code_commit':os.environ['BUILD_SHA'],
       'validation_run':os.environ['BUILD_RUN'],'windows_ui':ui,
       'game_tested_this_release':False,'multiplayer_tested_this_release':False,'assets':assets}
    (DIST/'release-manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))
if __name__=='__main__':main()

