#pragma once
namespace framesignal {
// Preserve the original supported session modes. Diagnostic selection is
// metadata, not an override required to enable native multiplayer sampling.
// The caller still checks local readiness, ownership and exact signatures.
constexpr bool nativeAllowed(int session,bool /*explicitDiagnostic*/){
 return session==0||session==5||session==3||session==4;
}
}
