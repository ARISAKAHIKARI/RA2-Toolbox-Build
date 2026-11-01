#pragma once
namespace framesignal {
// A user-supplied RA2OL run first OOSed with only CommitSite installed.
// Normal multiplayer uses the existing message/polling fallback. The test
// host can still explicitly isolate this site; no integrity-check bypass.
constexpr bool nativeAllowed(int session,bool explicitDiagnostic){
 return session==0||session==5||((session==3||session==4)&&explicitDiagnostic);
}
}
