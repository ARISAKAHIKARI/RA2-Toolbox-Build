"""Exercise passenger toggle ownership and production INI statements."""
from pathlib import Path
import hashlib,json,re,subprocess,sys
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
ui=root/'assist/source/auto_settings_ui.h';io=root/'assist/source/extra_settings_io.h'
s=ui.read_text();body=s[s.index(' if((id==897'):s.index(' if(id==9065')]
text=io.read_text();fields=['passengerBubbles','dangerPassengers','passengerOwn','passengerAlways']
load=''.join(re.search(r'extra\.'+f+r'=value\([^;]+;',text).group() for f in fields)
save=''.join(re.search(r'write\(L"Display",L"[^"]+",extra\.'+f+r'\);',text).group() for f in fields)
cpp=r'''
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
static struct{bool passengerBubbles=false,dangerPassengers=false,passengerOwn=false,passengerAlways=false,magnetronDrop=false;}extra;
static std::map<int,int> fields;
static std::map<std::wstring,int> profile;
constexpr int BN_CLICKED=1,BM_GETCHECK=2,BST_CHECKED=1;
static int ui=1,changes=0,syncs=0;
static int GetDlgItem(int,int id){return id;}
static int SendMessageW(int id,int,int,int){return fields[id];}
static void changedSetting(){++changes;}
static void syncAdditional(){++syncs;}
static int value(const wchar_t*,const wchar_t*k,int fallback){auto i=profile.find(k);return i==profile.end()?fallback:i->second;}
static void write(const wchar_t*,const wchar_t*k,int v){profile[k]=v;}
'''
cpp+='static bool edit(int id,int notification){'+body+'return false;}\n'
cpp+='static void load(){'+load+'}\nstatic void save(){'+save+'}\n'
cpp+=r'''
int main(){load();assert(!extra.passengerBubbles&&!extra.dangerPassengers&&!extra.passengerOwn&&!extra.passengerAlways);
 for(auto p:{std::pair<int,bool*>{897,&extra.passengerBubbles},{898,&extra.dangerPassengers},{9498,&extra.passengerOwn},{9499,&extra.passengerAlways}}){
  for(bool on:{false,true}){int before=changes;fields[p.first]=on;assert(edit(p.first,BN_CLICKED));assert(*p.second==on&&changes==before+1&&syncs==changes);save();*p.second=!on;load();assert(*p.second==on);}
 }
 assert(profile.size()==4);std::puts("PASS passenger settings: four independent toggles, one owner notification, production INI off/on roundtrips");
}
'''
(out/'test.cpp').write_text(cpp)
runs=[]
for cmd in [['g++','-std=c++17',str(out/'test.cpp'),'-o',str(out/'test')],[str(out/'test')]]:
 r=subprocess.run(cmd,capture_output=True,text=True);runs.append({'command':cmd,'exit_code':r.returncode,'output':r.stdout+r.stderr});print(r.stdout+r.stderr,end='')
 if r.returncode:break
(out/'result.json').write_text(json.dumps({'passed':len(runs)==2 and all(r['exit_code']==0 for r in runs),'runs':runs,'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ui,io,Path(__file__)]}},indent=2)+'\n')
raise SystemExit(0 if len(runs)==2 and all(r['exit_code']==0 for r in runs) else 1)
