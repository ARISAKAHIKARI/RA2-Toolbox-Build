"""Exercise actual INI assignments and display publication with controlled UI stubs."""
from pathlib import Path
import argparse,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]/'source';source=(root/'main.cpp').read_text();io=(root/'extra_settings_io.h').read_text()
fields=['revealCloakedUnits','revealMirageUnits','revealEnemySpies']
loads=''.join(re.search(r'\b'+f+r'=[^;]+;',io).group() for f in fields)
saves=''.join(re.search(r'write\(L"Display",L"[^"]+",'+f+r'\);',io).group() for f in fields)
publish=re.search(r'profile.revealUnits=[^;]+;',source).group()
handlers=re.search(r'else if\(id==9760\|\|id==9761\|\|id==9762\)\{[^}]+\}',source).group()
code='''#include <cassert>
#include <map>
#include <string>
#include <iostream>
#include "render_profile.h"
#include "display_settings.h"
static bool revealCloakedUnits=false,revealMirageUnits=false,revealEnemySpies=false;
static std::map<std::wstring,int> ini;
static int value(const wchar_t*,const wchar_t*k,int def){auto i=ini.find(k);return i==ini.end()?def:i->second;}
static void write(const wchar_t*,const wchar_t*k,int v){ini[k]=v;}
'''+f'void load(){{{loads}}}\nvoid save(){{{saves}}}\nvoid toggle(int id,bool on){{if(false){{}}{handlers}}}\n'+f'external::presentation::Profile publish(){{external::presentation::Profile profile;{publish}return profile;}}\n'+r'''
int main(){load();assert(publish().revealUnits==0);
 external::presentation::Settings shared;
 for(unsigned bits=0;bits<8;++bits){for(int i=0;i<3;++i)toggle(9760+i,bits&(1u<<i));save();ini[L"RevealCloakedUnits"]=bits&1;ini[L"RevealMirageUnits"]=bits&2;ini[L"RevealEnemySpies"]=bits&4;toggle(9760,false);toggle(9761,false);toggle(9762,false);load();auto profile=publish();assert(profile.revealUnits==0);shared.publish(profile);assert(shared.get().profile.revealUnits==0);}
 assert(suite::displaySpecs[suite::RevealCloakedUnits].id==9760&&suite::displaySpecs[suite::RevealMirageUnits].id==9761&&suite::displaySpecs[suite::RevealEnemySpies].id==9762);
 std::cout<<"settings: 8 legacy INI/button combinations cannot enable suspended model rendering\n";
}
'''
cpp=a.out/'settings.cpp';exe=a.out/'settings';cpp.write_text(code)
subprocess.run(['g++','-std=c++17','-O1','-pthread','-I'+str(root.parents[1]/'external_reader/source'),'-I'+str(root.parents[1]/'suite/source'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
(a.out/'settings-result.json').write_text(json.dumps({'passed':True,'independent_combinations':8,'live_UI_tested':False},indent=2)+'\n')
