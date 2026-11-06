// The native detour and Master adaptation share these three owned sites.
// A mode is never relabelled while any previous bytes remain installed.
#include "../../shared/source/master_control_policy.h"
static RiskSite controlSites[]={
 {experimentalpatch::ControlOwner,5,{0xE8,0x31,0x7D,0xED,0xFF},{}},
 {experimentalpatch::ControlPlanning,5,{0xE8,0xAB,0x38,0xED,0xFF},{}},
 {experimentalpatch::ControlHouse,5,{0xA1,0x38,0xB2,0xA8,0},{}}
};
static uint32_t controlSitesMode=controlmode::Off;
static bool controlSitesOwned(){for(const auto&s:controlSites)if(codeHookRegistry.active(s.address))return true;return false;}
static bool controlTransitionPending(){return !controlParametersRetired()||controlHouseFlagsPending()||(controlmode::master(controlSitesMode)&&controlSitesOwned());}
static bool restoreControlSites(){
 riskControlActive=false;
 // Master has no callback gate. While its unconditional predicate remains,
 // the strict owner calls MUST remain, including after an OS post-write error.
 bool ok=restoreRiskSite(controlSites[2]);
 if(codeHookRegistry.active(controlSites[2].address))return false;
 for(int i=1;i>=0;--i)if(!restoreRiskSite(controlSites[i]))ok=false;
 if(!controlSitesOwned())controlSitesMode=controlmode::Off;
 return ok&&!controlSitesOwned();
}
static void controlSitesSuspend(){
 if(sh&&controlmode::parameters(sh->controlAll)){if(controlSitesOwned()&&!restoreControlSites())sh->playerControlStatus=-16;return;}
 // Focus/menu suspension retains the current implementation. An explicit
 // off/mode change still retires it; installation waits for playable context.
 if(!sh||!controlmode::valid(sh->controlAll)||sh->controlAll!=controlSitesMode||!controlParametersRetired()||sh->playerControlStatus==int(controlmode::Parameters)){
  const bool parameters=restoreControlParameters(),code=restoreControlSites();if(sh)sh->playerControlStatus=parameters&&code?0:-2;
 }
}
static void controlSitesTick(bool enabled){
 const uint32_t requested=sh&&controlmode::valid(sh->controlAll)?sh->controlAll:controlmode::Off;
 enabled=enabled&&playertest::Available&&requested!=controlmode::Off&&!controlHouseFlagsPending();
 // Selection is a capability of this control mode, not a Runtime startup
 // dependency. Retire only this mode if its native selection calls differ.
 if(enabled&&!nativeDependenciesReady(0x48DC90,0x6FBFA0)){
  riskControlActive=false;const bool parameters=restoreControlParameters(),code=restoreControlSites();
  if(sh)sh->playerControlStatus=parameters&&code?-11:-16;return;
 }
 // The no-code mode must never pass through either patch installer. A sibling
 // hardware lease may remain while this group is asynchronously retiring.
 if(controlmode::parameters(requested)){
  riskControlActive=false;
  if(!restoreControlSites()||controlHouseFlagsPending()||!controlCompanionsRetired()){
   restoreControlParameters();if(sh)sh->playerControlStatus=-16;return;
  }
  if(enabled)sh->playerControlStatus=requestControlParameters();
  else{const bool retired=restoreControlParameters();if(sh)sh->playerControlStatus=retired?0:-2;}
  return;
 }
 if(!restoreControlParameters()){riskControlActive=false;if(sh)sh->playerControlStatus=-2;return;}

 if(riskControlActive&&(!enabled||requested!=controlSitesMode)&&riskContextReady())unselect();
 riskControlActive=false;
 if(!enabled||requested!=controlSitesMode){
  if(!restoreControlSites()){if(sh)sh->playerControlStatus=-2;return;}
 }
 if(!enabled){if(sh)sh->playerControlStatus=0;return;}
 if(controlmode::master(requested)&&!controlCompanionsRetired()){
  restoreControlSites();sh->playerControlStatus=-2;return;
 }
 bool ok=true;
 if(!controlSitesOwned()){
  // Save only exact native/previously verified platform originals. An E9 by
  // itself is never evidence that somebody else's hook belongs to us.
  for(auto&s:controlSites){uint8_t original[5]{};
   if(!read(s.address,original,5)||!mastercontrol::original(s.address,original,5)){ok=false;break;}
   memcpy(s.before,original,5);
  }
  if(ok){
   auto&s=controlSites[2];riskControlNativeJump=s.before[0]==0xE9;riskControlNativeTarget=s.address+5;
   if(riskControlNativeJump){uint32_t rel=0;memcpy(&rel,s.before+1,4);riskControlNativeTarget+=rel;}
   for(unsigned i=0;i<3;++i){auto&site=controlSites[i];
    if(controlmode::master(requested))memcpy(site.after,mastercontrol::Sites[i].after,5);
    else{const uint32_t target=uint32_t(uintptr_t(site.address==experimentalpatch::ControlHouse?&riskControlEntry:&riskControlOwnerEntry));
     site.after[0]=experimentalpatch::callSite(site.address)?0xE8:0xE9;const uint32_t rel=target-site.address-5;memcpy(site.after+1,&rel,4);}
   }
   // Set before the first API call: failed writes can still acquire ownership.
   controlSitesMode=requested;
  }
 }
 if(ok&&controlmode::master(requested)&&!mastercontrol::compatible(read))ok=false;
 // Whole-group preflight prevents knowingly installing half a group. Both
 // owner calls are installed before the common predicate in either mode.
 if(ok)for(auto&s:controlSites){uint8_t current[5]{};const bool active=codeHookRegistry.active(s.address);
  if(codeHookRegistry.conflict(s.address)||!patchcontext::compatible(s.address,read)||!read(s.address,current,5)||(active?memcmp(current,s.after,5):!hookregistry::admissibleOriginal(s.address,current,5))){
   if(active)codeHookRegistry.markConflict(s.address);ok=false;break;
  }
 }
 if(ok)for(auto&s:controlSites)if(!installRiskSite(s)){ok=false;break;}
 if(!ok){restoreControlSites();sh->playerControlStatus=-2;return;}
 riskControlActive=controlmode::native(requested);sh->playerControlStatus=int(requested);
}
