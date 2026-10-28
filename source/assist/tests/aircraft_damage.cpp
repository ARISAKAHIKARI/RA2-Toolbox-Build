#include "../source/cooperative_plan.h"
#include <cassert>
#include <climits>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
using namespace assist::cooperative;

static Input scene(int sources,int targets,int mode=AirDamage){
 Input w;w.mode=mode;w.clicked=100;
 for(int i=0;i<sources;++i)w.units.push_back({{uint32_t(i+1),uint32_t(20+i)},1,0,0,1});
 for(int j=0;j<targets;++j){Target t{};t.id=100+j;t.type=1000+j;t.x=j*256;t.free=3;t.quota={1,2,0};t.hp=100;t.kind=2;t.enemy=true;w.targets.push_back(t);}
 w.compatible.assign(size_t(sources)*targets,1);w.expectedDamage.assign(size_t(sources)*targets,60);return w;
}
static uint32_t targetOf(const std::vector<Assignment>&orders,uint32_t source){for(const auto&o:orders)if(o.id==source)return o.target;return 0;}
static std::map<uint32_t,int> counts(const std::vector<Assignment>&orders){std::map<uint32_t,int> out;for(const auto&o:orders)++out[o.target];return out;}

int main(){
 // The click gets enough per-source estimated damage first; the next aircraft
 // changes target at exact 120%, rather than balancing aircraft counts.
 auto w=scene(4,2);auto orders=allocate(w);assert(orders.size()==4);
 assert(targetOf(orders,1)==100&&targetOf(orders,2)==100&&targetOf(orders,3)==101&&targetOf(orders,4)==101);
 for(const auto&o:orders)assert(o.expectedDamage==60&&o.token==19+o.id);
 w=scene(3,3);for(auto&u:w.units)u.x=512;orders=allocate(w);assert(targetOf(orders,1)==100&&targetOf(orders,2)==100&&targetOf(orders,3)==102&&"fill the clicked target before a nearer uncovered target");
 w=scene(4,2);
 w.expectedDamage.assign(8,70);orders=allocate(w);assert(targetOf(orders,1)==100&&targetOf(orders,2)==100&&targetOf(orders,3)==101&&"the aircraft crossing 120% is allowed");
 w=scene(1,2);w.targets[0].priorDamage=120;assert(targetOf(allocate(w),1)==101);
 w.targets[0].priorDamage=119.999;assert(targetOf(allocate(w),1)==100);
 w=scene(3,2);w.expectedDamage={70,10,50,100,20,80};orders=allocate(w);
 assert(orders.size()==3&&targetOf(orders,1)==100&&targetOf(orders,2)==100&&targetOf(orders,3)==101);
 assert(orders[0].expectedDamage==70&&orders[1].expectedDamage==50&&orders[2].expectedDamage==80);

 // Covered fallback is per aircraft: an uncovered target it cannot attack
 // cannot strand this aircraft, while any attackable uncovered option wins.
 w=scene(1,2);w.targets[0].priorDamage=120;w.compatible={1,0};assert(targetOf(allocate(w),1)==100);
 w.compatible[1]=1;assert(targetOf(allocate(w),1)==101);
 w.expectedDamage[1]=0;assert(targetOf(allocate(w),1)==100&&"a zero-damage option cannot prevent covered fallback");w.expectedDamage[1]=60;
 w.targets[1].priorDamage=120;assert(allocate(w).size()==1);
 w.targets[0].priorDamage=180;assert(targetOf(allocate(w),1)==101);

 // Radius is a circle in world XY, including exactly five cells. A square
 // corner and a one-unit step outside the circle are not candidates.
 for(auto xy:{std::pair<int,int>{DamageRadius,0},{768,1024},{-768,-1024}}){w=scene(1,1);w.centerX=100;w.centerY=-200;w.targets[0].x=w.centerX+xy.first;w.targets[0].y=w.centerY+xy.second;assert(allocate(w).size()==1);}
 for(auto xy:{std::pair<int,int>{DamageRadius+1,0},{769,1024},{DamageRadius,DamageRadius}}){w=scene(1,1);w.targets[0].x=xy.first;w.targets[0].y=xy.second;assert(allocate(w).empty());}
 for(int kind:{-1,0,1,3}){w=scene(1,1);w.targets[0].kind=kind;assert(allocate(w).empty());}
 w=scene(1,1);w.targets[0].enemy=false;assert(allocate(w).empty());
 w=scene(1,1);w.targets[0].airborne=true;assert(allocate(w).empty());
 w=scene(1,1);w.targets[0].hp=0;assert(allocate(w).empty());

 // A single-target request has no covered fallback and never redirects to
 // another target. Sources omitted from the result receive no new order.
 w=scene(5,2,AirSingle);w.targets[0].kind=3;w.targets[0].x=5000;orders=allocate(w);
 assert(orders.size()==2&&targetOf(orders,1)==100&&targetOf(orders,2)==100);
 assert(!targetOf(orders,3)&&!targetOf(orders,4)&&!targetOf(orders,5));
 w.expectedDamage.assign(10,70);orders=allocate(w);assert(orders.size()==2&&orders[1].expectedDamage==70);
 w.targets[0].priorDamage=120;assert(allocate(w).empty());
 w.targets[0].priorDamage=119.999;assert(allocate(w).size()==1);
 w.clicked=999;assert(allocate(w).empty());
 w=scene(4,1,AirSingle);w.expectedDamage={40,130,80,10};orders=allocate(w);assert(orders.size()==1&&orders[0].id==2&&orders[0].expectedDamage==130);
 w.targets[0].hp=200;w.expectedDamage={90,140,110,40};orders=allocate(w);assert(orders.size()==2&&orders[0].id==2&&orders[1].id==3&&"larger valid volleys minimize the necessary subset");

 // Bad edges are excluded individually. Their presence cannot justify a
 // nominal attack or contaminate an independent valid candidate.
 const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
 for(double bad:{0.0,-1.0,nan,inf,-inf,MaximumExpectedDamage+1}){
  w=scene(1,1);w.expectedDamage[0]=bad;assert(allocate(w).empty());
  w=scene(1,2);w.expectedDamage[0]=bad;assert(targetOf(allocate(w),1)==101);
 }
 w=scene(1,1);w.expectedDamage[0]=MaximumExpectedDamage;assert(allocate(w).size()==1);
 for(double bad:{-1.0,nan,inf,MaximumExpectedDamage*MaxSources+1}){w=scene(1,1);w.targets[0].priorDamage=bad;assert(allocate(w).empty());}
 w=scene(1,1);w.expectedDamage.clear();assert(allocate(w).empty());
 w=scene(1,1);w.compatible.clear();assert(allocate(w).empty());
 w=scene(1,1);w.centerX=INT_MAX;assert(allocate(w).empty());
 w=scene(1,1);w.units[0].y=INT_MIN;assert(allocate(w).empty());

 // Replanning must retain accepted orders' original damage estimate even
 // after source ammo/weapon estimates change. Rejected orders add no reserve.
 auto first=scene(3,2);orders=allocate(first);std::vector<Assignment> accepted(orders.begin(),orders.begin()+2);
 w=scene(1,2);w.units[0].source={3,22};w.expectedDamage={1,250};reserveAccepted(w,accepted);
 assert(w.targets[0].prior==2&&w.targets[0].priorDamage==120&&targetOf(allocate(w),3)==101);
 w=scene(1,2);w.units[0].source={3,22};reserveAccepted(w,{accepted[0]});assert(w.targets[0].priorDamage==60&&targetOf(allocate(w),3)==100);
 w=scene(1,2);++w.targets[0].type;reserveAccepted(w,accepted);assert(!w.targets[0].prior&&!w.targets[0].priorDamage);
 w=scene(1,1,AirSingle);w.units[0].source={3,22};reserveAccepted(w,accepted);assert(w.targets[0].priorDamage==120&&allocate(w).empty());
 w=scene(3,1,AirSingle);w.targets[0].hp=50;reserveAccepted(w,{accepted[0]});assert(w.targets[0].priorDamage==60&&allocate(w).empty()&&"HP drop cannot send another aircraft after accepted damage is sufficient");
 w=scene(1,1);accepted[0].expectedDamage=nan;reserveAccepted(w,{accepted[0]});assert(!w.targets[0].priorDamage);

 // Originally unassigned candidates must survive replan as replacements;
 // only an actually processed identity with its original token is removed.
 Request original{};original.serial=7;original.match=9;original.tick=100;original.mode=AirSingle;original.count=4;original.centerObject=0x123400;
 for(int i=0;i<4;++i)original.sources[i]={uint32_t(1+i),uint32_t(20+i)};
 Request remaining{};assert(retrySourcePool(remaining,original,{{1,20,100,1000,60}})&&remaining.count==3&&remaining.sources[0].id==2&&remaining.sources[1].id==3&&remaining.sources[2].id==4);
 assert(remaining.serial==original.serial&&remaining.tick==original.tick&&remaining.centerObject==original.centerObject);
 assert(retrySourcePool(remaining,original,{{1,999,100,1000,60}})&&remaining.count==4&&"a different manual token is a different source identity");
 assert(!retrySourcePool(remaining,original,{{1,20,0,0},{2,21,0,0},{3,22,0,0},{4,23,0,0}})&&remaining.count==0);

 // Existing count-quota and entry allocation do not consume damage-only
 // metadata, and still obey their original counts and capacity behavior.
 w=scene(7,3,AirFirst);w.expectedDamage.clear();for(auto&t:w.targets){t.priorDamage=nan;t.enemy=false;}
 auto n=counts(allocate(w));assert(n[100]==3&&n[101]==2&&n[102]==2);
 w=scene(6,3,AirSecond);n=counts(allocate(w));assert(n[100]==2&&n[101]==2&&n[102]==2);
 for(auto&t:w.targets)t.quota.maximum=1;assert(allocate(w).size()==3);
 w=scene(3,2,Enter);w.units[0].slots=3;w.units[1].slots=w.units[2].slots=2;w.targets[0].free=4;w.targets[1].free=3;orders=allocate(w);assert(orders.size()==3);
 std::map<uint32_t,int> occupied;for(const auto&o:orders){occupied[o.target]+=w.units[o.id-1].slots;assert(!o.expectedDamage);}assert(occupied[100]<=4&&occupied[101]<=3);
 std::cout<<"PASS aircraft damage allocation: click priority, exact/crossed 120%, per-aircraft fallback, circular boundary and enemy/type filters, per-edge validity, single-target stop, accepted replan reserves and legacy modes\n";
}
