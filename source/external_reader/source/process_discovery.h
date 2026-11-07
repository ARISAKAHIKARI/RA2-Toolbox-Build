#pragma once
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <cwctype>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include "../../shared/source/message_query.h"
#include "game_process_names.h"
#include "process_image_identity.h"
#include "process_target_selection.h"

// Discover candidate game processes before looking for a window. No handle
// injection, no privilege adjustment, no writing of game memory.
// Supports YRpp's authoritative Game::hWnd (0xB73550) resolution alongside
// EnumWindows fallback for Alt-Tab (切屏) and minimized (最小化) states.
namespace external { namespace discovery {
constexpr uint32_t YrppGameHwndAddress = assist::messagequery::YrppGameHwndAddr;

template<class Reader> auto windowRead(Reader&r,uint32_t a,void*p,size_t n,int)->decltype(r.readFresh(a,p,n)){return r.readFresh(a,p,n);}
template<class Reader> bool windowRead(Reader&r,uint32_t a,void*p,size_t n,long){return r.read(a,p,n);}

struct Candidate {HWND window=nullptr;DWORD pid=0,thread=0;long long score=-1;};
struct Scan {Candidate best{};DWORD processCount=0,windowCount=0;DWORD snapshotError=0;
 DWORD enumeratedProcesses=0,nameCandidates=0,descriptionCandidates=0,descriptionMatches=0;
 DWORD descriptionErrors=0,lastDescriptionError=0,descriptionPending=0,rememberedMatches=0;
 DWORD fingerprintCandidates=0,fingerprintMatches=0,fingerprintMismatches=0,queryErrors=0,lastQueryError=0;
 bool explicitSelection=false;
 // A single known process can be sampled without a visible game HWND.
 // Multiple windowless processes remain ambiguous; never guess a PID.
 bool uniqueWindowless=false;
};
struct WindowContext {Scan* scan;const std::unordered_set<DWORD>* ids;DWORD preferredPid=0;};
inline BOOL CALLBACK checkWindow(HWND w,LPARAM value){
 auto* ctx=reinterpret_cast<WindowContext*>(value);
 DWORD pid=0;DWORD thread=GetWindowThreadProcessId(w,&pid);
 if(!pid||!ctx->ids->count(pid)||(ctx->preferredPid&&pid!=ctx->preferredPid))return TRUE;
 if(GetWindowLongW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)return TRUE;
 RECT r{};if(!GetClientRect(w,&r))return TRUE;
 long long width=r.right-r.left,height=r.bottom-r.top;
 // Fullscreen DirectDraw and minimized YR windows report 0x0 client rects during
 // Alt-Tab (切屏) or minimize (最小化). Keep tracking unowned top-level windows.
 if((width<320||height<200)&&(IsIconic(w)||!GetWindow(w,GW_OWNER))){width=800;height=600;}
 if(width<320||height<200||width>16384||height>16384)return TRUE;
 ++ctx->scan->windowCount;
 // Prefer the actual visible game window; still discover a hidden/minimized
 // one for diagnosis and read-only sampling. Ownership is a tie-breaker.
 long long score=(IsWindowVisible(w)?1LL<<42:0)+
                  (IsIconic(w)?1LL<<41:0)+
                  (GetWindow(w,GW_OWNER)?0:1LL<<40)+width*height;
 if(score>ctx->scan->best.score)ctx->scan->best={w,pid,thread,score};
 return TRUE;
}
// Authoritative YRpp Game::hWnd (0xB73550) resolver:
// Reads DEFINE_REFERENCE(HWND, hWnd, 0xB73550u) via the process reader so the
// exact game window is preserved across fullscreen Alt-Tab and minimize.
template <class ProcessReader>
inline HWND resolveYrppWindow(ProcessReader& reader,DWORD pid,HWND fallback=nullptr,DWORD* outThread=nullptr,bool* fromYrpp=nullptr){
 if(fromYrpp)*fromYrpp=false;
 uint32_t rawHwnd=0;
 if(pid&&windowRead(reader,YrppGameHwndAddress,&rawHwnd,sizeof rawHwnd,0)&&rawHwnd){
  HWND w=reinterpret_cast<HWND>(static_cast<uintptr_t>(rawHwnd));
  DWORD wpid=0;DWORD wtid=GetWindowThreadProcessId(w,&wpid);
  if(wtid&&wpid==pid){
   if(outThread)*outThread=wtid;
   if(fromYrpp)*fromYrpp=true;
   return w;
  }
 }
 if(fallback){
  DWORD wpid=0;DWORD wtid=GetWindowThreadProcessId(fallback,&wpid);
  if(wtid&&(!pid||wpid==pid)){
   if(outThread)*outThread=wtid;
   return fallback;
  }
 }
 if(outThread)*outThread=0;
 return nullptr;
}
// Description keywords, remembered programs and known names are discovery
// alternatives. None authorizes a Runtime patch or replaces layout validation.
struct FallbackContext {
 Scan* scan;std::unordered_set<DWORD>* ids;targetselection::Policy policy;
 std::unordered_set<DWORD> checked,described;
 DWORD trustedPid=0;
};
inline void considerProcess(const targetselection::ProcessInfo& info,FallbackContext& ctx){
 if(!ctx.described.insert(info.pid).second)return;
 ++ctx.scan->descriptionCandidates;
 if(info.descriptionPending)++ctx.scan->descriptionPending;
 if(info.identityError){++ctx.scan->queryErrors;ctx.scan->lastQueryError=info.identityError;}
 if(info.descriptionError){++ctx.scan->descriptionErrors;ctx.scan->lastDescriptionError=info.descriptionError;}
 const bool description=targetselection::matchesKeywords(info.description,ctx.policy.keywords);
 const bool name=isGameName(info.name.c_str())||targetselection::matchesKeywords(info.name,ctx.policy.keywords);
 const bool remembered=targetselection::remembered(info,ctx.policy);
 if(description)++ctx.scan->descriptionMatches;
 if(name)++ctx.scan->nameCandidates;
 if(remembered)++ctx.scan->rememberedMatches;
 if(description||name||remembered)ctx.ids->insert(info.pid);
 // An already bound caller supplies its own process/birth lifetime guard.
 // Re-resolving that lease's window must survive later allowlist edits.
 if(info.pid==ctx.trustedPid&&!info.identityError)ctx.ids->insert(info.pid);
}
inline void considerFingerprint(DWORD pid,FallbackContext& ctx){
 if(ctx.ids->count(pid)||!ctx.checked.insert(pid).second)return;
 ++ctx.scan->fingerprintCandidates;
 if(imageidentity::processMatches(pid)){++ctx.scan->fingerprintMatches;ctx.ids->insert(pid);}
 else ++ctx.scan->fingerprintMismatches;
}
inline BOOL CALLBACK checkFallbackWindow(HWND w,LPARAM value){
 auto* ctx=reinterpret_cast<FallbackContext*>(value);
 DWORD pid=0;GetWindowThreadProcessId(w,&pid);
 if(!pid||ctx->ids->count(pid)||pid==GetCurrentProcessId())return TRUE;
 if(!ctx->described.count(pid))considerProcess(targetselection::inspectCachedProcess(pid),*ctx);
 considerFingerprint(pid,*ctx);return TRUE;
}
struct TreeContext {FallbackContext* fallback;WindowContext* windows;};
inline BOOL CALLBACK checkTreeWindow(HWND w,LPARAM value){
 auto* ctx=reinterpret_cast<TreeContext*>(value);
 checkFallbackWindow(w,reinterpret_cast<LPARAM>(ctx->fallback));
 return checkWindow(w,reinterpret_cast<LPARAM>(ctx->windows));
}
inline BOOL CALLBACK checkTopLevelWindow(HWND w,LPARAM value){
 checkTreeWindow(w,value);
 // EnumChildWindows already visits every descendant. Call it once per root,
 // never recursively per child. A platform may keep the game renderer in a
 // different process under a background host while the toolbox has focus.
 EnumChildWindows(w,checkTreeWindow,value);
 return TRUE;
}
inline Scan scan(DWORD preferredPid=0){
 Scan result{};
 if(preferredPid==GetCurrentProcessId())return result;
 std::unordered_set<DWORD> ids;
 FallbackContext fallback{&result,&ids,targetselection::current(),{}, {},preferredPid};
 const auto processes=targetselection::enumerateCandidates(&result.snapshotError);
 result.enumeratedProcesses=static_cast<DWORD>(processes.size());
 for(const auto& process:processes){
  considerProcess(process,fallback);
  if(imageidentity::randomName(process.name.c_str()))considerFingerprint(process.pid,fallback);
 }
 // An explicit argument belongs to an already bound reader/retired lease and
 // always takes precedence over a newer UI selection. Session preference is
 // birth-bound and never persisted as a PID or an exclusive target.
 if(preferredPid&&!fallback.described.count(preferredPid))considerProcess(targetselection::inspectCachedProcess(preferredPid),fallback);
 if(!preferredPid&&fallback.policy.selectedPid&&!fallback.described.count(fallback.policy.selectedPid))
  considerProcess(targetselection::inspectCachedProcess(fallback.policy.selectedPid),fallback);
 const DWORD selected=!preferredPid&&ids.count(fallback.policy.selectedPid)&&targetselection::preferredAlive(fallback.policy)?fallback.policy.selectedPid:0;
 const DWORD effectivePreferred=preferredPid?preferredPid:selected;
 // Inspect all top-level trees, even when a fixed-name instance exists.
 // Identity checking is once per PID, not once per system/control child HWND;
 // the existing creation/path cache also bounds repeated file reads.
 WindowContext ctx{&result,&ids,effectivePreferred};
 TreeContext tree{&fallback,&ctx};
 EnumWindows(checkTopLevelWindow,reinterpret_cast<LPARAM>(&tree));
 result.processCount=static_cast<DWORD>(ids.size());
 if(!ids.empty()){
  if(!result.best.window){
   DWORD candidate=0;
   if(effectivePreferred && ids.count(effectivePreferred))candidate=effectivePreferred;
   else if(!effectivePreferred&&ids.size()==1)candidate=*ids.begin();
   if(candidate){result.best.pid=candidate;result.best.thread=0;
    result.uniqueWindowless=ids.size()==1;}
  }
 }
 result.explicitSelection=selected&&result.best.pid==selected;
 return result;
}
} }

