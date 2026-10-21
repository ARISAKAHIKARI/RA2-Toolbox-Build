#include "reverse_dodge.h"
// Read-only projectile/terrain observation; movement goes through nativeMission.
// Never call Scatter, SetDestination, or alter a projectile on this peer.
struct ReverseShot {uint32_t id=0,pointer=0;int frame=0;bool handled=false;};
static ReverseShot reverseShots[16384];static uint32_t reverseMatch=0;static bool reverseWasEnabled=false;
static ReverseShot* reverseShot(uint32_t p,int frame){
 uint32_t id=get<uint32_t>(p+0x10);if(!id)return nullptr;
 unsigned slot=(id*2654435761u) & 16383u;
 for(unsigned n=0;n<16384;++n){auto&s=reverseShots[(slot+n)&16383u];
  if(s.id==id&&s.pointer==p)return &s;
  if(!s.id||frame-s.frame>900){s={id,p,frame,false};return &s;}
 }return nullptr;
}
static bool reverseVector(uint32_t q,reversedodge::Point&delta){
 // BulletClass Velocity E8/F0; SourceCoords 134/138, TargetCoords 140/144.
 if(reversedodge::vector(get<double>(q+0xE8),get<double>(q+0xF0),delta))return true;
 return reversedodge::vector(double(get<int>(q+0x140))-get<int>(q+0x134),
                             double(get<int>(q+0x144))-get<int>(q+0x138),delta);
}
static bool reverseDodgeActor(uint32_t p,uint32_t q,uint32_t house,int frame,DWORD now,reversedodge::Point delta){
 if(!dodgeSource(p,house))return true;
 dodgecompute::Source s{};dodgeObserve(p,house,s);int j=dodgeIndex(s.id);uint32_t shot=get<uint32_t>(q+0x10);
 if(j>=0&&dodgeSeen(dodgeJobs[j],shot))return true;
 // Nearby shots, including later frames, belong to the accepted dodge. Consume
 // them now, not as deferred turns. Suppression does not extend the window.
 if(j>=0&&dodgeJobs[j].reverseFrame>=0&&frame>=dodgeJobs[j].reverseFrame&&
    frame-dodgeJobs[j].reverseFrame<=reversedodge::MergeFrames){dodgeRemember(dodgeJobs[j],s);return true;}
 if(!outgoingRoom())return false;
 uint32_t cell=reversedodge::destination(get<int>(p+0x9C),get<int>(p+0xA0),delta,[&](int x,int y,int direction){
  uint32_t c=readMapCell(x,y);if(!c)return false;
  vanilla::Cell xy{short(x),short(y)};
  if(!call<bool>(0x578460,0x87F7E8,&xy,1))return false;
  return call<int>(get<uint32_t>(get<uint32_t>(p)+0x1AC),p,c,direction,call<int>(0x5F5F00,p),0,1)==0;
 });
 if(!cell)return true; // No nearby passable cell in the requested direction.
 j=createDodge(s);if(j<0)return false;
 uint32_t ticket=0;
 if(!nativeMission(house,s.id,2,0,cell,11,0,0,taskarbiter::Owner::Dodge,&ticket))return false;
 auto&job=dodgeJobs[j];if(job.active)finishDodge(job,true);
 gateway::Receipt receipt{};
 for(int k=0;k<missionObserver.count;++k)if(missionObserver.tickets[k].receipt.serial==ticket){receipt=missionObserver.tickets[k].receipt;break;}
 missionObserver.dismissTicket(match,ticket); // Dodge observer owns this receipt now.
 job.active=true;job.observed=false;job.sentFrame=job.quietFrame=frame;
 job.reverseFrame=frame;
 job.x=get<int>(p+0x9C);job.y=get<int>(p+0xA0);job.marker=get<bool>(p+0x6B2);job.receipt=receipt;
 dodgeRemember(job,s);++sh->dodgeIssued;cancelBurst(s.id);return true;
}
static bool reverseDodgeTick(uint32_t house,int frame,DWORD now){
 if(reverseMatch!=match||!reverseWasEnabled){memset(reverseShots,0,sizeof reverseShots);reverseMatch=match;}
 reverseWasEnabled=true;
 // Scan projectile targets first, avoiding a 32-unit rotating scan delay.
 for(int n=0;n<dodgeThreatCount;++n){const auto&ref=dodgeThreatRefs[n];if(!ref.projectile)continue;
  auto*shot=reverseShot(ref.pointer,frame);if(!shot||shot->handled)continue;
  if(frame-shot->frame>8){shot->handled=true;continue;} // stale queue/ownership retry
  reversedodge::Point delta;if(!reverseVector(ref.pointer,delta)){shot->handled=true;continue;}
  bool done=true;
  if(worldObjects.contains(ref.target))done=reverseDodgeActor(ref.target,ref.pointer,house,frame,now,delta);
  else {Vec all=vec(0xA8EC78);for(int k=0;k<all.count;++k){uint32_t p=item(all,k);if(!dodgeSource(p,house))continue;
   if(ref.target==readMapCell(get<int>(p+0x9C)/256,get<int>(p+0xA0)/256))done=reverseDodgeActor(p,ref.pointer,house,frame,now,delta)&&done;
  }}
  shot->handled=done;
 }
 // Preserve crushing avoidance and finish existing movements, using the same
 // synchronous entry. Projectile handling above deliberately ignores busy/active.
 auto&i=dodgeInput;i={};i.match=match;i.tick=now;i.frame=frame;
 for(int n=0;n<dodgeCount&&i.count<dodgecompute::MaxSources;++n)if(dodgeJobs[n].active)dodgeObserve(dodgeJobs[n].pointer,house,i.sources[i.count++]);
 for(int n=0;n<dodgeThreatCount&&i.count<dodgecompute::MaxSources;++n){const auto&ref=dodgeThreatRefs[n];if(ref.projectile||!dodgeSource(ref.target,house))continue;
  bool found=false;for(int k=0;k<i.count;++k)if(i.sources[k].pointer==ref.target)found=true;
  if(!found)dodgeObserve(ref.target,house,i.sources[i.count++]);
 }
 // Never turn a projectile-only failed reverse move into random Scatter.
 for(int n=0;n<i.count;++n)if(!i.sources[n].active){bool crush=false;for(int k=0;k<dodgeThreatCount;++k)if(!dodgeThreatRefs[k].projectile&&dodgeThreatRefs[k].target==i.sources[n].pointer&&dodgeThreat(i.sources[n].pointer,dodgeThreatRefs[k].pointer,house))crush=true;if(!crush)i.sources[n].freshThreat=0;}
 applyDodgeResult(dodgecompute::calculate(i),house,frame,now);
 sh->dodgeActive=0;for(int n=0;n<dodgeCount;++n)if(dodgeJobs[n].active)++sh->dodgeActive;
 return true;
}
