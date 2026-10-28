"""Execute the shipped render-control handler and its actual INI statements."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
main = (root/'assist/source/main.cpp').read_text(encoding='utf-8')
io = (root/'assist/source/extra_settings_io.h').read_text(encoding='utf-8')
globals_ = next(line for line in main.splitlines() if line.startswith('static bool renderUpgradeEnabled='))
load = io.split('static void loadAdditionalSettings(){', 1)[1].split(' extra.viewZoomEnabled=', 1)[0]
save = '\n'.join(line for line in io.splitlines() if line.lstrip().startswith('write(L"Display",') and ('L"Render' in line or 'L"UnifiedOverlay"' in line))
handler = main.split(' if(id==9300||', 1)[1].split('\n if(id==862', 1)[0]
handler = ' if(id==9300||'+handler
fixture = r'''
#include <algorithm>
#include <cassert>
#include <map>
#include <string>
static std::map<std::wstring,int> profile;static std::map<int,int> controls;
static bool updatingControls=false;static int paintRevision=0,changes=0,stops=0,ui=1;
constexpr int CBN_SELCHANGE=1,BN_CLICKED=2,CB_GETCURSEL=3,BM_GETCHECK=4,BST_CHECKED=1;
static int HIWORD(unsigned v){return int(v>>16);}
static int SendDlgItemMessageW(int,int id,int,int,int){return controls[id];}
static int GetDlgItem(int,int id){return id;}
static void SetWindowTextW(int,const wchar_t*){}
static void changedSetting(){++changes;}static void paint(){}
namespace renderupgrade {static void stop(){++stops;}}
static int value(const wchar_t*,const wchar_t*key,int fallback){auto i=profile.find(key);return i==profile.end()?fallback:i->second;}
static void write(const wchar_t*,const wchar_t*key,int v){profile[key]=v;}
'''+globals_+'\nstatic void load(){'+load+'}\nstatic void save(){'+save+'}\nstatic int handle(int id,unsigned wp){int w=1;'+handler+'return -1;}\n'+r'''
int main(){
 load();assert(!renderUpgradeEnabled&&!renderPhysicalOutput&&renderMaterialRefresh&&renderSceneDetail==1&&renderPixelScale==3);
 for(int detail:{0,1})for(int refresh:{0,1})for(int scale:{0,1,2,3}){
  controls[9308]=detail;handle(9308,CBN_SELCHANGE<<16);controls[9305]=scale;handle(9305,CBN_SELCHANGE<<16);
  controls[9307]=refresh;handle(9307,BN_CLICKED<<16);
  assert(renderSceneDetail==detail&&renderPixelScale==scale&&renderMaterialRefresh==bool(refresh));
  save();renderSceneDetail=9;renderPixelScale=-1;renderMaterialRefresh=!refresh;load();
  assert(renderSceneDetail==detail&&renderPixelScale==scale&&renderMaterialRefresh==bool(refresh));
 }
 for(int id:{9302,9304,9305,9308})for(int invalid:{-1,4,999}){
  int revision=paintRevision,count=changes,stopped=stops;controls[id]=invalid;handle(id,CBN_SELCHANGE<<16);
  assert(paintRevision==revision&&changes==count&&stops==stopped);
 }
 updatingControls=true;controls[9305]=0;int old=renderPixelScale;handle(9305,CBN_SELCHANGE<<16);assert(renderPixelScale==old);updatingControls=false;
 for(int id:{9300,9301,9306}){controls[id]=1;handle(id,BN_CLICKED<<16);}save();load();assert(renderUpgradeEnabled&&unifiedOverlayEnabled&&renderPhysicalOutput);
 profile[L"RenderPixelScale"]=99;profile[L"RenderSceneDetail"]=-1;load();assert(renderPixelScale==3&&renderSceneDetail==0);
}
'''
with tempfile.TemporaryDirectory() as temp:
    source=Path(temp)/'settings.cpp';binary=Path(temp)/'settings'
    source.write_text(fixture,encoding='utf-8')
    subprocess.run(['g++','-std=c++17','-O2',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: actual render control events and INI load/save, 16 detail/refresh/scale combinations, adaptive mode 3 retained, invalid/programmatic events ignored')
