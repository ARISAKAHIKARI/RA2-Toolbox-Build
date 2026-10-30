#pragma once
#include "core.h"
namespace um {
// Sampling retries, source pointers and unused capacity are not visible data.
// A new immutable snapshot still carries fresh timestamps/chat/diagnostics.
inline bool samePresentation(const Snapshot&a,const Snapshot&b){
 if(a.match!=b.match||a.status!=b.status||a.typeCount!=b.typeCount||a.playerCount!=b.playerCount||a.paletteReady!=b.paletteReady)return false;
 if(a.paletteReady&&memcmp(a.palette,b.palette,sizeof a.palette))return false;
 for(int i=0;i<a.typeCount;++i){const auto&x=a.types[i];const auto&y=b.types[i];
  if(x.category!=y.category||memcmp(x.id,y.id,sizeof x.id)||memcmp(x.cameo,y.cameo,sizeof x.cameo)||bool(x.iconReady)!=bool(y.iconReady))return false;
  if(x.iconReady&&memcmp(x.icon,y.icon,sizeof x.icon))return false;
 }
 for(int i=0;i<a.playerCount;++i){const auto&x=a.players[i];const auto&y=b.players[i];
  if(x.color!=y.color||x.houseIndex!=y.houseIndex||x.local!=y.local||x.credits!=y.credits||x.creditsKnown!=y.creditsKnown||memcmp(x.name,y.name,sizeof x.name)||memcmp(x.cells,y.cells,sizeof(Cell)*a.typeCount))return false;
 }
 return true;
}
}
