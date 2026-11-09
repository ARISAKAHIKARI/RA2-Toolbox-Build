"""Production toggle, INI and shared publication roundtrips with UI boundaries."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]/'source';io=(root/'extra_settings_io.h').read_text();ui=(root/'auto_settings_ui.h').read_text()
fields=['keepInputUnlocked','bombBoardEnabled','bombBoardRangeCells']
loads=''.join(re.search(r'extra\.'+f+r'=[^;]+;',io).group() for f in fields)
saves=''.join(re.search(r'write\(L"[^"]+",L"[^"]+",extra\.'+f+r'\);',io).group() for f in fields)
sync=''.join(re.search(r'shared->'+f+r'=[^;]+;',io).group() for f in fields)
handlers='\n'.join(re.search(r' if\(id=='+str(i)+r'&&notification==BN_CLICKED\)\{[^\n]+',ui).group() for i in [9530,9536])
code=r'''
#include "additional_settings.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <iostream>
using namespace assist;static AdditionalSettings extra;
static struct{bool keepInputUnlocked=false,bombBoardEnabled=false;unsigned bombBoardRangeCells=0;}runtime;static auto*shared=&runtime;
static std::map<std::wstring,int>profile;static std::map<int,int>fields;static int ui=1,changed=0,wakes=0;static std::wstring notice;
constexpr int BN_CLICKED=1,BM_GETCHECK=2,BST_CHECKED=1;
static int GetDlgItem(int,int id){return id;}static int SendMessageW(int id,int,int,int){return fields[id];}
static void changedSetting(){++changed;}static void wakeGame(){++wakes;}static void notify(const std::wstring&s){notice=s;}
static std::wstring key(const wchar_t*s,const wchar_t*k){return std::wstring(s)+L"."+k;}
static int value(const wchar_t*s,const wchar_t*k,int fallback){auto i=profile.find(key(s,k));return i==profile.end()?fallback:i->second;}
static void write(const wchar_t*s,const wchar_t*k,int v){profile[key(s,k)]=v;}
'''
code+='static void syncAdditional(){'+sync+'}\nstatic void load(){'+loads+'}\nstatic void save(){'+saves+'}\nstatic bool edit(int id,int notification){'+handlers+'return false;}\n'
code+=r'''
int main(){
 load();assert(!extra.keepInputUnlocked&&!extra.bombBoardEnabled&&extra.bombBoardRangeCells==8);
 for(int id:{9530,9536})for(bool on:{true,false}){int before=changed;fields[id]=on;assert(edit(id,BN_CLICKED));assert(changed==before+1&&wakes==changed);save();extra.keepInputUnlocked=extra.bombBoardEnabled=!on;load();syncAdditional();assert(id==9530?(extra.bombBoardEnabled==on&&runtime.bombBoardEnabled==on):(extra.keepInputUnlocked==on&&runtime.keepInputUnlocked==on));if(id==9536)assert(!notice.empty());}
 for(int input:{-1,0,1,8,64,65,1000}){profile[L"BombBoard.RangeCells"]=input;load();syncAdditional();assert(extra.bombBoardRangeCells==std::clamp(input,1,64)&&runtime.bombBoardRangeCells==unsigned(extra.bombBoardRangeCells));save();assert(profile[L"BombBoard.RangeCells"]==extra.bombBoardRangeCells);}
 std::cout<<"PASS bomb/input settings: independent defaults, production toggle owner, persistence, shared publication, radius bounds and visible feedback\n";
}
'''
cpp=args.out/'settings.cpp';exe=args.out/'settings';cpp.write_text(code)
subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I'+str(root),str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
