#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/watch_slots.h"
#include "watch_tracking_before.inc"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace external;
static std::vector<uint8_t> memory(0xC00000);static uint32_t failAt=0;static unsigned checks=0;static uint64_t reads=0,inventoryReads=0;
template<class T>static void put(uint32_t p,const T&v){assert(uint64_t(p)+sizeof v<=memory.size());std::memcpy(memory.data()+p,&v,sizeof v);}
static bool read(uint32_t p,void*out,size_t n){++reads;if(p==0x900000)++inventoryReads;if(p<0x10000||uint64_t(p)+n>memory.size()||(failAt>=p&&failAt<uint64_t(p)+n))return false;std::memcpy(out,memory.data()+p,n);return true;}
static void checkAt(bool okay,int line){++checks;if(!okay){std::fprintf(stderr,"FAIL watch tracking check=%u line=%d\n",checks,line);std::fflush(nullptr);std::exit(1);}}
#define check(okay) checkAt(bool(okay),__LINE__)
static constexpr uint32_t House=0x400000,UnitType=0x500000,BuildingType=0x510000,OtherType=0x520000;
static void reset(){std::fill(memory.begin(),memory.end(),0);failAt=0;put(0xA83D4C,House);put(0xA8ED84,100);put(0xA8B238,3);put(0xA8B320,uint8_t(1));put(UnitType,0x7F6218u);put(UnitType+0x404,BuildingType);put(BuildingType,0x7E4570u);put(BuildingType+0x408,UnitType);put(BuildingType+0xEF0,0);put(0x8192B8,3);put(0x819310,3);put(0x89F6A4,std::array<int16_t,2>{{-1,-1}});put(0x89F6F0,std::array<int,2>{{256,256}});}
static watch::Target actor(uint32_t p,uint32_t id,int kind=2,int x=20*256+128,int y=30*256+128){
 std::fill(memory.begin()+p,memory.begin()+p+0x1000,0);put(p,kind==3?0x7E3EBCu:kind==1?0x7EB058u:kind==0?0x7E22A4u:0x7F5C70u);put(p+0x10,id);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9C,x);put(p+0xA0,y);put(p+0x21C,House);put(p+(kind==3?0x520:kind==1?0x6C0:0x6C4),kind==3?BuildingType:UnitType);return {p,id};
}
static void vec(uint32_t at,const std::vector<uint32_t>&items,uint32_t data=0x900000){put(at+4,data);put(at+8,int(items.size()));put(at+16,int(items.size()));for(size_t i=0;i<items.size();++i)put(data+uint32_t(i)*4,items[i]);}
static void die(watch::Target t){put(t.pointer+0x6C,0);put(t.pointer+0x90,uint8_t(0));put(t.pointer+0x74,uint8_t(0));}
static watch::Track track(watch::Target t){watch::Track out;out.target=out.spatial=t;out.members={t};GameView<decltype(&read)>g(&read);out.located=watch::locate(g,t,out.x,out.y,out.z);out.spatialKind=g.kind(t.pointer);return out;}
static void deploy(watch::Target t){put(t.pointer+0xAC,16);put(t.pointer+0x68C,uint8_t(1));}
static void conversion(){
 reset();auto a=actor(0x20000,1);deploy(a);vec(0xA8EC78,{a.pointer});auto t=track(a);watch::follow(read,t);check(t.transition.armed&&t.transition.inventoryValid&&t.transition.kind==3&&t.transition.x==19*256+128&&t.transition.y==29*256+128);
 const auto inventoryCount=inventoryReads;for(int n=0;n<100;++n)watch::follow(read,t);check(inventoryReads==inventoryCount);
 before_watch::Track old;old.target={a.pointer,a.id};old.members={old.target};old.located=true;old.x=t.x;old.y=t.y;old.z=t.z;old.spatial=old.target;
 auto b=actor(0x22000,2,3,t.transition.x,t.transition.y);die(a);vec(0xA8EC78,{b.pointer});put(0xA8ED84,101);watch::follow(read,t);before_watch::follow(read,old);
 check(!old);check(t.target==b&&!t.fixed&&t.members.size()==1&&t.members[0]==b);check(t.spatial==b&&t.spatialKind==3);
 // Reverse the exact native foundation offset; never use a nearest MCV.
 put(b.pointer+0xAC,19);put(b.pointer+0xBC,2);put(b.pointer+0x218,0x700000u);put(BuildingType+0x16B9,uint8_t(1));watch::follow(read,t);check(t.transition.armed&&t.transition.kind==2&&t.transition.x==20*256+128);
 auto c=actor(0x24000,3,2,t.transition.x,t.transition.y);die(b);vec(0xA8EC78,{c.pointer});put(0xA8ED84,102);watch::follow(read,t);check(t.target==c&&t.members[0]==c&&!t.fixed);
 put(c.pointer+0x9C,24*256+17);watch::follow(read,t);check(t.target==c&&t.x==24*256+17&&!t.transition.armed);
 // A real MCV death now retains the location by the explicitly chosen narrow
 // policy. Ordinary non-deployer death still inherits its original cohort.
 die(c);watch::follow(read,t);check(t.fixed&&!t.target);
 auto ordinary=actor(0x28000,5);put(ordinary.pointer+0x6C4,OtherType);auto member=actor(0x26000,4,2,20*256+178,30*256+128);t=track(ordinary);t.members.push_back(member);die(ordinary);watch::follow(read,t);check(t.target==member);
 std::puts("PASS actual old baseline loses the MCV pin; new exact native-cell deploy/undeploy birth follows new UIDs, then movement and original cohort succession.");
}
static void rejectedBirths(){
 // Independent fixed enum witnesses: Hunt(15) and AreaGuard(11) cannot arm
 // either direction. Do not feed production constants back into this oracle.
 reset();auto ordinary=actor(0x20000,1);vec(0xA8EC78,{ordinary.pointer});auto guard=track(ordinary);put(ordinary.pointer+0xAC,15);watch::follow(read,guard);check(!guard.transition.armed);
 reset();ordinary=actor(0x20000,1,3);vec(0xA8EC78,{ordinary.pointer});guard=track(ordinary);put(ordinary.pointer+0xAC,11);put(ordinary.pointer+0xBC,2);watch::follow(read,guard);check(!guard.transition.armed);
 for(int mode=0;mode<16;++mode){reset();auto a=actor(0x20000,10,mode==15?3:2);if(mode==15){put(a.pointer+0xAC,19);put(a.pointer+0xBC,2);}else deploy(a);auto t=track(a);std::vector<uint32_t> objects{a.pointer};
  watch::Target pre;if(mode==0||mode>=14){pre=actor(0x22000,11,mode==15?2:3,mode==0?19*256+128:18*256+128,29*256+128);objects.push_back(pre.pointer);}vec(0xA8EC78,objects);watch::follow(read,t);check(t.transition.armed);
  auto b=(mode==0||mode>=14)?pre:actor(0x24000,12,3,t.transition.x,t.transition.y);if(mode>=14){put(b.pointer+0x9C,t.transition.x);put(b.pointer+0xA0,t.transition.y);}objects={b.pointer};
  if(mode==1){auto other=actor(0x26000,13,3,t.transition.x,t.transition.y);objects.push_back(other.pointer);}
  if(mode==2)put(b.pointer+0x520,OtherType);if(mode==3)put(b.pointer+0x21C,House+0x1000);if(mode==4)put(b.pointer+0x9C,t.transition.x+1);if(mode==5)put(b.pointer+0xA4,1);
  die(a);if(mode==6){b=actor(a.pointer,99,3,t.transition.x,t.transition.y);objects={b.pointer};}
  vec(0xA8EC78,objects);put(0xA8ED84,mode==7?103:mode==8?99:101);
  if(mode==9)failAt=b.pointer+0x520;if(mode==10)failAt=0x900000;if(mode==11)put(0xA83D4C,House+0x1000);if(mode==12)put(b.pointer+0x10,0u);if(mode==13)put(b.pointer+0x220,2);
  const int x=t.x,y=t.y;watch::follow(read,t);check(t&&t.fixed&&!t.target&&t.located&&t.x==x&&t.y==y);failAt=0;
  reads=0;watch::follow(read,t);check(reads==0&&t.fixed);int cx=0,cy=0;watch::Anchor at{{},t.x,t.y,t.z,true,t.spatialKind};check(watch::camera(read,{},800,600,cx,cy,at));
 }
 // No deployment observation means normal death, including cancelled orders
 // and enemy identities. A transient unreadable life sample never clears.
 for(int mode=0;mode<3;++mode){reset();auto a=actor(0x20000,1);vec(0xA8EC78,{a.pointer});auto t=track(a);if(mode==0){deploy(a);watch::follow(read,t);check(t.transition.armed);put(a.pointer+0xAC,0);put(a.pointer+0x68C,uint8_t(0));}if(mode==1){deploy(a);put(a.pointer+0x21C,House+0x1000);}watch::follow(read,t);check(!t.transition.armed);failAt=a.pointer+0x90;watch::follow(read,t);check(t.target==a);failAt=0;die(a);watch::follow(read,t);check(!t);}
 std::puts("PASS 16 rejected births: pre-existing or moved-in (both directions)/ambiguous/wrong type-owner-XYZ/reused source pointer/stale-rewound frame/read failures/observer change/zero UID/cloak => fixed live location; cancel, enemy, death and unknown semantics preserved.");
 reset();auto capped=actor(0x20000,1);deploy(capped);vec(0xA8EC78,{});put(0xA8EC80,8193);put(0xA8EC88,8193);auto limited=track(capped);watch::follow(read,limited);check(limited.transition.armed&&!limited.transition.inventoryValid);die(capped);watch::follow(read,limited);check(limited.fixed&&!limited.target);
 // An aligned MCV can finish entirely between polls. Its configured owned
 // ConstructionYard grants only fixed-location fallback, never a guessed UID.
 reset();auto instant=actor(0x20000,1);put(BuildingType+0x16B9,uint8_t(1));vec(0xA8EC78,{instant.pointer});auto quick=track(instant);const auto readsBefore=inventoryReads;watch::follow(read,quick);check(!quick.transition.armed&&quick.transition.fallbackEligible&&inventoryReads==readsBefore);auto yard=actor(0x22000,2,3,19*256+128,29*256+128);die(instant);vec(0xA8EC78,{yard.pointer});watch::follow(read,quick);check(quick.fixed&&!quick.target&&quick.x==20*256+128);
}
static void load(watch::Target passenger,watch::Target carrier){put(passenger.pointer+0x11C,carrier.pointer);put(passenger.pointer+0x74,uint8_t(0));put(passenger.pointer+0x81,uint8_t(1));put(carrier.pointer+0x114,1);put(carrier.pointer+0x118,passenger.pointer);put(passenger.pointer+0x30,0u);}
static void containers(){
 reset();auto inf=actor(0x20000,1,1);auto apc=actor(0x22000,2);auto ship=actor(0x24000,3,0,40*256,50*256);load(inf,apc);load(apc,ship);auto t=track(inf);watch::follow(read,t);check(t.target==inf&&t.spatial==ship&&t.x==40*256);
 put(ship.pointer+0x9C,41*256+23);watch::follow(read,t);check(t.spatial==ship&&t.x==41*256+23);
 before_watch::Anchor old=before_watch::anchor(read,{inf.pointer,inf.id});check(!old.located);
 put(apc.pointer+0x11C,0u);put(apc.pointer+0x74,uint8_t(1));put(apc.pointer+0x81,uint8_t(0));watch::follow(read,t);check(t.target==inf&&t.spatial==apc);
 put(inf.pointer+0x11C,0u);put(inf.pointer+0x74,uint8_t(1));put(inf.pointer+0x81,uint8_t(0));watch::follow(read,t);check(t.target==inf&&t.spatial==inf);
 // A tank may be carried by the same validated passenger chain.
 auto tank=actor(0x26000,4);load(tank,apc);load(apc,ship);auto tt=track(tank);watch::follow(read,tt);check(tt.target==tank&&tt.spatial==ship);
 // Reciprocal bunker links plus the native Bunker flag are required.
 reset();tank=actor(0x20000,1);auto bunker=actor(0x22000,2,3,60*256,70*256);put(BuildingType+0x16AB,uint8_t(1));put(tank.pointer+0x2E4,bunker.pointer);put(bunker.pointer+0x2E4,tank.pointer);put(tank.pointer+0x74,uint8_t(0));tt=track(tank);watch::follow(read,tt);check(tt.target==tank&&tt.spatial==bunker&&tt.spatialKind==3&&tt.x==60*256);
 put(bunker.pointer+0x2E4,0u);put(bunker.pointer+0x9C,61*256);watch::follow(read,tt);check(tt.target==tank&&!tt.spatial&&tt.x==60*256);put(bunker.pointer+0x2E4,tank.pointer);watch::follow(read,tt);check(tt.spatial==bunker&&tt.x==61*256);
 // A garrison host after the first 64 entries stays cached even when that
 // host itself is transported; the outer host still moves each presentation.
 reset();inf=actor(0x20000,1,1);put(inf.pointer+0x81,uint8_t(1));put(inf.pointer+0x74,uint8_t(0));ship=actor(0x24000,3,0,80*256,90*256);std::vector<uint32_t> buildings;
 for(int i=0;i<100;++i){auto b=actor(0x100000+uint32_t(i)*0x1000,uint32_t(100+i),3);buildings.push_back(b.pointer);}bunker={buildings[70],170};put(bunker.pointer+0x688,std::array<uint32_t,4>{{0x910000,1,0,1}});put(0x910000,inf.pointer);vec(0xA8EB40,buildings,0x920000);load(bunker,ship);t=track(inf);watch::follow(read,t);check(!t.spatial);put(0xA8ED84,101);watch::follow(read,t);check(t.spatial==ship&&t.garrison==bunker);put(ship.pointer+0x9C,81*256);watch::follow(read,t);check(t.spatial==ship&&t.x==81*256&&t.garrison==bunker);
 // Cycles/depth and malformed forward links retain the verified point.
 reset();auto a=actor(0x20000,1);auto b=actor(0x22000,2);load(a,b);load(b,a);watch::Anchor prior{{},1234,2345,0,true,2};auto center=watch::anchor(read,a,prior);check(center.located&&!center.target&&center.x==1234);
 reset();std::vector<watch::Target> chain;for(int i=0;i<9;++i)chain.push_back(actor(0x20000+uint32_t(i)*0x2000,uint32_t(i+1)));for(int i=0;i<8;++i)load(chain[i],chain[i+1]);center=watch::anchor(read,chain[0],prior);check(center.located&&!center.target&&center.x==1234);
 reset();a=actor(0x20000,1);b=actor(0x22000,2);load(a,b);put(a.pointer+0x30,a.pointer);center=watch::anchor(read,a,prior);check(center.located&&!center.target);put(a.pointer+0x30,0u);failAt=b.pointer+0x90;center=watch::anchor(read,a,prior);check(center.located&&!center.target);failAt=0;put(a.pointer+0x10,99u);check(!watch::anchor(read,a,prior).located);
 // Foreign spatial hosts must be currently visible, including navigation.
 reset();a=actor(0x20000,1,1);b=actor(0x22000,2);load(a,b);put(b.pointer+0x21C,House+0x1000);center=watch::anchor(read,a,prior);check(center.located&&!center.target&&center.x==prior.x);
 put(0xA8B230,0x600000u);put(0x87F7E8+0x13C,0xA00000u);put(0x87F7E8+0x140,512*512);const int index=30*512+20;put(0xA00000+index*4,0xB00000u);put(0xB00024,std::array<int16_t,2>{{20,30}});put(0xB0012C,uint8_t(0x10));center=watch::anchor(read,a,prior);check(center.target==b);
 put(0x600001,uint8_t(0x10));put(0xB00140,uint8_t(3));center=watch::anchor(read,a,prior);check(!center.target&&center.x==prior.x);put(0xB00140,uint8_t(1));center=watch::anchor(read,a,prior);check(center.target==b);
 std::puts("PASS full infantry/vehicle passenger chains, unload, reciprocal tank bunker, cached >64th garrison+outer carrier, continuous host motion; cycles/depth/malformed membership/read failure/UID reuse rejected.");
}
int main(){conversion();rejectedBirths();containers();std::printf("PASS watch tracking: %u active checks; controlled source-bound fixture, no live game.\n",checks);}
