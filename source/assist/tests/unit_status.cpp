#define main previous_enemy_marker_test_main
#include "enemy_markers.cpp"
#undef main
#include "../source/status_badges.h"
#include <limits>
using F=EnemyFixture;
static constexpr uint32_t attacker=0x10E000,parasiteLink=0x720000;
static void attach(F&f){
 f.object(attacker,90,F::enemy,2,F::enemyType);f.mem.put<uint8_t>(attacker+0x81,1);f.mem.put<uint8_t>(attacker+0x74,0);
 f.mem.put(F::unit+0x694,attacker);f.mem.put(attacker+0x69C,parasiteLink);f.mem.put(parasiteLink+0x24,attacker);f.mem.put(parasiteLink+0x28,F::unit);
}
static void buffs(F&f,uint32_t object=F::unit){f.mem.put<double>(object+0x160,1.5);f.mem.put<double>(object+0x158,1.5);f.mem.put<double>(object+0x580,1.2);}
static const external::status::Frame& sample(F&f,unsigned options=3){f.sampler.sample(f.now+=2,0,false,options);return f.sampler.unitStatuses();}
int main(){using namespace external::status;
 {
  F f;attach(f);buffs(f);auto checksum=f.mem.hash();auto&m=sample(f);
  assert(m.count==1&&m.items[0].id==1&&m.items[0].buffs==7&&m.items[0].parasite==attacker&&m.items[0].parasiteColor==0xFFDC5046);
  assert(f.mem.hash()==checksum&&f.sampler.sample(f.now,0,false,3).labels==0); // Host-private, no added Shared labels.
  f.mem.scans=0;f.mem.put<uint8_t>(F::ally+22265,20);f.mem.put<uint8_t>(F::ally+22266,210);f.mem.put<uint8_t>(F::ally+22267,140);
  f.mem.put(attacker+0x21C,F::ally);auto&changed=sample(f);assert(changed.count==1&&changed.items[0].parasiteOwner==F::ally&&changed.items[0].parasiteColor==0xFF14D28C&&f.mem.scans==0);
  f.mem.put<uint8_t>(F::ally+22266,200);assert(sample(f).items[0].parasiteColor==0xFF14C88C);
  auto&onlyParasite=sample(f,1);assert(onlyParasite.count==1&&!onlyParasite.items[0].buffs&&onlyParasite.items[0].parasite);
  auto&onlyBuffs=sample(f,2);assert(onlyBuffs.count==1&&onlyBuffs.items[0].buffs==7&&!onlyBuffs.items[0].parasite);
  assert(sample(f,0).count==0&&sample(f).count==1);
  std::cout<<"PASS live reciprocal parasite relation including Limbo attacker; attacker ownership/color changes in same frame; separate settings; read-only host data\n";
 }
 {
  F f;attach(f);assert(sample(f).count==1);
  f.mem.put(parasiteLink+0x28,F::foe);assert(sample(f).count==0);f.mem.put(parasiteLink+0x28,F::unit);f.next();assert(sample(f).count==1);
  f.mem.put(parasiteLink+0x24,F::foe);assert(sample(f).count==0);f.mem.put(parasiteLink+0x24,attacker);f.next();assert(sample(f).count==1);
  f.mem.put<int>(attacker+0x6C,0);assert(sample(f).count==0);f.mem.put<int>(attacker+0x6C,100);f.next();assert(sample(f).count==1);
  f.mem.put<uint8_t>(attacker+0x90,0);assert(sample(f).count==0);f.mem.put<uint8_t>(attacker+0x90,1);f.next();assert(sample(f).count==1);
  f.mem.put<uint32_t>(F::unit+0x694,0);assert(sample(f).count==0);
  std::cout<<"PASS detach/death and stale one-way parasite pointers clear the icon\n";
 }
 {
  F f;f.mem.put<float>(F::unit+0x150,2.0f);for(int off:{0x158,0x160,0x580})f.mem.put<double>(F::unit+off,1.0);
  assert(sample(f).count==0);buffs(f);f.next();assert(sample(f).items[0].buffs==7);
  for(int off:{0x158,0x160,0x580})f.mem.put<double>(F::unit+off,std::numeric_limits<double>::quiet_NaN());assert(sample(f).count==0);
  for(double invalid:{0.0,-1.0,1.0,std::numeric_limits<double>::infinity(),1e100}){
   f.mem.put<double>(F::unit+0x160,invalid);f.next();assert(sample(f).count==0);
  }
  f.mem.put<double>(F::unit+0x160,1.5);f.next();assert(sample(f).items[0].buffs==Firepower);
  f.mem.put<double>(F::unit+0x160,1.0);f.mem.put<double>(F::unit+0x158,1.5);assert(sample(f).items[0].buffs==Armor);
  f.mem.put<double>(F::unit+0x158,1.0);f.mem.put<double>(F::unit+0x580,1.5);assert(sample(f).items[0].buffs==Speed);
  std::cout<<"PASS crate fields separate from elite rank; individual icons; no marker for NaN/infinity/default or invalid multipliers\n";
 }
 {
  F f;for(auto p:{F::unit,F::foe,F::friendUnit,F::infantry,F::aircraft,F::building})buffs(f,p);
  auto&m=sample(f);assert(m.count==5);for(int i=0;i<m.count;++i)assert(m.items[i].kind!=3);
  f.mem.put(F::unit+0x11C,F::foe);assert(sample(f).count==4);f.mem.put<uint32_t>(F::unit+0x11C,0);f.next();assert(sample(f).count==5);
  for(int guard=1;guard<=4;++guard){f.mem.guard=guard;assert(sample(f).count==0);f.mem.guard=0;assert(sample(f).count==5);}
  f.mem.put<int>(0xA8EDA0,1);assert(sample(f).count==0);f.mem.put<int>(0xA8EDA0,0);assert(sample(f).count==5);
  f.mem.fail=true;assert(sample(f).count==0);f.mem.fail=false;assert(sample(f).count==5);
  f.mem.put<uint32_t>(F::unit+0x10,99);assert(sample(f).items[0].id==99);
  f.sampler.reset();assert(f.sampler.unitStatuses().count==0);
  std::cout<<"PASS all owners, infantry/vehicle/aircraft fields; buildings/transported units excluded; menu/session/read failures and recycled IDs\n";
 }
 {
  F f;attach(f);buffs(f);sample(f);f.mem.scans=0;
  for(int i=0;i<1000;++i){if(i%8==0)f.next();f.mem.put<int>(F::unit+0x9C,42*256+(i%32)*8);auto&m=sample(f);assert(m.count==1&&m.items[0].parasite==attacker);}
  assert(f.mem.scans==125);assert(external::status::fresh(f.sampler.unitStatuses(),f.sampler.unitStatuses().match,f.now));
  assert(!external::status::fresh(f.sampler.unitStatuses(),f.sampler.unitStatuses().match,f.now+250));
  std::cout<<"PASS 1000 high-refresh samples with 125 frame scans; freshness expires at 250ms\n";
 }
 {
  using namespace assist::statusbadges;assert(SpiderSize==14&&BuffSize==9);
  auto icon=spider(0xFF14D28C);assert(icon.width==14&&icon.height==14);int colored=0;
  for(auto c:icon.pixels){assert(!c||c==0xFF07101B||c==0xFF14D28C);if(c==0xFF14D28C)++colored;}assert(colored>25);
  // Exhaust the positions within a tile; every corner of every buff remains
  // within its real 60x30 cell, even while the unit crosses a cell edge.
  for(int wx=0;wx<256;wx+=4)for(int wy=0;wy<256;wy+=4)for(unsigned b=1;b<8;++b)for(bool p:{false,true}){
   float x=(wx-wy)*30.f/256.f,y=(wx+wy)*15.f/256.f;
   auto l=layout(x,y,0,15,b,p);assert(l.count>=1&&l.count<=3);
   for(int i=1;i<l.count;++i)assert(l.icons[i].x-l.icons[i-1].x>8.9f);
   for(int i=0;i<l.count;++i)for(int dx:{-1,1})for(int dy:{-1,1})
    assert(std::abs(l.icons[i].x+dx*4.5f-l.center.x)/30.f+std::abs(l.icons[i].y+dy*4.5f-l.center.y)/15.f<1.f);
  }
  std::cout<<"PASS exact icon sizes/colors and all buff corners inside the native 60x30 diamond over 57344 layouts\n";
 }
}
