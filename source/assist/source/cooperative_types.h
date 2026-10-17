#pragma once
#include <stdint.h>
namespace assist { namespace cooperative {
constexpr int MaxSources=512,MaxTargets=512;
enum Mode { Enter=1,AirFirst=2,AirSecond=3 };
struct Source {uint32_t id,token;};
struct Request {uint32_t serial,match,tick,target,type;int frame,mode,count,cameraX,cameraY,viewWidth,viewHeight;Source sources[MaxSources];};
struct Assignment {uint32_t id,token,target,type;};
struct Quota {int first=1,second=2,maximum=0;}; // zero maximum = no limit
// Keep only the unprocessed suffix, preserving the original click timestamp,
// source tokens, bounds and serial. Accepted/invalid entries are never replayed.
inline bool retryRemaining(Request&r,const Assignment*orders,int count,int processed){
 if(!orders||count<0||count>MaxSources||processed<0||processed>count){r.count=0;return false;}
 r.count=0;for(int i=processed;i<count;++i)r.sources[r.count++]={orders[i].id,orders[i].token};
 return r.count!=0;
}
inline Quota bounded(Quota q){if(q.first<0)q.first=0;if(q.first>512)q.first=512;if(q.second<0)q.second=0;if(q.second>512)q.second=512;if(q.maximum<0)q.maximum=0;if(q.maximum>512)q.maximum=512;return q;}
inline bool valid(const Request&r){return r.serial&&r.match&&r.count>=0&&r.count<=MaxSources&&r.mode>=Enter&&r.mode<=AirSecond&&r.viewWidth>0&&r.viewWidth<=32768&&r.viewHeight>0&&r.viewHeight<=32768&&r.cameraX>=-1048576&&r.cameraX<=1048576&&r.cameraY>=-1048576&&r.cameraY<=1048576;}
} }
