"""Execute the production lost-release cancellation blocks with controlled keys."""
import argparse, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
src=Path(__file__).resolve().parents[1]/'source'
f=(src/'formation_bridge.h').read_text();start=f.index(' const unsigned button=formationQuick?');end=f.index(' formationRefreshHold();',start)
m=(src/'mo_orders_bridge.h').read_text();start_m=m.index(' if(moReleaseWatch.expired(');end_m=m.index(' if(!moOrdersPending())',start_m)
code=r'''#include "input_ownership.h"
#include "mo_orders.h"
#include <cassert>
#include <cstdio>
using namespace assist;
constexpr unsigned VK_LBUTTON=1,VK_RBUTTON=2,WM_LBUTTONUP=0x202,WM_RBUTTONUP=0x205;
static unsigned keys[256]{},cancelled=0,moCancelled=0;
static int GetAsyncKeyState(unsigned k){return keys[k]?0x8000:0;}
static bool formationQuick=false,formationQuickCaptured=false,formationButton=false,moHolding=false;
static bool formationHolding=false,formationModal=false;
static uint32_t formationHoldingKey=0;
static struct {uint32_t hold=0;}formationPolicy;
static struct {void decorate(uint8_t*){}}runtimeKeys;
static void formationPublish(){}
static uint32_t formationQuickMouse=0,formationButtonUp=WM_RBUTTONUP,moGestureButton=0;
static inputownership::ReleaseWatch formationReleaseWatch,formationFollowReleaseWatch,moReleaseWatch;
static struct {bool down=false;int cell=0;}formationFollow;
static struct {uint32_t formationConsumedUntil=0;}state,*sh=&state;
static void formationCancel(){++cancelled;formationQuick=false;}
static void resetMoOrders(){++moCancelled;moHolding=false;moReleaseWatch.clear();}
'''
hs=f.index('static void formationRefreshHold(){');he=f.index('static bool formationQuickAuthorized',hs)
code+=f[hs:he]
code+='void formationRecover(uint32_t now){'+f[start:end]+'}\n'
code+='void moRecover(uint32_t now){'+m[start_m:end_m]+'}\n'
code+=r'''
int main(){
 formationHolding=true;formationHoldingKey=formationPolicy.hold='F';keys['F']=1;
 formationRefreshHold();assert(formationHolding);
 keys['F']=0;formationRefreshHold();assert(!formationHolding&&!formationHoldingKey&&cancelled==1);

 for(auto button:{1u,2u,3u,4u,6u}){
  formationQuick=formationQuickCaptured=formationButton=true;formationQuickMouse=button;formationReleaseWatch.clear();
  unsigned vk=moorders::gestureVK(button),before=cancelled;keys[vk]=1;
  formationRecover(1000);formationRecover(2000);assert(cancelled==before);
  keys[vk]=0;formationRecover(2001);formationRecover(2250);assert(cancelled==before);
  formationRecover(2251);assert(cancelled==before+1&&!formationButton&&!formationQuickCaptured&&!formationQuickMouse);
  moHolding=true;moGestureButton=button;moReleaseWatch.clear();keys[vk]=1;before=moCancelled;
  moRecover(1000);moRecover(2000);assert(moCancelled==before);
  keys[vk]=0;moRecover(2001);moRecover(2250);assert(moCancelled==before);
  moRecover(2251);assert(moCancelled==before+1&&!moHolding);
 }
 formationFollow.down=true;formationFollow.cell=123;sh->formationConsumedUntil=99999;
 formationRecover(3000);formationRecover(3250);
 assert(!formationFollow.down&&!formationFollow.cell&&!sh->formationConsumedUntil);
 // A normal (non-quick) formation drag uses the native left/right VK.
 formationQuick=false;formationButton=true;formationButtonUp=WM_LBUTTONUP;formationReleaseWatch.clear();keys[VK_LBUTTON]=1;
 const auto before=cancelled;formationRecover(4000);formationRecover(5000);assert(cancelled==before);
 keys[VK_LBUTTON]=0;formationRecover(5001);formationRecover(5251);assert(cancelled==before+1);
 puts("PASS production gesture cancellation: all five configured mouse buttons, native formation, held/late-release/lost-release and stale continuation input lease");
}
'''
cpp=a.out/'production.cpp';exe=a.out/'lost-release';cpp.write_text(code)
subprocess.run(['g++','-std=c++17','-O1','-I'+str(src),str(cpp),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True);print(r.stdout,end='');(a.out/'result.json').write_text(json.dumps({'passed':r.returncode==0,'production_call_sites':True,'windows_game_tested':False,'stdout':r.stdout,'stderr':r.stderr},indent=2)+'\n');raise SystemExit(r.returncode)
