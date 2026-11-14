#!/usr/bin/env python3
"""Run actual rule-data technology/native placement mode transitions with bounded native/patch mocks."""
import argparse,json,subprocess,sys,hashlib
from pathlib import Path

def block(s,start):
 a=s.index(start);o=s.index('{',a);n=1;i=o+1
 while n:n+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]

def main():
 p=argparse.ArgumentParser();tool=p.add_mutually_exclusive_group(required=True);tool.add_argument('--zig',type=Path);tool.add_argument('--cxx',type=Path);p.add_argument('--placement-source',type=Path);p.add_argument('--negative-control',action='store_true');p.add_argument('--build-dir',type=Path,required=True);a=p.parse_args()
 src=Path(__file__).resolve().parents[1]/'source';sidebar=(src/'sidebar.h').read_text(encoding='utf-8');placement=(a.placement_source or src/'placement_bridge.h').read_text(encoding='utf-8');bridge=(src/'bridge.cpp').read_text(encoding='utf-8');master=(src/'master_tech_bridge.h').read_text(encoding='utf-8')
 names=['static int typeSlot(uint32_t type)','static int typeTab(','static void sidebarRedraw(','static int canonicalKind(','static bool hasCameo(','static uint32_t techFactory(','static bool techTransitionProductionPending(','static bool ensureTechCameo(','static void restoreTech(','static void observeMasterCameos(','static void refreshTech(']
 functions='\n'.join(block(sidebar,x) for x in names)
 retire='\n'.join(block(placement,x) for x in ['static bool nativeTextTechRetired(','static bool nativeTechRetired(','static bool nativePlacementRetired(','static bool setTechSites('])
 place=placement[placement.index(' if((sh->flags&BuildAnywhere)&&sh->placementImplementation==2)'):placement.index(' if(airpathpolicy::native(',placement.index('static void placementTick()'))]
 dispatch=block(bridge,'else if(c==OpenTech)')+'\n'+block(bridge,'else if(c==Produce||c==CancelTechProduction)')
 code=r'''
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <type_traits>
#include "technology_mode_policy.h"
#include "sidebar_policy.h"
#include "hook_registry.h"
using namespace assist;using DWORD=uint32_t;
constexpr uint32_t UnsafeTech=4096,BuildAnywhere=65536;
enum {OpenTech=10,Produce=12,CancelTechProduction=22};
struct Type {int kind=40,index=0,naval=0;};
struct Snapshot {int techOpen=0,techVisible=0,techUnavailable=0,techError=0,typeCount=3,placementStatus=0;Type types[4];} snap;
struct Shared {uint32_t flags=UnsafeTech,arg2=3,placementImplementation=0;uint64_t requestTypeHash=123;bool techAllowed[4]{true,true,true,false};} opts,*sh=&opts;
static uint64_t typeListHash(const Type*,int){return 123;}
static bool techAllowed[4]{true,true,true,false};static uint32_t typePointers[4]{0x20000,0x21000,0x22000,0};
static uint32_t catalogueHash=123,techRefresh=0;static unsigned techImplementation=0;static bool techPending=false,riskTechActive=false,riskPlacementActive=false;
static int techScanAt=0,techScanVisible=0,techScanUnavailable=0,techScanFrame=INT32_MIN;static uint64_t techScanCatalogue=0;static unsigned techScanImplementation=UINT32_MAX;
static void requestTechRefresh(){techPending=true;techScanAt=techScanVisible=techScanUnavailable=0;techScanCatalogue=catalogueHash;}
static constexpr uint32_t Sidebar=0x87F7E8,Tabs=Sidebar+0x1544;
struct Watch {int tab=-1;};static Watch reservedSidebar[2];
static std::map<uint32_t,uint8_t> mem;static uint32_t unreadable=0,failRestore=0;static bool afterFail=false,fullBuild=true,limitBuild=true,factoryReady=true,factoryAllow=true;
static int adds=0,events=0,eventKind=0,watches=0,dataWrites=0,installs=0,restores=0;
template<class T>static void seed(uint32_t at,T value){auto*p=(const uint8_t*)&value;for(unsigned i=0;i<sizeof value;++i)mem[at+i]=p[i];}
static bool read(uint32_t at,void*out,size_t n){if(unreadable>=at&&unreadable<uint64_t(at)+n)return false;auto*p=(uint8_t*)out;for(unsigned i=0;i<n;++i)p[i]=mem[at+i];return true;}
template<class T>static T get(uint32_t at){T v{};read(at,&v,sizeof v);return v;}
template<class T>static void put(uint32_t at,T value){++dataWrites;seed(at,value);}
static int kind(uint32_t){return 1;}static uint32_t typeOf(uint32_t,int){return 0x20000;}
static uintptr_t valueOf(const void*p){return uintptr_t(p);}template<class T>static uintptr_t valueOf(T v){return uintptr_t(v);}
template<class R,class... A>static R call(uint32_t address,uint32_t,A... args){
 uintptr_t values[]={valueOf(args)...};uintptr_t value=0;
 if(address==0x4F7870)value=values[1]?limitBuild:fullBuild;
 else if(address==0x6A6300){++adds;int count=get<int>(Tabs+84);seed<int>(Tabs+88+count*52,int(values[1]));seed<int>(Tabs+92+count*52,int(values[0]));seed<int>(Tabs+84,count+1);value=1;}
 else if(address==0x60000){factoryAllow=values[0]!=0;value=factoryReady?0x40000:0;}
 if constexpr(!std::is_void_v<R>)return R(value);
}
template<class R,class... A>static R callFast(uint32_t,A...){return R(0);}
// Entry availability is a bounded engine mock here; this fixture owns mode transitions.
static bool nativeDependencyReady(uint32_t,bool=false){return true;}
template<class... A>static bool nativeDependenciesReady(A...){return true;}
static bool techNativeDependencies(unsigned){return true;}
static bool dataApplyOK=true,dataRestoreOK=true,dataOwned=false;static int dataApplies=0,dataRestores=0;
static bool applyTechData(uint32_t){++dataApplies;if(!dataApplyOK)return false;dataOwned=true;return true;}
static bool restoreTechData(){++dataRestores;if(dataOwned&&!dataRestoreOK)return false;dataOwned=false;return true;}
static bool techDataPending(){return dataOwned;}
static bool restoreTechParameters(){return true;}
static bool requestTechParameters(uint32_t){return true;}
static bool techParametersRetired(){return true;}
static bool restoreTechEnhanceParameters(){return true;} // independent hardware modes off
static bool techDataOwns(uint32_t){return dataOwned;}
static bool techLegacyEnhancementOwns(uint32_t){return false;} // legacy enhancement off in this mode fixture
static bool nativeProduceEvent(uint32_t,const Type&,bool=false,int kind=0x0E){++events;eventKind=kind;return true;}
static void watchReservedSidebar(uint32_t,int,int,uint32_t,DWORD){++watches;}
static hookregistry::Registry<> codeHookRegistry;
struct RiskSite {uint32_t address;unsigned size;uint8_t before[16],after[16];};
RISKARRAY
MASTERARRAY
static void prepareRiskSites(){for(auto&s:riskSites){memcpy(s.after,s.before,s.size);s.after[0]^=0x55;}}
static void prepareMasterTech(){for(auto&s:masterTechSites){memcpy(s.after,s.before,s.size);s.after[0]^=0x55;}}
static bool installRiskSite(RiskSite&s){
 if(dataOwned)return false;
 if(codeHookRegistry.active(s.address))return !codeHookRegistry.conflict(s.address);
 uint8_t bytes[16]{};read(s.address,bytes,s.size);if(memcmp(bytes,s.before,s.size)||codeHookRegistry.conflict(s.address))return false;
 codeHookRegistry.commit(s.address,s.before,s.after,s.size,hookregistry::Transition::Install);for(unsigned i=0;i<s.size;++i)seed<uint8_t>(s.address+i,s.after[i]);++installs;return true;
}
static bool restoreRiskSite(RiskSite&s){
 if(!codeHookRegistry.active(s.address))return true;if(codeHookRegistry.conflict(s.address))return false;
 if(failRestore==s.address&&!afterFail)return false;
 for(unsigned i=0;i<s.size;++i)seed<uint8_t>(s.address+i,s.before[i]);++restores;
 if(failRestore==s.address)return false;
 codeHookRegistry.commit(s.address,s.after,s.before,s.size,hookregistry::Transition::Restore);return true;
}
static bool restoreMasterTech(){bool ok=true;for(auto&s:masterTechSites)if(!restoreRiskSite(s))ok=false;return ok;}
namespace patchcontext {template<class Read>static bool compatible(uint32_t,Read){return true;}}
static bool directGroupRestore(directpatch::Group group){bool ok=true;for(auto&s:directpatch::Sites)if(s.group==group&&codeHookRegistry.active(s.address)){
 RiskSite r{s.address,s.size,{},{}};memcpy(r.before,s.before,s.size);memcpy(r.after,s.after,s.size);if(!restoreRiskSite(r))ok=false;}return ok;}
static bool directGroupEnable(directpatch::Group group){bool ok=true;for(auto&s:directpatch::Sites)if(s.group==group){RiskSite r{s.address,s.size,{},{}};memcpy(r.before,s.before,s.size);memcpy(r.after,s.after,s.size);if(!installRiskSite(r))ok=false;}return ok;}
RETIRE
FUNCTIONS
static int dispatch(int c,int arg=1,uint32_t house=0x10000,DWORD now=1000){int result=1,n=0;if(false){}
DISPATCH
return result;}
static void placementModeTick(){
PLACE
}
static void reset(){
 mem.clear();codeHookRegistry={};opts={};snap={};dataApplyOK=dataRestoreOK=true;dataOwned=false;dataApplies=dataRestores=0;techImplementation=0;techPending=riskTechActive=riskPlacementActive=false;
 unreadable=failRestore=0;afterFail=false;fullBuild=limitBuild=factoryReady=factoryAllow=true;adds=events=eventKind=watches=dataWrites=installs=restores=0;
 techScanFrame=INT32_MIN;techScanImplementation=UINT32_MAX;techScanCatalogue=0;techScanAt=techScanVisible=techScanUnavailable=0;techRefresh=0;
 prepareRiskSites();prepareMasterTech();for(auto&s:riskSites)for(unsigned i=0;i<s.size;++i)seed<uint8_t>(s.address+i,s.before[i]);
 for(auto&s:masterTechSites)for(unsigned i=0;i<s.size;++i)seed<uint8_t>(s.address+i,s.before[i]);
 for(auto&s:directpatch::Sites)for(unsigned i=0;i<s.size;++i)seed<uint8_t>(s.address+i,s.before[i]);
 for(int i=0;i<4;++i){techAllowed[i]=i<3;if(i<3){snap.types[i].index=i;typePointers[i]=0x20000+i*0x1000;seed<uint32_t>(typePointers[i],0x30000);}}
 seed<uint32_t>(0x30094,0x60000);seed<int>(0xA8ED84,100);reservedSidebar[0]={};reservedSidebar[1]={};
}
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char*name){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",name);}};
 reset();check(dispatch(OpenTech)==1&&snap.techOpen==4&&nativeTechRetired(),"cold rule-data mode ready after old patches retired");
 check(installs==0&&dataApplies==1&&adds==3&&snap.techVisible==3,"whole allowed catalogue uses native AddCameo without instruction patches");
 check(dispatch(Produce,0)==-4&&dispatch(CancelTechProduction,0)==-4&&events==0,"rule-data mode cannot fall back to unpublished toolbox production");
 int before=adds;refreshTech(0x10000,1001);check(adds==before,"same-frame maintenance does not repeat whole catalogue pass");
 reset();fullBuild=false;check(dispatch(OpenTech)==1&&snap.techVisible==0&&snap.techUnavailable==3&&adds==0,"full native CanBuild controls icon admission");
 reset();factoryReady=false;check(dispatch(OpenTech)==1&&snap.techVisible==0&&adds==0,"real compatible factory is mandatory");
 reset();opts.arg2=0;check(dispatch(OpenTech)==1&&riskTechActive&&codeHookRegistry.installed()==2,"legacy mode retains original pair");
 opts.arg2=3;before=installs;check(dispatch(OpenTech)==1&&snap.techOpen==4&&nativeTechRetired()&&codeHookRegistry.installed()==0&&installs==before,"legacy pair retires before data ownership");
 opts.arg2=1;check(dispatch(OpenTech)==1&&snap.techOpen==2&&!dataOwned&&codeHookRegistry.installed()==2,"data restores before switching to classic patch mode");
 reset();opts.arg2=2;check(dispatch(OpenTech)==1&&codeHookRegistry.installed()==5,"master mode retains five sites");
 opts.arg2=3;check(dispatch(OpenTech)==1&&snap.techOpen==4&&codeHookRegistry.installed()==0,"master group retires before rule data apply");
 for(bool after:{false,true}){reset();opts.arg2=0;dispatch(OpenTech);opts.arg2=3;failRestore=riskSites[6].address;afterFail=after;
  check(dispatch(OpenTech)==-11&&snap.techOpen==0&&techPending&&!nativeTechRetired()&&!dataOwned,"failed pre/post patch restore cannot apply data");
  failRestore=0;refreshTech(0x10000,2000);check(snap.techOpen==4&&!techPending&&nativeTechRetired()&&dataOwned,"patch cleanup recovery can finish data mode");}
 reset();seed<uint8_t>(masterTechSites[0].address,0xCC);check(dispatch(OpenTech)==-11&&techPending&&!dataOwned&&installs==0,"foreign inactive site blocks data mode");
 seed<uint8_t>(masterTechSites[0].address,masterTechSites[0].before[0]);refreshTech(0x10000,2000);check(snap.techOpen==4&&dataOwned,"original signature recovery retries pending request");
 reset();codeHookRegistry.markConflict(riskSites[7].address);check(dispatch(OpenTech)==-11&&snap.techOpen==0&&!dataOwned,"sticky conflict blocks data despite original inactive bytes");
 reset();dataApplyOK=false;check(dispatch(OpenTech)==-44&&snap.techOpen==0&&techPending&&adds==0,"failed rule apply advertises no readiness or icons");dataApplyOK=true;refreshTech(0x10000,2000);check(snap.techOpen==4&&adds==3,"rule application recovery retains pending request");
 reset();dispatch(OpenTech);dataRestoreOK=false;opts.arg2=0;check(dispatch(OpenTech)==-44&&snap.techOpen==0&&techPending&&codeHookRegistry.installed()==0&&dataOwned,"pending rule rollback blocks new patch mode");
 dataRestoreOK=true;refreshTech(0x10000,2000);check(snap.techOpen==1&&!dataOwned&&codeHookRegistry.installed()==2,"rule cleanup completes before deferred old mode installs");
 reset();dispatch(OpenTech);dataRestoreOK=false;check(dispatch(OpenTech,0)==-44&&techImplementation==0&&snap.techOpen==0&&dataOwned,"explicit off retains failed data restoration responsibility");
 reset();opts.arg2=0;dispatch(OpenTech);const int oldOpen=snap.techOpen;opts.arg2=3;opts.techAllowed[0]=false;
 seed<uint32_t>(0x10000+0x53B4,0x40000);seed<uint32_t>(0x4006C,0x10000);seed<uint32_t>(0x40058,0x50000);fullBuild=false;
 check(dispatch(OpenTech)==-43&&techImplementation==0&&snap.techOpen==oldOpen&&techAllowed[0]&&codeHookRegistry.installed()==2,"unsafe old production preserves old mode allowlist and hooks");
 refreshTech(0x10000,2000);check(snap.techError==-43&&codeHookRegistry.installed()==2,"pending transition remains explicit");
 fullBuild=true;refreshTech(0x10000,3000);check(snap.techError==0&&techImplementation==0,"production clearance permits host retry");
 check(dispatch(OpenTech)==1&&snap.techOpen==4&&!techAllowed[0],"host retry applies new allowlist");
 reset();dispatch(OpenTech);opts.arg2=0;seed<uint32_t>(0x10000+0x53B4,0x40000);seed<uint32_t>(0x4006C,0x10000);seed<uint32_t>(0x40058,0x50000);
 check(dispatch(OpenTech)==-43&&snap.techOpen==4&&dataOwned&&codeHookRegistry.installed()==0,"data-dependent product blocks rollback mode switch");
 for(unsigned mode:{0u,1u,2u}){reset();dispatch(OpenTech);opts.arg2=mode;
  check(dispatch(OpenTech)==1&&snap.techOpen==int(mode+1)&&!dataOwned&&codeHookRegistry.installed()==(mode==2?5:2),"data restores before every retained native patch implementation");
  opts.arg2=3;check(dispatch(OpenTech)==1&&snap.techOpen==4&&dataOwned&&nativeTechRetired()&&codeHookRegistry.installed()==0,"each retained implementation can return to data mode");}
 reset();opts.flags=BuildAnywhere;opts.placementImplementation=2;placementModeTick();check(snap.placementStatus==2&&nativePlacementRetired()&&installs==0,"independent native placement installs nothing");
 opts.placementImplementation=0;placementModeTick();check(riskPlacementActive&&codeHookRegistry.installed()==1,"old placement remains available");opts.placementImplementation=2;placementModeTick();check(nativePlacementRetired()&&snap.placementStatus==2&&codeHookRegistry.installed()==0,"placement cleanup before native mode");
 codeHookRegistry.markConflict(directpatch::Sites[5].address);placementModeTick();check(snap.placementStatus==-2&&!nativePlacementRetired(),"placement conflict blocks native readiness");
 // Foreign legacy instruction bytes are not evidence that we own a patch.
 // Reproduce all three choices being blocked by an unrelated platform edit:
 // the two code adapters still reject it, but the native event must be usable.
 reset();opts.flags=BuildAnywhere;
 for(uint32_t at:{riskSites[0].address,directpatch::Sites[5].address,directpatch::Sites[6].address})seed<uint8_t>(at,0xCC);
 const auto foreign=mem;opts.placementImplementation=0;placementModeTick();check(snap.placementStatus==-2&&mem==foreign,"controlled adapter leaves unsupported platform bytes alone");
 opts.placementImplementation=1;placementModeTick();check(snap.placementStatus==-2&&mem==foreign,"direct adapter leaves unsupported platform bytes alone");
 opts.placementImplementation=2;placementModeTick();check(snap.placementStatus==2&&nativePlacementRetired()&&mem==foreign&&installs==0,"native event does not require pristine unused platform code");
 // Missing bytes, real journals and recognizable untracked replacements still block.
 reset();opts.flags=BuildAnywhere;opts.placementImplementation=2;unreadable=riskSites[0].address;placementModeTick();check(snap.placementStatus==-2,"unreadable old site does not prove retirement");
 for(unsigned which:{0u,1u,2u}){reset();opts.flags=BuildAnywhere;opts.placementImplementation=2;
  const uint32_t at=which==0?riskSites[0].address:directpatch::Sites[which+4].address;
  const unsigned n=which==0?riskSites[0].size:directpatch::Sites[which+4].size;
  const uint8_t*replacement=which==0?riskSites[0].after:directpatch::Sites[which+4].after;
  for(unsigned i=0;i<n;++i)seed<uint8_t>(at+i,replacement[i]);placementModeTick();check(snap.placementStatus==-2,"untracked recognizable own placement patch blocks native event");
 }
 reset();opts.flags=BuildAnywhere;opts.placementImplementation=0;placementModeTick();failRestore=riskSites[0].address;
 opts.placementImplementation=2;placementModeTick();check(snap.placementStatus==-2&&codeHookRegistry.active(riskSites[0].address),"failed actual restore remains a debt");
 failRestore=0;placementModeTick();check(snap.placementStatus==2&&nativePlacementRetired(),"restoration recovery enables native event without restart");

 std::printf("technology rule-data production transition checks=%d failures=%d; bounded engine/data/patch mocks, no game/netplay\n",checks,failures);return failures?1:0;
}
'''
 risk=block(placement,'static RiskSite riskSites[]=')+';';masterarray=block(master,'static RiskSite masterTechSites[]=')+';'
 code=code.replace('RISKARRAY',risk).replace('MASTERARRAY',masterarray).replace('RETIRE',retire).replace('FUNCTIONS',functions).replace('DISPATCH',dispatch).replace('PLACE',place)
 code=code.replace('#include "hook_registry.h"','#include "hook_registry.h"\n#include "../../shared/source/experimental_patch_policy.h"')
 a.build_dir.mkdir(parents=True,exist_ok=True);cpp=a.build_dir/'technology_data_mode.cpp';exe=a.build_dir/'technology_data_mode.exe';cpp.write_text(code,encoding='utf-8')
 cmd=([str(a.zig),'c++','-target','x86_64-windows-gnu'] if a.zig else [str(a.cxx)])+['-std=c++17','-O1','-fno-exceptions','-fno-rtti','-I',str(src),str(cpp),'-o',str(exe)]
 c=subprocess.run(cmd,text=True,capture_output=True);(a.build_dir/'build.log').write_text(c.stdout+c.stderr,encoding='utf-8')
 if c.returncode:print(c.stderr);raise SystemExit(c.returncode)
 r=subprocess.run([str(exe)],text=True,capture_output=True);(a.build_dir/'test.log').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout+r.stderr)
 report=dict(passed=r.returncode==0,exit_code=r.returncode,production_functions_extracted=True,placement_sha256=hashlib.sha256((a.placement_source or src/'placement_bridge.h').read_bytes()).hexdigest(),game_tested=False,multiplayer_tested=False,output=r.stdout+r.stderr)
 if a.negative_control and r.returncode==0:
  baseline='5e1502339dc6bfc3b46231bb874875e979dde3c9';old=a.build_dir/'pre6-placement.h'
  old.write_bytes(subprocess.check_output(['git','show',baseline+':source/assist/source/placement_bridge.h'],cwd=src.parents[2]))
  child=[sys.executable,str(Path(__file__).resolve()),'--build-dir',str(a.build_dir/'baseline'),'--placement-source',str(old.resolve())]
  child+=['--zig',str(a.zig.resolve())] if a.zig else ['--cxx',str(a.cxx.resolve())]
  negative=subprocess.run(child,text=True,capture_output=True);(a.build_dir/'negative-driver.log').write_text(negative.stdout+negative.stderr)
  expected=negative.returncode==1 and 'checks=47 failures=1' in negative.stdout and 'FAIL native event does not require pristine unused platform code' in negative.stdout
  report['negative_control']=dict(passed=expected,baseline=baseline,driver_exit=negative.returncode,expected_failures=1)
  report['passed']=report['passed'] and expected;print('pre.6 negative control: '+('expected single platform-byte rejection reproduced' if expected else 'FAILED'))
 (a.build_dir/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise SystemExit(not report['passed'])
if __name__=='__main__':main()
