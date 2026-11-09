// Profile-storage boundary only; see profile-result.json and the research note.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include "runtime_feature_demand.h"
#include "hook_policy.h"
using namespace assist;
static std::map<std::wstring,int> ini;
static bool hookAdvanced=false,hookSmartUnload=false;
static int value(const wchar_t*s,const wchar_t*k,int fallback){auto i=ini.find(std::wstring(s)+L"/"+k);return i==ini.end()?fallback:i->second;}
static void write(const wchar_t*s,const wchar_t*k,int v){ini[std::wstring(s)+L"/"+k]=v;}
static void oldLoad(){hookAdvanced=value(L"Hooks",L"Advanced",value(L"NativeOrders",L"EnableInputHooks",1))!=0;}
static void oldSaveHooks(){write(L"Hooks",L"Advanced",hookAdvanced);write(L"Hooks",L"SmartUnload",hookSmartUnload);}
static void newLoad(){hookAdvanced=value(L"Hooks",L"Advanced",value(L"NativeOrders",L"EnableInputHooks",0))!=0;}
static unsigned checks=0;
static void check(bool value){++checks;if(!value)std::exit(1);}
static uint32_t flags(){return hookpolicy::resolve({hookAdvanced,false,false,false},true,false,false);}
static void show(const char*name,const AdditionalSettings&extra){std::printf("%s advanced=%u ownGround=%u flags=0x%08X demand=%u\n",name,unsigned(hookAdvanced),extra.missionModeKeys[selectedmode::OwnGround],flags(),unsigned(runtimeFeatureDemand(flags(),extra,false,false)));}
int main(){
 AdditionalSettings extra;
 ini.clear();newLoad();check(!hookAdvanced);check(extra.missionModeKeys[selectedmode::OwnGround]==uint32_t('F'|(6<<8)));check(runtimeFeatureDemand(flags(),extra,false,false));show("current_fresh_defaults",extra);
 extra.missionModeKeys[selectedmode::OwnGround]=0;check(!runtimeFeatureDemand(flags(),extra,false,false));show("current_manual_only_ownGround_unbound",extra);
 ini.clear();oldLoad();check(hookAdvanced);oldSaveHooks();check(ini[L"Hooks/Advanced"]==1);
 hookAdvanced=false;newLoad();check(hookAdvanced);check(flags()==uint32_t(AdvancedInput|NativeOrders));check(runtimeFeatureDemand(flags(),extra,false,false));show("current_inherits_v135_hook_default",extra);
 ini.clear();ini[L"Hooks/Advanced"]=0;oldLoad();check(!hookAdvanced);oldSaveHooks();hookAdvanced=true;newLoad();check(!hookAdvanced);check(!runtimeFeatureDemand(flags(),extra,false,false));show("v135_preserves_explicit_advanced_off",extra);
 std::printf("PASS v135 profile demand boundary: 4 cases, %u checks; profile API mocked, production load/save statements and demand headers\n",checks);
}
