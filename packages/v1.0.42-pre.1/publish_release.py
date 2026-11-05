"""Publish only already-built, tested, fingerprint-pinned 1.0.42-pre.1 assets.

Invoked by the publication-only Actions workflow. Does not build or package.
"""
from pathlib import Path
import hashlib,json,os,re,subprocess,zipfile
from urllib.parse import quote

ROOT=Path(__file__).resolve().parents[2];PACKAGE=Path(__file__).resolve().parent
TAG='v1.0.42-pre.1';VERSION='1.0.42-pre.1';FILE_VERSION='1.0.42.1'
BRANCH='codex/building-animation-insignia-fix-20261105'
BASE='513aeb4cf03238cce956bd35e0c9381c2a7cc484'
REPO='ARISAKAHIKARI/RA2-Toolbox-Build'
NAMES={'RA2Toolbox-'+VERSION+'-x64-Update.zip','RA2Toolbox-'+VERSION+'-x64-Source.zip','release-manifest.json'}
BINARIES={'RA2Toolbox.exe','GameRuntime.dll','GameRuntimeHost32.exe','WebView2Loader.dll'}
REPORTS={'building_animation','insignia_scale','watch_tracking','review','pe_package'}
OUT=ROOT/'publication';OUT.mkdir(exist_ok=True)

def require(ok,why):
 if not ok:raise ValueError(why)
