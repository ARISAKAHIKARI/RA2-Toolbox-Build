"""Execute actual 9470 routing and INI/Shared assignments with bounded UI/IO models."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True,type=Path);p.add_argument('--build-dir',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();here=Path(__file__).resolve().parent;repo=here.parents[2];src=here.parent/'source';a.build_dir.mkdir(parents=True,exist_ok=True)
paths=['source/assist/source/additional_settings.h','source/assist/source/extra_settings_io.h','source/assist/source/auto_settings_ui.h','source/assist/tests/tech_enhancement_parameter_settings.py'];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={n:sha(repo/n)for n in paths};io=(src/'extra_settings_io.h').read_text(encoding='utf-8');ui=(src/'auto_settings_ui.h').read_text(encoding='utf-8');settings=(src/'additional_settings.h').read_text(encoding='utf-8')
def block(source,needle):
 start=source.index(needle);at=source.index('{',start)+1;depth=1
 while depth:depth+=(source[at]=='{')-(source[at]=='}');at+=1
 return source[start:at]
load=re.search(r'extra\.techEnhanceImplementation=std::clamp\(value\([^\n;]+;',io)[0];save=re.search(r'write\(L"Experimental",L"TechEnhanceImplementation",[^;]+;',io)[0];sync=re.search(r'shared->techEnhanceImplementation=[^;]+;',io)[0];field=re.search(r'int techEnhanceImplementation=[^;]+;',settings)[0];branch=block(ui,'if(id==9470&&select)')
cpp=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#ifdef NDEBUG
#error Assertions must run.
#endif
struct Settings {FIELD bool techEnhance=false;} extra;
struct Shared {uint32_t techEnhanceImplementation=99;} wire,*shared=&wire;
static std::map<std::pair<std::wstring,std::wstring>,int> ini;static int writes=0,syncs=0,selected=0,changes=0;
static int value(const wchar_t*section,const wchar_t*key,int fallback){auto i=ini.find({section,key});return i==ini.end()?fallback:i->second;}
static void write(const wchar_t*section,const wchar_t*key,int number){ini[{section,key}]=number;++writes;}
static void load(){LOAD}
static void save(){SAVE}
static void syncAdditional(){SYNC ++syncs;}
static void changedSetting(){++changes;save();}
static constexpr int CB_GETCURSEL=1;static int ui=1;
static int GetDlgItem(int,int id){assert(id==9470);return id;}
static intptr_t SendMessageW(int control,int message,int,int){assert(control==9470&&message==CB_GETCURSEL);return selected;}
static bool route(int id,bool select){BRANCH return false;}
int main(){int checks=0;auto check=[&](bool ok){++checks;assert(ok);};load();check(extra.techEnhanceImplementation==0);syncAdditional();check(wire.techEnhanceImplementation==0);
 for(int mode:{0,1}){extra.techEnhanceImplementation=mode;save();extra.techEnhanceImplementation=9;load();check(extra.techEnhanceImplementation==mode);syncAdditional();check(wire.techEnhanceImplementation==unsigned(mode));}
 ini[{L"Experimental",L"TechEnhanceImplementation"}]=99;load();check(extra.techEnhanceImplementation==1);ini[{L"Experimental",L"TechEnhanceImplementation"}]=-4;load();check(extra.techEnhanceImplementation==0);
 int old=changes;check(!route(9470,false)&&!route(9469,true)&&changes==old);
 for(int pick:{0,1,99,-1}){selected=pick;const int expected=std::clamp(pick,0,1);int c=changes,s=syncs;check(route(9470,true));check(extra.techEnhanceImplementation==expected&&wire.techEnhanceImplementation==unsigned(expected));check(changes==c+1&&syncs==s+1&&ini[{L"Experimental",L"TechEnhanceImplementation"}]==expected);}
 check(!extra.techEnhance);printf("TechEnhance settings checks=%d; actual 9470/load/save/sync; UI/IO modeled\n",checks);return 0;
}
'''
for key,val in {'FIELD':field,'LOAD':load,'SAVE':save,'SYNC':sync,'BRANCH':branch}.items():cpp=cpp.replace(key,val)
cpp_path=a.build_dir/'tech-enhance-settings.cpp';cpp_path.write_text(cpp,encoding='utf-8');results=[]
for target in ('x86-windows-gnu','x86_64-windows-gnu'):
 exe=a.build_dir/(target+'.exe');flags=['-target',target,'-std=c++17','-O2','-UNDEBUG'];subprocess.run([str(a.zig.resolve()),'c++',*flags,str(cpp_path.resolve()),'-o',str(exe.resolve())],check=True);r=subprocess.run([str(exe.resolve())],capture_output=True,text=True);print(r.stdout,end='');assert r.returncode==0,r.stderr;m=re.search(r'checks=(\d+)',r.stdout);assert m and int(m[1])==22;results.append({'target':target,'checks':int(m[1]),'exe_sha256':sha(exe),'stdout':r.stdout,'flags':flags})
assert before=={n:sha(repo/n)for n in paths};a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'pass':True,'scope':__doc__,'source_sha256':before,'production_code_extracted':True,'assertions_enabled':True,'results':results,'game_process_tested':False},indent=2)+'\n',encoding='utf-8')
