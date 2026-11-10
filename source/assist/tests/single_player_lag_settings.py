"""Execute production lag toggles, masked INI roundtrip and shared publication."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]/'source'
io=(root/'extra_settings_io.h').read_text();ui=(root/'auto_settings_ui.h').read_text()
load=re.search(r'extra\.singlePlayerLagFix=[^;]+;',io).group()
save=re.search(r'write\(L"SinglePlayer",L"LagFix",[^;]+;',io).group()
sync=re.search(r'shared->singlePlayerLagFix=[^;]+;',io).group()
handler=next(line for line in ui.splitlines() if 'id==9538||id==9539' in line)
code=r'''
#include "additional_settings.h"
#include <cassert>
#include <map>
#include <string>
#include <iostream>
using namespace assist;static AdditionalSettings extra;
static struct{uint32_t singlePlayerLagFix=0;}wire;static auto*shared=&wire;
static std::map<int,int>fields;static int ui=1,changed=0,wakes=0,profile=0;static std::wstring notice;
constexpr int BN_CLICKED=1,BM_GETCHECK=2,BST_CHECKED=1;
static int GetDlgItem(int,int id){return id;}static int SendMessageW(int id,int,int,int){return fields[id];}
static void changedSetting(){++changed;}static void wakeGame(){++wakes;}static void notify(const std::wstring&s){notice=s;}
static int value(const wchar_t*,const wchar_t*,int){return profile;}static void write(const wchar_t*,const wchar_t*,int v){profile=v;}
'''
code+='static void load(){'+load+'}\nstatic void save(){'+save+'}\nstatic void syncAdditional(){'+sync+'}\nstatic bool edit(int id,int notification){'+handler+'return false;}\n'
code+=r'''
int main(){
 assert(!extra.singlePlayerLagFix);load();assert(!extra.singlePlayerLagFix);
 for(int initial=0;initial<4;++initial)for(int id:{9538,9539})for(bool on:{true,false}){
  extra.singlePlayerLagFix=initial;fields[id]=on;const unsigned bit=id==9538?1:2,expected=on?(initial|bit):(initial&~bit);
  int before=changed;assert(edit(id,BN_CLICKED));assert(changed==before+1&&wakes==changed&&!notice.empty());
  assert(extra.singlePlayerLagFix==expected&&wire.singlePlayerLagFix==expected);
  save();extra.singlePlayerLagFix=123;load();assert(extra.singlePlayerLagFix==expected);
 }
 for(int bad:{-1,4,7,255}){profile=bad;load();syncAdditional();assert(extra.singlePlayerLagFix==(uint32_t(bad)&3)&&wire.singlePlayerLagFix==(uint32_t(bad)&3));}
 std::cout<<"PASS: independent lag toggles, off defaults, INI masks, persistence, feedback and Runtime publication\n";
}
'''
cpp=a.out/'settings.cpp';exe=a.out/'settings';cpp.write_text(code)
subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I'+str(root),str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
