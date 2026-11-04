#pragma once
#include <stdint.h>
#include <vector>
namespace assist { namespace cooperative {
constexpr int MaxSources=1024,MaxTargets=1024;
enum Mode { Enter=1,AirFirst=2,AirSecond=3,AirDamage=4,AirSingle=5 };
constexpr int DamageRadius=5*256;
constexpr double MaximumExpectedDamage=1000000000.0;
struct Source {uint32_t id,token;};
// AirDamage freezes the clicked target's WORLD XY in cameraX/Y. Other modes
// retain their original click-time screen projection and viewport bounds.
struct Request {uint32_t serial,match,tick,target,type;int frame,mode,count,cameraX,cameraY,viewWidth,viewHeight;Source sources[MaxSources];uint32_t centerObject=0;};
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
inline bool valid(const Request&r){return r.serial&&r.match&&r.count>=0&&r.count<=MaxSources&&r.mode>=Enter&&r.mode<=AirSingle&&(!damageMode(r.mode)||r.centerObject>=0x10000)&&r.viewWidth>0&&r.viewWidth<=32768&&r.viewHeight>0&&r.viewHeight<=32768&&r.cameraX>=-1048576&&r.cameraX<=1048576&&r.cameraY>=-1048576&&r.cameraY<=1048576;}
} }
