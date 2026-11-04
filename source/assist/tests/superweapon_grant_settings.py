"""Execute the actual switch handler and INI/Shared statements with UI/IO seams."""
import argparse,hashlib,json,re,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[2];src=root/'assist/source'
io=(src/'extra_settings_io.h').read_text();ui=(src/'auto_settings_ui.h').read_text()
def block(s,key):
 start=s.index(key);at=s.index('{',start)+1;depth=1
 while depth:depth+=(s[at]=='{')-(s[at]=='}');at+=1
 return s[start:at]
branch=block(ui,'if(id==9490&&notification==BN_CLICKED)')
load=re.search(r'extra\.superweaponGrant=[^;]+;',io)[0]
save=re.search(r'write\(L"Experimental",L"SuperweaponPermanentGrant",[^;]+;',io)[0]
sync=re.search(r'shared->superweaponGrant=[^;]+;',io)[0]
cpp=r'''
#include "runtime_feature_demand.h"
#include "../../suite/source/settings_categories.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
using namespace assist;
static AdditionalSettings extra;static Shared wire,*shared=&wire;
static std::map<std::pair<std::wstring,std::wstring>,int> ini;
static int ui=1,checked=0,changes=0,syncs=0;
static constexpr int BN_CLICKED=0,BM_GETCHECK=1,BST_CHECKED=1;
static int value(const wchar_t*s,const wchar_t*k,int fallback){auto i=ini.find({s,k});return i==ini.end()?fallback:i->second;}
static void write(const wchar_t*s,const wchar_t*k,int v){ini[{s,k}]=v;}
static void load(){LOAD}
static void save(){SAVE}
static void changedSetting(){++changes;}
static void syncAdditional(){SYNC ++syncs;}
static int GetDlgItem(int,int id){assert(id==9490);return id;}
static int SendMessageW(int control,int message,int,int){assert(control==9490&&message==BM_GETCHECK);return checked;}
static bool route(int id,int notification){BRANCH return false;}
int main(){unsigned checks=0;auto check=[&](bool b){assert(b);++checks;};
 check(!extra.superweaponGrant);load();syncAdditional();check(!extra.superweaponGrant&&!wire.superweaponGrant);
 check(!runtimeFeatureDemand(0,extra,false,false));
 check(!route(9489,BN_CLICKED)&&!route(9490,99)&&!changes);
 for(int on:{1,0,1,0}){checked=on;const int c=changes,s=syncs;
  check(route(9490,BN_CLICKED));check(extra.superweaponGrant==bool(on)&&wire.superweaponGrant==unsigned(on)&&changes==c+1&&syncs==s+1);
  check(runtimeFeatureDemand(0,extra,false,false)==bool(on));save();extra.superweaponGrant=!on;load();check(extra.superweaponGrant==bool(on));
 }
 int ids[]={9490};check(suite::categories::row(4,ids,1)==suite::categories::Experimental);
 check(Version==95);std::printf("PASS superweapon settings checks=%u\n",checks);
}
'''.replace('LOAD',load).replace('SAVE',save).replace('SYNC',sync).replace('BRANCH',branch)
cpp_path=a.out/'settings.cpp';exe=a.out/'settings';cpp_path.write_text(cpp)
subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I'+str(src),str(cpp_path),'-o',str(exe)],check=True)
run=subprocess.run([str(exe)],capture_output=True,text=True);print(run.stdout,end='');assert run.returncode==0,run.stderr
files=['additional_settings.h','extra_settings_io.h','auto_settings_ui.h','protocol.h','runtime_feature_demand.h']
report=dict(passed=True,scope=__doc__,source_sha256={n:hashlib.sha256((src/n).read_bytes()).hexdigest()for n in files},stdout=run.stdout,real_windows_ui=False)
(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
