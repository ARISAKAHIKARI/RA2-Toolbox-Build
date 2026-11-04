#pragma once
namespace framesignal {
// Normal compute/render clients detect frame-number changes without writing
// game code. Only the explicitly selected high-risk diagnostic owns this site.
// Diagnostic selection still cannot bypass readiness or signature checks.
constexpr bool nativeAllowed(int session,bool explicitDiagnostic){
 return explicitDiagnostic&&(session==0||session==5||session==3||session==4);
}
}
