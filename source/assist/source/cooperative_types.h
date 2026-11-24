#pragma once
#include <stdint.h>
#include <vector>
namespace assist { namespace cooperative {
constexpr int MaxSources=1024,MaxTargets=1024;
enum Mode { Enter=1,AirFirst=2,AirSecond=3,AirDamage=4,AirSingle=5 };
constexpr int DefaultDamageRadiusCells=5,DefaultDamagePercent=120;
constexpr int MinDamageRadiusCells=1,MaxDamageRadiusCells=64,MinDamagePercent=1,MaxDamagePercent=500;
constexpr int DamageRadius=DefaultDamageRadiusCells*256; // legacy test/default boundary
inline int radiusCells(uint32_t value){return value?int(value):DefaultDamageRadiusCells;}
inline int damagePercent(uint32_t value){return value?int(value):DefaultDamagePercent;}
inline bool validRange(uint32_t radius,uint32_t percent){return radiusCells(radius)>=MinDamageRadiusCells&&radiusCells(radius)<=MaxDamageRadiusCells&&damagePercent(percent)>=MinDamagePercent&&damagePercent(percent)<=MaxDamagePercent;}
constexpr double MaximumExpectedDamage=1000000000.0;
struct Source {uint32_t id,token;};
// AirDamage freezes the clicked target's WORLD XY in cameraX/Y. Other modes
// retain their original click-time screen projection and viewport bounds.
struct Request {uint32_t serial,match,tick,target,type,pointerRequest;int mode,count,cameraX,cameraY,viewWidth,viewHeight;Source sources[MaxSources];uint32_t centerObject=0;uint32_t airRadiusCells=0,airDamagePercent=0; // frozen at click; zero decodes as legacy 5/120 for older tests
};
// One pointer mailbox is processed on the game thread and may need more than
// one frame when some selected actors are temporarily unresolved. Its request
// serial, not the simulation frame, defines the click-scoped source batch.
inline bool sameInputRequest(const Request&r,uint32_t currentInput,
                             uint32_t match,uint32_t target,int mode,uint32_t radius,uint32_t percent){
 return currentInput&&r.pointerRequest==currentInput&&r.serial&&r.match==match&&
  r.target==target&&r.mode==mode&&
  (mode!=AirDamage||(r.airRadiusCells==radius&&r.airDamagePercent==percent));
}
struct Assignment {uint32_t id,token,target,type;double expectedDamage=0;};
struct Quota {int first=1,second=2,maximum=0;}; // zero maximum = no limit
// Keep only the unprocessed suffix, preserving the original click timestamp,
// source tokens, bounds and serial. Accepted/invalid entries are never replayed.
inline bool retryRemaining(Request&r,const Assignment*orders,int count,int processed){
 if(!orders||count<0||count>MaxSources||processed<0||processed>count){r.count=0;return false;}
 r.count=0;for(int i=processed;i<count;++i)r.sources[r.count++]={orders[i].id,orders[i].token};
 return r.count!=0;
}
// Damage allocation can intentionally leave sources unassigned. Preserve the
// original pool on a replan and remove only actually processed identities,
// rather than losing every unused replacement with the old order suffix.
inline bool retrySourcePool(Request&r,const Request&original,const std::vector<Assignment>&processed){
 r=original;if(original.count<0||original.count>MaxSources||processed.size()>MaxSources){r.count=0;return false;}
 r.count=0;for(int i=0;i<original.count;++i){bool done=false;for(const auto&o:processed)if(o.id==original.sources[i].id&&o.token==original.sources[i].token){done=true;break;}if(!done)r.sources[r.count++]=original.sources[i];}
 return r.count!=0;
}
inline Quota bounded(Quota q){if(q.first<0)q.first=0;if(q.first>512)q.first=512;if(q.second<0)q.second=0;if(q.second>512)q.second=512;if(q.maximum<0)q.maximum=0;if(q.maximum>512)q.maximum=512;return q;}
inline bool damageMode(int mode){return mode==AirDamage||mode==AirSingle;}
inline bool valid(const Request&r){return r.serial&&r.match&&r.pointerRequest&&r.count>=0&&r.count<=MaxSources&&r.mode>=Enter&&r.mode<=AirSingle&&(!damageMode(r.mode)||r.centerObject>=0x10000)&&(r.mode!=AirDamage||validRange(r.airRadiusCells,r.airDamagePercent))&&r.viewWidth>0&&r.viewWidth<=32768&&r.viewHeight>0&&r.viewHeight<=32768&&r.cameraX>=-1048576&&r.cameraX<=1048576&&r.cameraY>=-1048576&&r.cameraY<=1048576;}
} }
