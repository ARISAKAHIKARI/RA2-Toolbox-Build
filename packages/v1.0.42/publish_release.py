"""Publish only the exact, verified v1.0.42 build and packages."""
from pathlib import Path
import hashlib,json,os,re,subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'publication';OUT.mkdir(exist_ok=True)
DIST=ROOT/'dist'
REPO=os.environ['GH_REPO']
COMMIT=os.environ['GITHUB_SHA']
TAG='v1.0.42'
NAMES={'RA2Toolbox-1.0.42-x64-Full.zip','RA2Toolbox-1.0.42-x64-Update.zip','release-manifest.json','SHA256SUMS.txt'}
def require(ok,why):
 if not ok:raise ValueError(why)
def api(endpoint,method='GET',data=None):
 command=['gh','api','--method',method,endpoint]
 if data is not None:
  request=OUT/'request.json';request.write_text(json.dumps(data),encoding='utf-8')
  command+=['--input',str(request)]
 return json.loads(subprocess.check_output(command))
def record(name,value):
 (OUT/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def tag_sha():
 result=subprocess.run(['gh','api',f'repos/{REPO}/git/ref/tags/{TAG}'],text=True,capture_output=True)
 if result.returncode==0:return api(f'repos/{REPO}/commits/{TAG}')['sha']
 require(re.search(r'\(HTTP 404\)',result.stderr) is not None,'Cannot verify tag: '+result.stderr)
 return None

require(REPO=='ARISAKAHIKARI/RA2-Toolbox-Build','Unexpected repository')
require(os.environ['GITHUB_REF']=='refs/heads/codex/release-v1.0.42-20261106','Unexpected release branch')
require(subprocess.check_output(['git','rev-parse','HEAD'],text=True,cwd=ROOT).strip()==COMMIT,'Checkout differs')
require({p.name for p in DIST.iterdir()}==NAMES,'Unexpected distribution files')
manifest=json.loads((DIST/'release-manifest.json').read_text())
require(manifest['source_commit']==COMMIT and manifest['version']=='1.0.42' and manifest['file_version']=='1.0.42.0','Package identity differs')
require(manifest['baseline_commit']=='e00b842e81676b41e85a2db4e8ad67cf60ca424b' and manifest['assist_protocol']==96 and manifest['assist_shared_bytes']==23141840 and not manifest['full_game_tested'] and not manifest['multiplayer_tested'],'Package validation scope differs')
subprocess.run(['sha256sum','-c','SHA256SUMS.txt'],cwd=DIST,check=True)
expected={n:{'bytes':(DIST/n).stat().st_size,'sha256':hashlib.sha256((DIST/n).read_bytes()).hexdigest()} for n in sorted(NAMES)}
record('expected-assets.json',expected)
prior_tag=tag_sha();require(prior_tag in (None,COMMIT),'Refusing to move an existing tag')
pages=json.loads(subprocess.check_output(['gh','api','--paginate',f'repos/{REPO}/releases?per_page=100','--slurp']))
matches=[r for page in pages for r in page if r['tag_name']==TAG]
require(len(matches)<=1,'Ambiguous release')
request=dict(tag_name=TAG,target_commitish=COMMIT,draft=True,prerelease=False,make_latest='false',name='RA2 Toolbox 1.0.42',body=(Path(__file__).with_name('RELEASE_NOTES.md')).read_text())
if matches:
 release=matches[0]
 require(release['draft'] and not release['prerelease'] and release['target_commitish']==COMMIT,'Refusing to alter another or published release')
 require(release['author']['login']=='github-actions[bot]','Unexpected draft owner')
 require({a['name'] for a in release['assets']}<=NAMES,'Unexpected draft assets')
else:
 release=api(f'repos/{REPO}/releases','POST',request)
require(release['draft'] and not release['prerelease'] and release['target_commitish']==COMMIT,'Draft identity differs')
record('release-draft.json',release)
existing={a['name']:a for a in release['assets']}
from urllib.parse import quote
for name,row in expected.items():
 if name in existing:
  asset=existing[name]
 else:
  endpoint=release['upload_url'].split('{')[0]+'?name='+quote(name)
  asset=json.loads(subprocess.check_output(['gh','api','--method','POST',endpoint,'-H','Content-Type: application/octet-stream','--input',str(DIST/name)]))
 require(asset['state']=='uploaded' and asset['size']==row['bytes'] and asset.get('digest')=='sha256:'+row['sha256'],'Uploaded asset differs: '+name)
release=api(f'repos/{REPO}/releases/{release["id"]}')
require(release['draft'] and not release['prerelease'] and release['target_commitish']==COMMIT,'Draft changed')
require({a['name'] for a in release['assets']}==NAMES,'Asset membership differs')
download=OUT/'downloaded';download.mkdir(exist_ok=True)
for asset in release['assets']:
 name=asset['name'];path=download/name
 with path.open('wb')as output:
  subprocess.run(['gh','api',f'repos/{REPO}/releases/assets/{asset["id"]}','-H','Accept: application/octet-stream'],stdout=output,check=True)
 require(path.stat().st_size==expected[name]['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==expected[name]['sha256'],'Downloaded digest differs: '+name)
subprocess.run(['sha256sum','-c','SHA256SUMS.txt'],cwd=download,check=True)
require(tag_sha() in (None,COMMIT),'Tag changed during publication')
published=api(f'repos/{REPO}/releases/{release["id"]}','PATCH',{'draft':False,'prerelease':False,'make_latest':'true'})
require(not published['draft'] and not published['prerelease'] and published['tag_name']==TAG and published['target_commitish']==COMMIT,'Publication differs')
require(tag_sha()==COMMIT,'Published tag differs')
latest=api(f'repos/{REPO}/releases/latest')
require(latest['id']==published['id'],'Release was not marked latest')
record('published-release.json',published)
record('tag-commit.json',{'tag':TAG,'commit':COMMIT})
with open(os.environ['GITHUB_STEP_SUMMARY'],'a',encoding='utf-8') as output:
 output.write(published['html_url']+'\n\nFull and Update were built from this commit, downloaded and SHA-256 verified before stable publication.\n')
print(published['html_url'])

