#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "../../shared/source/debug_entry_policy.h"

using LONG=int32_t;
static constexpr int FALSE=0;
static std::map<std::pair<std::wstring,std::wstring>,std::wstring> ini;
static unsigned checks=0,writes=0,barriers=0;
static bool failWrite=false,saveWriteOK=true;
static std::vector<LONG> publishedSequence;
static void check(bool ok,const char*message){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
static std::wstring path(const wchar_t*name){return std::wstring(L"fixture/")+name;}
static unsigned GetPrivateProfileStringW(const wchar_t*section,const wchar_t*key,const wchar_t*fallback,wchar_t*out,unsigned size,const wchar_t*){
 const auto found=ini.find({section,key});const std::wstring text=found==ini.end()?fallback:found->second;
 const unsigned count=unsigned(text.size())<size-1?unsigned(text.size()):size-1;
 for(unsigned i=0;i<count;++i)out[i]=text[i];out[count]=0;return count;
}
static int WritePrivateProfileStringW(const wchar_t*section,const wchar_t*key,const wchar_t*value,const wchar_t*){
 ++writes;if(failWrite){failWrite=false;return 0;}ini[{section,key}]=value;return 1;
}
static LONG InterlockedIncrement(volatile LONG*p){const LONG result=++*p;publishedSequence.push_back(result);return result;}
static void MemoryBarrier(){++barriers;}
struct Settings {debugentries::Config debugPolicy;} extra;
struct Shared {volatile LONG debugPolicySeq=0;debugentries::Config debugPolicy;} state;
static Shared*shared=&state;
#include "debug_entry_settings_subject.inc"

static std::wstring key(uint32_t id){wchar_t result[14]{};debugEntrySettingKey(id,result);return result;}
static bool required(uint32_t id){return debugentries::required(extra.debugPolicy,id);}
int main(){
 using namespace debugentries;
 const uint32_t call=id(Kind::Call,0x4F7870),dr=id(Kind::Breakpoint,0x4F7870);
 check(call!=dr&&find(call)&&find(dr),"same address has independent stable Call and DR identities");
 loadDebugEntrySettings();check(extra.debugPolicy.generation==1&&!extra.debugPolicy.count,"missing settings are strict");
 for(const auto&e:Entries)check(required(e.id),"every entry defaults to required");
 const wchar_t*bad[]={L"",L"0",L"2",L"-1",L"true",L"01",L"1junk",L"1 0",L"4294967297"};
 for(const auto*value:bad){ini[{L"DebugEntries",key(call)}]=value;loadDebugEntrySettings();check(required(call)&&!extra.debugPolicy.count,"only the complete explicit value 1 skips");}
 ini[{L"DebugEntries",key(call)}]=L"1";ini[{L"DebugEntries",key(dr)}]=L"0";
 ini[{L"DebugEntries",L"Skip_FF123456"}]=L"1";
 loadDebugEntrySettings();check(!required(call)&&required(dr)&&extra.debugPolicy.count==1,"stable kind/address settings survive load independently");
 check(extra.debugPolicy.skip[0]==call,"stored configuration contains the stable identity, not a row index");
 for(const auto&e:Entries)if(!(e.flags&Checkable)){ini[{L"DebugEntries",key(e.id)}]=L"1";break;}
 loadDebugEntrySettings();check(extra.debugPolicy.count==1,"unknown and noncheckable stored values cannot opt in");
 saveDebugEntrySettings();check(ini[{L"DebugEntries",key(call)}]==L"1"&&ini[{L"DebugEntries",key(dr)}]==L"0","save writes exact 0/1 per identity");
 check(ini[{L"DebugEntries",L"Skip_FF123456"}]==L"1","unrelated saved keys are preserved");
 extra.debugPolicy={};loadDebugEntrySettings();check(!required(call)&&required(dr),"saved identity round trip is exact");
 const uint32_t priorGeneration=extra.debugPolicy.generation;
 check(setSkip(extra.debugPolicy,call,false)&&extra.debugPolicy.generation!=priorGeneration,"turning off changes the policy generation");
 saveDebugEntrySettings();extra.debugPolicy={};loadDebugEntrySettings();check(required(call)&&!extra.debugPolicy.count,"turning off persists strict validation");
 failWrite=true;saveWriteOK=true;saveDebugEntrySettings();check(!saveWriteOK&&writes>1,"the real host writer retains an individual persistence failure");
 saveWriteOK=true;
 syncDebugEntrySettings();check(state.debugPolicySeq==2&&barriers==1,"first mapping receives one complete seqlock publication");
 check(publishedSequence.size()==2&&publishedSequence[0]==1&&publishedSequence[1]==2,"publication exposes odd then even sequence");
 check(!std::memcmp(&state.debugPolicy,&extra.debugPolicy,sizeof(Config)),"published policy is the exact host value");
 syncDebugEntrySettings();check(state.debugPolicySeq==2&&barriers==1,"an unchanged policy is not republished every frame");
 check(setSkip(extra.debugPolicy,dr,true),"one DR override can be selected");syncDebugEntrySettings();
 check(state.debugPolicySeq==4&&state.debugPolicy.count==1&&state.debugPolicy.skip[0]==dr,"changed policy reaches the runtime before a disk save");
 Shared replacement;shared=&replacement;syncDebugEntrySettings();check(replacement.debugPolicySeq==2&&replacement.debugPolicy.skip[0]==dr,"reconnection publishes into a fresh mapping");
 shared=nullptr;syncDebugEntrySettings();check(replacement.debugPolicySeq==2,"disconnected synchronization is inert");
 std::printf("{\"passed\":true,\"checks\":%u,\"production_functions_extracted\":true,\"profile_and_atomic_boundaries_modeled\":true,\"windows_executed\":false,\"game_tested\":false}\n",checks);
}
