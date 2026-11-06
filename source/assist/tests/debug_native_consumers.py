"""Extract production secondary call-signature consumers and exercise entry-local overrides."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',required=True,type=Path);a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[3];base=r/'source/assist/source';sources={}
def function(file,name):
 text=(base/file).read_text();sources[file]=hashlib.sha256((base/file).read_bytes()).hexdigest()
 start=text.index('static bool '+name+'(');at=text.index('{',start);depth=1;end=at+1
 while depth:
  if text[end]=='{':depth+=1
  elif text[end]=='}':depth-=1
  end+=1
 return text[start:end]
pairs=[('autoload_bridge.h','loadInterfaces'),('superweapon_grant_bridge.h','superweaponGrantSignatures'),('sticky_bridge.h','stickyABI'),('native_placement_bridge.h','nativePlacementSignature'),('native_placement_completion_bridge.h','nativePlacementCompletionSignature'),('view_zoom_native_state.h','nativeViewSignature'),('sidebar_view_bridge.h','sidebarViewRedrawCompatible'),('view_zoom_native_state.h','nativeViewNativeCompatible'),('view_zoom_legacy_bridge.h','zoomNativeCompatible'),('build_time_tooltip.h','buildTimeTooltipCompatible'),('aircraft_selection_bridge.h','aircraftSelectionCode'),('native_viewport_query.h','nativeViewQueryCompatible'),('scroll_refresh_bridge.h','highScrollInterfaces')]
head=r'''#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "DEBUG_CHECKS"
using DWORD=uint32_t;static uint32_t match=1,now=1000,failAt=0;static bool originalLoad=false;
struct Signature{uint32_t address,length;uint8_t bytes[12];};struct Shared{int scrollRefreshMode=3;};static Shared shared,*sh=&shared;
static DWORD sampleClock(Shared*){return now;}static bool(*nativeViewQueryOwner)()=nullptr;
static bool read(uint32_t at,void*out,size_t n){if(at==failAt)return false;memset(out,0,n);if(originalLoad&&at==0x4C6860){const uint8_t b[]={0x8B,0xC1,0x8B,0x4C,0x24,4,0x85,0xC9};memcpy(out,b,n<sizeof b?n:sizeof b);}return true;}
static unsigned checks=0;static void checked(bool ok,int line){++checks;if(!ok){std::fprintf(stderr,"consumer check %u at %d failed\n",checks,line);std::abort();}}
#define check(x) checked((x),__LINE__)
'''.replace('DEBUG_CHECKS',str(r/'source/shared/source/debug_entry_checks.h'))
body='\n'.join(function(f,n) for f,n in pairs)
main=r'''
int main(){using namespace debugentries;
 struct Group{const char*name;std::vector<uint32_t>deps;bool(*run)();};
 const Group groups[]={
 {"autoload",{0x4C6860},loadInterfaces},
 {"grant",{0x6E1C40,0x6CB560,0x6A6300,0x6ABC60,0x6A60A0},superweaponGrantSignatures},
 {"sticky",{0x5B2FD0,0x5B3570},stickyABI},
 {"place",{0x4C6AE0},[](){const uint8_t expected[]={0x53,0x8B};return nativePlacementSignature(0x4C6AE0,expected,2);}},
 {"cursor",{0x4A8BF0,0x4A8D50},[](){return nativePlacementCompletionSignature(0x4A8BF0,0xC44D9D5E)&&nativePlacementCompletionSignature(0x4A8D50,0xDC84E30E);}},
 {"view",{0x4A8960,0x6D5F60,0x4BA5A0,0x7BC970,0x7BCAE0,0x410CE0,0x410E50,0x6D2790,0x6D2B3F,0x4F42F0},nativeViewNativeCompatible},
 {"legacy_view",{0x560BF0},zoomNativeCompatible},
 {"tooltip",{0x479050,0x6A9316,0x6F47A0,0x6F4803,0x4C9FB0},buildTimeTooltipCompatible},
 {"deselect",{0x5F44A0,0x40CF00,0x4AEB10,0x4AEB30},[](){return aircraftSelectionCode(0x5F44A0,123,0x1064603B)&&aircraftSelectionCode(0x40CF00,38,0xE88F130E)&&aircraftSelectionCode(0x4AEB10,23,0x0EF2E454)&&aircraftSelectionCode(0x4AEB30,24,0xE51B873C);}},
 {"query",{0x692300},nativeViewQueryCompatible},
 {"refresh",{0x4F42F0,0x4F4480,0x6D6000,0x692B60,0x693440},highScrollInterfaces}
 };
 for(const auto&g:groups){Config all{};for(uint32_t at:g.deps)check(setSkip(all,id(Kind::Call,at),true));applyPolicy(all);now+=600;check(g.run());
  for(uint32_t at:g.deps){Config oneStrict=all;check(setSkip(oneStrict,id(Kind::Call,at),false));applyPolicy(oneStrict);now+=600;check(!g.run());check(observation(Kind::Call,at)->signature==debugentries::Signature::Mismatch);
   applyPolicy(all);now+=600;check(g.run());failAt=at;now+=600;check(!g.run());check(observation(Kind::Call,at)->signature==debugentries::Signature::Unreadable);failAt=0;
   check(signatureRequired(Kind::Patch,at));check(signatureRequired(Kind::Breakpoint,at));
  }
 }
 // Existing 500ms automatic-load memo must be invalidated immediately when
 // restoring strict mode, including re-used host configuration generations.
 applyPolicy(Config{});originalLoad=true;now+=600;check(loadInterfaces());originalLoad=false;now+=5;check(loadInterfaces());
 Config skipLoad{};setSkip(skipLoad,id(Kind::Call,0x4C6860),true);applyPolicy(skipLoad);check(loadInterfaces());applyPolicy(Config{});now+=5;check(!loadInterfaces());
 // A still-owned native viewport query is a known platform path, but unreadable
 // memory is not. Its independent Patch switch does not admit the ordinary Call.
 nativeViewQueryOwner=[](){return true;};applyPolicy(Config{});check(nativeViewQueryCompatible());failAt=0x692300;check(!nativeViewQueryCompatible());failAt=0;nativeViewQueryOwner=nullptr;
 Config patch{};setSkip(patch,id(Kind::Patch,0x692300),true);applyPolicy(patch);check(!nativeViewQueryCompatible());
 check(!nativeViewSignature(0x4A8960,33,1));check(!aircraftSelectionCode(0x5F44A0,129,1));
 Config executable{};setSkip(executable,id(Kind::Call,0x4C6AE0),true);setSkip(executable,id(Kind::Patch,0x55DE81),true);applyPolicy(executable);
 executableProbe=[](uint32_t,size_t){return false;};const uint8_t signature[]={0x53,0x8B};
 check(!nativePlacementSignature(0x4C6AE0,signature,sizeof signature));check(allow(Kind::Patch,0x55DE81,true,false));executableProbe=nullptr;
 std::printf("PASS: %u extracted secondary native-consumer checks across %zu groups; no engine calls\n",checks,sizeof groups/sizeof groups[0]);
}
'''
(out/'fixture.cpp').write_text(head+body+main);commands=[]
for cmd in [['g++','-std=c++17','-O2','-UNDEBUG',str(out/'fixture.cpp'),'-o',str(out/'fixture')],[str(out/'fixture')]]:
 proc=subprocess.run(cmd,text=True,capture_output=True);commands.append(dict(command=cmd,exit_code=proc.returncode,stdout=proc.stdout,stderr=proc.stderr));print(proc.stdout,proc.stderr,flush=True)
 if proc.returncode:break
stable=all(hashlib.sha256((base/f).read_bytes()).hexdigest()==h for f,h in sources.items())
result=dict(passed=stable and all(x['exit_code']==0 for x in commands),source_sha256=sources,source_hash_stable=stable,game_process_executed=False,commands=commands)
(out/'result.json').write_text(json.dumps(result,indent=2));raise SystemExit(not result['passed'])
