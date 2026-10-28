#pragma once
#include "game_view.h"
#include "projection.h"
#include "../../assist/source/cooperative_plan.h"
#include "cooperative_damage.h"
#include <unordered_map>
namespace external {
template<class Read,class Quotas> assist::cooperative::Input cooperativeInput(Read read,const assist::cooperative::Request&r,Quotas quota){
 using namespace assist;cooperative::Input out;out.mode=r.mode;out.clicked=r.target;out.centerX=r.cameraX;out.centerY=r.cameraY;if(!cooperative::valid(r))return out;
 GameView<Read> game(read);uint32_t house=game.template get<uint32_t>(0xA83D4C);int frame=game.template get<int>(0xA8ED84,-1);
 if(house<0x10000||frame<0||!game.template get<uint8_t>(0xA8E9A0))return out;
 auto all=game.vec(0xA8EC78);std::unordered_map<uint32_t,uint32_t> byID;byID.reserve(all.size());for(uint32_t p:all){uint32_t id=game.template get<uint32_t>(p+0x10);if(id)byID.emplace(id,p);}
 auto clicked=byID.find(r.target);if(clicked==byID.end()||game.type(clicked->second,game.kind(clicked->second))!=r.type||(cooperative::damageMode(r.mode)&&r.centerObject!=clicked->second))return out;
 // Freeze the screen bounds at click time, before the user scrolls again.
 Projection projection;projection.width=r.viewWidth;projection.height=r.viewHeight;projection.cameraX=r.cameraX;projection.cameraY=r.cameraY;projection.zScale=game.template get<double>(0xB0CD48,0.0866025403784);
 std::vector<uint32_t> sources,targets;std::vector<planner::Actor> sourceCombat,targetCombat;
 for(int i=0;i<r.count;++i){auto found=byID.find(r.sources[i].id);if(found==byID.end())continue;uint32_t p=found->second;int k=game.kind(p);uint32_t t=game.type(p,k);
  if(!game.live(p)||game.template get<uint32_t>(p+0x21C)!=house||game.template get<uint8_t>(p+0x1C8)||game.template get<uint32_t>(p+0x11C)||!t)continue;
  auto combat=game.combat(p,frame,r.mode!=cooperative::Enter);
  if(r.mode==cooperative::Enter){if((k!=1&&k!=2)||!game.entryCompatible(p,clicked->second,house))continue;}
  else if(!((k==0||k==2)&&(combat.flags&planner::Airborne))||(combat.flags&(planner::Manipulated|planner::TemporalTarget)))continue;
  int slots=r.mode==cooperative::Enter?game.entrySlots(p,clicked->second):1;
  uint32_t destination=r.mode==cooperative::Enter?game.entryDestination(p):0;
  uint32_t reserved=destination&&game.capacity(destination).singleVehicle&&game.radioLinked(destination,p)?game.template get<uint32_t>(destination+0x10):0;
  out.units.push_back({r.sources[i],t,game.template get<int>(p+0x9C),game.template get<int>(p+0xA0),slots,reserved});sources.push_back(p);sourceCombat.push_back(combat);
 }
 if(sources.empty())return out;
 std::vector<uint32_t> candidates;
 for(uint32_t p:all){if(!game.live(p))continue;int k=game.kind(p);uint32_t t=game.type(p,k);
  if(r.mode==cooperative::Enter){if(t!=r.type||k!=game.kind(clicked->second)||!game.capacity(p).maximum)continue;}
  else if(!game.enemy(p,house))continue;
  if(r.mode==cooperative::AirSingle){if(p!=clicked->second)continue;}
  else if(r.mode==cooperative::AirDamage){auto combat=game.combat(p,frame,false);
   if(k!=2||(combat.flags&(planner::Airborne|planner::Manipulated))||cooperative::distance(r.cameraX,r.cameraY,combat.x,combat.y)>int64_t(cooperative::DamageRadius)*cooperative::DamageRadius)continue;
  }else {int sx,sy;if(!project(projection,game.template get<int>(p+0x9C),game.template get<int>(p+0xA0),game.template get<int>(p+0xA4),sx,sy)||sx<0||sy<0||sx>=projection.width||sy>=projection.height)continue;}
  candidates.push_back(p);
 }
 // A busy screen is bounded without depending on engine vector ordering.
 std::sort(candidates.begin(),candidates.end(),[&](uint32_t a,uint32_t b){if(a==clicked->second||b==clicked->second)return a==clicked->second&&b!=clicked->second;int64_t da=cooperative::distance(out.units[0].x,out.units[0].y,game.template get<int>(a+0x9C),game.template get<int>(a+0xA0)),db=cooperative::distance(out.units[0].x,out.units[0].y,game.template get<int>(b+0x9C),game.template get<int>(b+0xA0));return da!=db?da<db:game.template get<uint32_t>(a+0x10)<game.template get<uint32_t>(b+0x10);});
 std::unordered_map<uint32_t,int> incoming;
 if(r.mode==cooperative::Enter)for(uint32_t q:all){if(!game.live(q)||game.template get<uint32_t>(q+0x21C)!=house||game.template get<uint32_t>(q+0x11C)||std::find(sources.begin(),sources.end(),q)!=sources.end())continue;
  uint32_t target=game.entryDestination(q);
  if(game.type(target,game.kind(target))!=r.type)continue;
  // Single-vehicle facilities count actual radio contacts in used already.
  if(game.capacity(target).singleVehicle&&game.radioLinked(target,q))continue;
  int slots=game.entrySlots(q,target);
  if(slots)incoming[target]+=slots;
 }
 for(uint32_t p:candidates){if(targets.size()>=cooperative::MaxTargets)break;int k=game.kind(p);uint32_t t=game.type(p,k);cooperative::Target target{};target.id=game.template get<uint32_t>(p+0x10);target.type=t;target.x=game.template get<int>(p+0x9C);target.y=game.template get<int>(p+0xA0);target.quota=quota(t);
  if(r.mode==cooperative::Enter){auto c=game.capacity(p);target.used=c.used;target.free=std::max(0,c.maximum-c.used);
   target.free=std::max(0,target.free-incoming[p]);
  }
  auto combat=game.combat(p,frame,false);target.hp=game.template get<int>(p+0x6C);target.kind=k;target.enemy=game.enemy(p,house);target.airborne=(combat.flags&planner::Airborne)!=0;
  out.targets.push_back(target);targets.push_back(p);targetCombat.push_back(combat);
 }
 out.compatible.resize(sources.size()*targets.size());
 if(cooperative::damageMode(r.mode))out.expectedDamage.resize(sources.size()*targets.size());
 for(size_t i=0;i<sources.size();++i)for(size_t j=0;j<targets.size();++j){size_t at=i*targets.size()+j;
  out.compatible[at]=r.mode==cooperative::Enter?game.entryCompatible(sources[i],targets[j],house):planner::compatibleTarget(sourceCombat[i],targetCombat[j],false);
  if(cooperative::damageMode(r.mode)){auto estimate=cooperativedamage::estimate(game,sources[i],targets[j],sourceCombat[i],targetCombat[j]);out.expectedDamage[at]=estimate.damage;out.compatible[at]=out.compatible[at]&&cooperative::validDamage(estimate.damage);}
 }
 // House changes invalidate this whole read; commands will also be checked late.
 if(house!=game.template get<uint32_t>(0xA83D4C))return cooperative::Input{};
 return out;
}
}