def sha(data):return hashlib.sha256(data).hexdigest()
def load(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def record(name,value):
 path=OUT/name;path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');return path
def api(endpoint,method='GET',data=None):
 command=['gh','api','--method',method,endpoint]
 if data is not None:command+=['--input',str(record('request.json',data))]
 return json.loads(subprocess.check_output(command))
def tag_sha():
 result=subprocess.run(['gh','api',f'repos/{REPO}/git/ref/tags/{TAG}'],text=True,capture_output=True)
 if result.returncode==0:return api(f'repos/{REPO}/commits/{TAG}')['sha']
 require(re.search(r'\(HTTP 404\)',result.stderr) is not None,'Cannot prove tag state: '+result.stderr)
 return None
def validate_local():
 ready=load(PACKAGE/'publish-ready.json');manifest=load(PACKAGE/'release-manifest.json')
 require((ready.get('tag'),ready.get('version'),ready.get('file_version'),ready.get('branch'))==(TAG,VERSION,FILE_VERSION,BRANCH),'Publication identity differs')
 require(ready.get('baseline_commit')==BASE and manifest.get('baseline_commit')==BASE,'Stable baseline identity differs')
 require(ready.get('game_tested') is False and ready.get('multiplayer_tested') is False,'Validation scope differs')
 hashes=ready.get('compiled_source_sha256');require(isinstance(hashes,dict) and hashes,'Missing exact compiled source hashes')
 source_commit=ready.get('compiled_source_commit','');require(isinstance(source_commit,str) and re.fullmatch('[0-9a-f]{40}',source_commit),'Invalid remote compiled source commit')
 parents=subprocess.check_output(['git','cat-file','-p','HEAD'],cwd=ROOT,text=True).split('\n\n',1)[0].splitlines()
 require([x[7:] for x in parents if x.startswith('parent ')]==[source_commit],'Publication must directly follow the tested source commit')
 source_header=subprocess.check_output(['git','cat-file','-p',source_commit],cwd=ROOT,text=True).split('\n\n',1)[0].splitlines()
 require([x[7:] for x in source_header if x.startswith('parent ')]==[BASE],'Source commit must directly follow the latest baseline')
 subprocess.run(['git','merge-base','--is-ancestor',source_commit,'HEAD'],cwd=ROOT,check=True)
 subprocess.run(['git','diff','--quiet',source_commit,'HEAD','--','source'],cwd=ROOT,check=True)
 require(set(ready.get('required_reports',[]))==REPORTS,'Wrong round validation reports')
 for name,wanted in hashes.items():
  parts=Path(name).parts;require(not Path(name).is_absolute() and '..' not in parts and '\\' not in name and ':' not in name,'Unsafe source path')
  require(sha((ROOT/'source'/name).read_bytes())==wanted,'Published source differs from tested build: '+name)
 require({x['name'] for x in ready['assets']}==NAMES and len(ready['assets'])==len(NAMES),'Unexpected asset membership')
 require(manifest['artifacts']==[x for x in ready['assets'] if x['name']!='release-manifest.json'],'Manifest asset list differs')
 require(manifest['compiled_source_sha256']==hashes and manifest['compiled_source_commit']==ready['compiled_source_commit'],'Manifest source identity differs')
 require((manifest['version'],manifest['file_version'],manifest['tag'])==(VERSION,FILE_VERSION,TAG),'Manifest version differs')
 for item in ready['assets']:
  require(item['name'] in NAMES,'Unexpected asset name');data=(PACKAGE/item['name']).read_bytes()
  require(len(data)==item['bytes'] and sha(data)==item['sha256'],'Package digest differs: '+item['name'])
  if item['name'].endswith('.zip'):
   with zipfile.ZipFile(PACKAGE/item['name']) as z:require(z.testzip() is None,'Package CRC differs')
 update=next(x for x in ready['assets'] if x['name'].endswith('-Update.zip'))
 source=next(x for x in ready['assets'] if x['name'].endswith('-Source.zip'))
 with zipfile.ZipFile(PACKAGE/update['name']) as z:
  target=json.loads(z.read('BUILD_TARGET.json'));integration=json.loads(z.read('INTEGRATION.json'))
  require((target['version'],target['file_version'],target['host'],target['runtime'],target['broker'],target['console_ui'])==(VERSION,FILE_VERSION,'x64','x86','x86','webview2'),'Paired profile differs')
  require(BINARIES<=set(z.namelist()),'Missing paired runtime files')
  require(integration['game_tested'] is False and integration['multiplayer_tested'] is False,'Update validation scope differs')
  for name in ready['required_reports']:
   data=z.read('diagnostics/'+name+'.json');report=json.loads(data)
   require(report.get('passed') is True or report.get('pass') is True or report.get('status')=='PASS','Nonpassing report: '+name)
   require(sha(data)==manifest['reports'][name]['sha256'],'Report digest differs: '+name)
   tested=report.get('tested_source_sha256');require(isinstance(tested,dict) and tested,'Report has no tested source binding: '+name)
   for path,wanted in tested.items():require(path in hashes and hashes[path]==wanted,'Report does not match compiled source: '+name+'/'+path)
  for row in manifest['paired_binaries']:require(sha(z.read(row['name']))==row['sha256'],'Paired binary bytes differ')
 with zipfile.ZipFile(PACKAGE/source['name']) as z:
  for name,wanted in hashes.items():require(sha(z.read('source/'+name))==wanted,'Corresponding source differs: '+name)
  for name,row in manifest['source_files'].items():require(sha(z.read(name))==row['sha256'],'Source archive member differs: '+name)
 return ready

require(os.environ.get('GH_REPO')==REPO,'Unexpected repository')
require(os.environ.get('GITHUB_REF')=='refs/heads/'+BRANCH,'Unexpected release branch')
COMMIT=os.environ['GITHUB_SHA'];require(re.fullmatch('[0-9a-f]{40}',COMMIT) is not None,'Invalid publication commit')
require(subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()==COMMIT,'Checkout differs from publication commit')
ready=validate_local();record('verified-ready.json',ready)
require(tag_sha() in (None,COMMIT),'Refusing to move an existing release tag')
pages=json.loads(subprocess.check_output(['gh','api','--paginate',f'repos/{REPO}/releases?per_page=100','--slurp']))
matches=[r for page in pages for r in page if r['tag_name']==TAG];require(len(matches)<=1,'Ambiguous existing release')
request=dict(tag_name=TAG,target_commitish=COMMIT,draft=True,prerelease=True,make_latest='false',name='紅警工具箱 '+VERSION+' — 建築動畫、星級與副相機修復',body=(PACKAGE/'RELEASE_NOTES.md').read_text(encoding='utf-8'))
if matches:
 release=matches[0]
 require(release['draft'] and release['prerelease'] and release['target_commitish']==COMMIT,'Refusing to alter a published or unrelated release')
 require(release['author']['login']=='github-actions[bot]','Existing draft belongs to another author')
 require({x['name'] for x in release['assets']}<=NAMES,'Existing draft has unexpected assets')
else:release=api(f'repos/{REPO}/releases','POST',request)
require(release['draft'] and release['prerelease'] and release['target_commitish']==COMMIT,'Draft identity differs')
record('release-draft.json',release);expected={x['name']:x for x in ready['assets']};existing={x['name']:x for x in release['assets']}
for name,item in expected.items():
 if name in existing:asset=existing[name]
 else:
  endpoint=release['upload_url'].split('{')[0]+'?name='+quote(name)
  asset=json.loads(subprocess.check_output(['gh','api','--method','POST',endpoint,'-H','Content-Type: application/octet-stream','--input',str(PACKAGE/name)]))
 require(asset['state']=='uploaded' and asset['size']==item['bytes'],'Uploaded asset state/length differs: '+name)
 require(asset.get('digest') in (None,'sha256:'+item['sha256']),'Uploaded asset digest differs: '+name)
current=api(f'repos/{REPO}/releases/{release["id"]}')
require(current['draft'] and current['prerelease'] and current['target_commitish']==COMMIT,'Draft changed during upload')
require({x['name'] for x in current['assets']}==NAMES,'Draft asset membership differs')
download=OUT/'downloaded';download.mkdir(exist_ok=True)
for asset in current['assets']:
 name=asset['name'];data=subprocess.check_output(['gh','api',f'repos/{REPO}/releases/assets/{asset["id"]}','-H','Accept: application/octet-stream'])
 require(len(data)==expected[name]['bytes'] and sha(data)==expected[name]['sha256'],'Downloaded asset digest differs: '+name)
 (download/name).write_bytes(data)
require(tag_sha() in (None,COMMIT),'Release tag changed during validation')
published=api(f'repos/{REPO}/releases/{release["id"]}','PATCH',{'draft':False,'prerelease':True,'make_latest':'false'})
require(not published['draft'] and published['prerelease'] and published['tag_name']==TAG and published['target_commitish']==COMMIT,'Published identity differs')
require(tag_sha()==COMMIT,'Published tag differs')
record('published-release.json',published);record('tag-commit.json',{'tag':TAG,'publication_commit':COMMIT,'compiled_source_commit':ready['compiled_source_commit']})
with open(os.environ['GITHUB_STEP_SUMMARY'],'a',encoding='utf-8') as summary:
 summary.write(published['html_url']+'\n\nUpdate, corresponding Source and release manifest were downloaded and SHA-256 verified before prerelease publication. The stable latest release was not changed.\n')
print(published['html_url'])
