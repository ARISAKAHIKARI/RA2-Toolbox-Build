#include "feature_logic_headers.h"
#include <cstdio>
#include <memory>
#include <map>
#include <set>
#include <tuple>
#include <chrono>
using namespace assist;
static int checks=0,failures=0;
static void check(bool value,const char*label){++checks;if(!value){++failures;std::fprintf(stderr,"FAIL %s\n",label);}}
static uint64_t digest(const featureformation::Slot*slots,int count){uint64_t h=1469598103934665603ull;for(int n=0;n<count;++n)for(uint32_t v:{uint32_t(slots[n].x),uint32_t(slots[n].y),uint32_t(slots[n].z),slots[n].valid})h=(h^v)*1099511628211ull;return h;}
int main(){
 using namespace featureformation;
 Unit u[3]{};u[0].x=128;u[1].x=1024;u[2].x=768;
 Slot slots[3]={{0,0,0,0},{512,0,0,1},{1024,0,0,1}};
 assign(u,3,slots);
 check(slots[0].valid&&slots[0].x==512,"an invalid zero slot cannot displace an earlier eligible unit");
 check(slots[1].valid&&slots[1].x==1024&&!slots[2].valid,"valid slots retain stable selection order");
 for(int mode=0;mode<7;++mode){
  auto world=std::make_unique<planner::World>();auto&w=*world;w.match=1;w.frame=1;w.tick=100;w.count=3;
  auto&a=w.actors[0];a.id=1;a.kind=3;a.flags=planner::Own|planner::PointDefense;a.x=a.y=50*256;a.target=2;a.range=10*256;
  a.weapons[0].range=a.range;a.weapons[0].flags=planner::GroundWeapon;a.weapons[0].armorMask=1;
  auto&old=w.actors[1];old.id=2;old.kind=1;old.flags=planner::Enemy|planner::Saboteur;old.x=52*256;old.y=a.y;old.armor=0;
  auto&near=w.actors[2];near=old;near.id=3;near.x=51*256;
  switch(mode){case 1:old.x=70*256;break;case 2:old.flags|=planner::Cloaked;break;case 3:old.flags|=planner::Protected;break;case 4:old.flags|=planner::Transported;break;case 5:old.armor=1;break;case 6:old.z=20*256;break;}
  planner::Planner p;auto orders=p.step(w,AntiTheft,200,300,90);
  if(!mode)check(orders.empty(),"do not reset a valid current defense shot");
  else check(orders.size()==1&&orders[0].target==3&&orders[0].purpose==planner::DefenseOrder,"replace a stale/ineligible/out-of-range infiltrator");
 }
 static Unit units[Maximum];static Slot placed[Maximum];static bool air[Maximum];
 for(int n=0;n<Maximum;++n){units[n].kind=n%3==0?1:2;units[n].x=(n%11)*256;units[n].y=(n/11)*256;air[n]=n%3==2;}
 for(int scenario=0;scenario<4;++scenario){
  quick::Path path;path.push(20,20);if(scenario==1){path.push(45,20);path.push(45,45);}
  std::map<std::tuple<int,int,bool>,int> visits;int queries=0;
  auto legal=[&](int x,int y,bool flying){return x>0&&y>0&&x<80&&y<80&&(flying||x!=20||y!=20)&&(scenario!=2||x%3!=0)&&scenario!=3;};
  auto start=std::chrono::steady_clock::now();
  quick::layout(units,Maximum,air,path,20,20,placed,[&](int x,int y,bool flying){++queries;++visits[{x,y,flying}];return legal(x,y,flying);});
  auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  std::map<std::tuple<int,int,int>,int> used;bool valid=true,once=true;
  for(const auto&v:visits)once&=v.second==1;
  for(int n=0;n<Maximum;++n)if(placed[n].valid){int x=placed[n].x/256,y=placed[n].y/256,c=quick::category(units[n],air[n]);valid&=legal(x,y,air[n])&&++used[{x,y,c}]<=quick::quota(c);}
  check(valid,"quick placement preserves legal cells and per-class quotas");
  check(once,"each cell layer is queried only once in a solve");
  std::printf("quick scenario=%d queries=%d unique=%zu digest=%llu solve_us=%lld\n",scenario,queries,visits.size(),(unsigned long long)digest(placed,Maximum),(long long)us);
 }
 std::printf("%s feature logic policy: %d checks, %d failures\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;
}
