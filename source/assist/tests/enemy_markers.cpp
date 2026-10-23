#include "../../external_reader/source/tactical_sampler.h"
#include "../source/label_motion.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
struct EnemyMemory {
 std::vector<unsigned char> data=std::vector<unsigned char>(0xC00000);
 uint32_t scans=0,freshReads=0,mapReads=0,nameReads=0;bool fail=false;int guard=0;
 template<class T>void put(uint32_t p,T v){assert(uint64_t(p)+sizeof v<=data.size());std::memcpy(data.data()+p,&v,sizeof v);}
 void vector(uint32_t at,uint32_t storage,const std::vector<uint32_t>&items){put(at+4,storage);put<int>(at+8,int(items.size()));put<int>(at+16,int(items.size()));for(auto p:items){put(storage,p);storage+=4;}}
 bool read(uint32_t at,void*out,size_t n,bool latest=false){
  if(latest)++freshReads;if(at==0xA8EC7C)++scans;if(at==0x87F7E8+316)++mapReads;if(at>=0x790000&&at<0x791000)++nameReads;
  if(fail||at<0x10000||uint64_t(at)+n>data.size())return false;
  std::memcpy(out,data.data()+at,n);
  if(latest&&guard){if(guard==1&&at==0xA8ED84){int v;std::memcpy(&v,out,4);v+=3;std::memcpy(out,&v,4);}
   if(guard==2&&at==0xA83D4C){uint32_t v=0x6A0000;std::memcpy(out,&v,4);}
   if(guard==3&&at==0xA8EDA0){int v=1;std::memcpy(out,&v,4);}
   if(guard==4&&at==0xA8E9A0){*static_cast<uint8_t*>(out)=0;}
  }return true;
 }
 uint64_t hash()const{uint64_t h=1469598103934665603ull;for(auto b:data)h=(h^b)*1099511628211ull;return h;}
};
struct EnemyRead {EnemyMemory*m;bool operator()(uint32_t a,void*out,size_t n)const{return m->read(a,out,n);}bool fresh(uint32_t a,void*out,size_t n)const{return m->read(a,out,n,true);}};
struct EnemyFixture {
 static constexpr uint32_t local=0x680000,enemy=0x6A0000,ally=0x6C0000,neutral=0x6E0000,tactical=0x7A0000;
 static constexpr uint32_t type=0x650000,enemyType=0x652000,infType=0x654000,buildingType=0x656000,airType=0x658000;
 static constexpr uint32_t unit=0x100000,foe=0x102000,friendUnit=0x104000,civilian=0x106000,building=0x108000,infantry=0x10A000,aircraft=0x10C000;
 EnemyMemory mem;EnemyRead reader{&mem};external::TacticalSampler<EnemyRead> sampler{reader};uint32_t now=100;int frame=100;
 EnemyFixture(){
  mem.put<uint8_t>(0xA8E9A0,1);mem.put<uint8_t>(0xA8E378,1);mem.put(0xA83D4C,local);mem.put(0x887324,tactical);mem.put<int>(0xA8ED84,frame);
  mem.put<int>(0x8A00A4,1920);mem.put<int>(0x8A00A8,1080);mem.put<int>(0xB0CE30,1600);mem.put<int>(0xB0CE34,1000);
  mem.put<int>(tactical+0xB0,-320);mem.put<int>(tactical+0xB4,800);mem.put<double>(0xB0CD48,.0866025403784);
  mem.vector(0xA80228,0x820000,{local,enemy,ally,neutral});
  for(int i=0;i<4;++i){uint32_t h=local+uint32_t(i)*0x20000,country=0x780000+uint32_t(i)*0x1000;mem.put(h+0x30,i);mem.put(h+0x34,country);mem.put<uint8_t>(country+0x1A6,i==3);mem.put<uint8_t>(h+22265,220);mem.put<uint8_t>(h+22266,80);mem.put<uint8_t>(h+22267,70);}
  mem.put<uint32_t>(local+0x5788,1u|4u);
  for(uint32_t t:{type,enemyType,infType,buildingType,airType}){mem.put<int>(t+0x634,1);mem.put<uint32_t>(t+0x6CC,1);mem.put<int>(t+0x5E0,5);const char id[]="HTNK";std::memcpy(mem.data.data()+t+0x24,id,sizeof id);}
  const char16_t title[]=u"犀牛坦克";std::memcpy(mem.data.data()+0x790000,title,sizeof title);mem.put(enemyType+0x60,uint32_t(0x790000));
  object(unit,1,local,2,type);object(foe,2,enemy,2,enemyType);object(friendUnit,3,ally,2,enemyType);object(civilian,4,neutral,2,enemyType);
  object(building,5,enemy,3,buildingType);object(infantry,6,enemy,1,infType);object(aircraft,7,enemy,0,airType);mem.put<int>(aircraft+0xA4,1000);
  mem.put<uint8_t>(unit+0x83,1);mem.vector(0xA8ECB8,0x850000,{unit});mem.vector(0xA83CE0,0x860000,{type,enemyType});
  world({unit,foe,friendUnit,civilian,building,infantry,aircraft});
 }
 void object(uint32_t p,uint32_t id,uint32_t owner,int kind,uint32_t t){
  mem.put<uint32_t>(p,kind==3?0x7E3EBC:kind==1?0x7EB058:kind==0?0x7E22A4:0x7F5C70);mem.put(p+0x10,id);mem.put(p+0x21C,owner);
  mem.put(p+(kind==3?0x520:kind==1?0x6C0:0x6C4),t);mem.put<int>(p+0x6C,100);mem.put<uint8_t>(p+0x74,1);mem.put<uint8_t>(p+0x90,1);
  mem.put<int>(p+0x9C,42*256);mem.put<int>(p+0xA0,40*256);
 }
 void world(const std::vector<uint32_t>&objects){mem.vector(0xA8EC78,0x830000,objects);}
 void next(){mem.put<int>(0xA8ED84,++frame);}
 const assist::Snapshot&sample(bool show=true,uint32_t flags=assist::Capacity){now+=2;return sampler.sample(now,flags,show);}
 const external::enemies::Frame&marks()const{return sampler.enemyMarkers();}
 bool has(uint32_t id)const{for(int i=0;i<marks().count;++i)if(marks().items[i].id==id)return true;return false;}
};
int main(){using F=EnemyFixture;using namespace assist;
 {
  F f;auto baseline=f.sample(false);assert(f.marks().count==0&&f.mem.mapReads==0);
  auto checksum=f.mem.hash();auto s=f.sample();assert(f.mem.hash()==checksum);assert(s.labels==baseline.labels&&s.transports==baseline.transports&&s.capacity==baseline.capacity);
  assert(f.marks().count==4&&f.has(2)&&f.has(5)&&f.has(6)&&f.has(7)&&!f.has(1)&&!f.has(3)&&!f.has(4));
  assert(f.mem.mapReads==0&&f.mem.freshReads>0);assert(f.marks().items[0].color==0xFFDC5046&&f.marks().items[0].player==1&&f.marks().items[0].name[0]==u'犀');
  f.mem.scans=0;auto names=f.mem.nameReads;int before=f.marks().items[0].x;f.mem.put<int>(F::foe+0x9C,43*256);f.sample();assert(f.marks().items[0].x==before+30&&f.mem.scans==0&&f.mem.nameReads==names);
  f.sample(false);assert(f.marks().count==0);f.sample(true);assert(f.marks().count==4);
  std::cout<<"PASS enemy units/buildings/infantry/aircraft without reading fog/cells; allies/neutrals excluded; setting isolated; same-frame coordinates and cached names\n";
 }
 {
  F f;f.sample();uint32_t epoch=f.marks().match;
  for(uint32_t at:{0xA8EDA0u,0xA8ED8Cu,0xA8E378u}){
   if(at==0xA8E378)f.mem.put<uint8_t>(at,0);else f.mem.put<int>(at,1);
   f.sample();assert(f.marks().count==0&&f.marks().match==epoch);
   if(at==0xA8E378)f.mem.put<uint8_t>(at,1);else f.mem.put<int>(at,0);
   f.sample();assert(f.marks().count==4&&f.marks().match==epoch);
  }
  for(int guard=1;guard<=4;++guard){f.mem.guard=guard;f.sample();assert(f.marks().count==0);f.mem.guard=0;f.sample();assert(f.marks().count==4);}
  f.mem.fail=true;f.sample();assert(f.marks().count==0);f.mem.fail=false;f.sample();assert(f.marks().count==4);
  f.mem.put<uint8_t>(0xA8E9A0,0);f.sample();assert(f.marks().count==0);f.mem.put<uint8_t>(0xA8E9A0,1);f.next();f.sample();assert(f.marks().count==4);
  f.sampler.reset();assert(f.marks().count==0&&f.marks().tick==0);f.sample();assert(f.marks().count==4);
  std::cout<<"PASS menu/paused-frame restore, fresh frame/house/menu/end-game guards, read failure and reconnect clearing\n";
 }
 {
  F f;f.sample();f.mem.put<uint32_t>(F::local+0x5788,1u|2u|4u);f.sample();assert(f.marks().count==0);
  f.mem.put<uint32_t>(F::local+0x5788,1u|4u);f.sample();assert(f.marks().count==4); // Empty cached set must rediscover a newly hostile house.
  f.mem.put<uint8_t>(0x781000+0x1A6,1);f.sample();assert(f.marks().count==0);f.mem.put<uint8_t>(0x781000+0x1A6,0);f.sample();assert(f.marks().count==4);
  f.mem.put(F::foe+0x21C,F::local);f.sample();assert(!f.has(2));f.mem.put(F::foe+0x21C,F::enemy);f.next();f.sample();assert(f.has(2));
  f.mem.put<uint32_t>(F::foe+0x10,99);f.sample();assert(!f.has(2)&&f.has(99));
  for(int field:{0x6C,0x74,0x81,0x90,0x11C}){
   f.mem.put<int>(F::foe+field,field==0x81?1:field==0x11C?int(F::unit):0);f.sample();assert(!f.has(99));
   if(field==0x6C)f.mem.put<int>(F::foe+field,100);else if(field==0x11C)f.mem.put<uint32_t>(F::foe+field,0);else f.mem.put<uint8_t>(F::foe+field,field==0x81?0:1);
   f.next();f.sample();assert(f.has(99));
  }
  f.mem.put<uint32_t>(F::foe+0x6C4,F::infType);f.sample();assert(f.marks().items[0].type==F::infType&&f.marks().items[0].name[0]=='H');
  f.mem.put<int>(F::enemy+0x30,32);f.sample();assert(f.marks().count==0);f.mem.put<int>(F::enemy+0x30,1);f.sample();assert(f.marks().count==4);
  f.mem.put<uint32_t>(F::enemy+0x34,0);f.sample();assert(f.marks().count==0);
  std::cout<<"PASS alliance/neutral/ownership changes, recycled IDs/types, dead/off-map/limbo/transported units and invalid owner data\n";
 }
 {
  F f;f.sample();f.mem.scans=0;visual::Motion normal,enemy;Snapshot enemyInput{};visual::LabelView normalViews[MaxLabels],enemyViews[MaxLabels];
  for(int i=0;i<1000;++i){
   if(i%8==0)f.next();int wx=42*256+(i%32)*8;f.mem.put<int>(F::foe+0x9C,wx);f.mem.put<int>(F::unit+0x9C,wx);
   const auto&s=f.sample();const auto&m=f.marks();assert(m.tick==s.sampledTick&&m.frame==s.frame&&m.count==4);
   normal.update(s);enemyInput.match=m.match;enemyInput.sampledTick=m.tick;enemyInput.frame=m.frame;enemyInput.cameraX=s.cameraX;enemyInput.cameraY=s.cameraY;enemyInput.labels=m.count;
   for(int j=0;j<m.count;++j){const auto&mark=m.items[j];enemyInput.label[j]={mark.id,mark.x,mark.y,0,0,0,0,0,mark.color,0};}
   enemy.update(enemyInput);assert(normal.render(f.now,normalViews)==1&&enemy.render(f.now,enemyViews)==4);
   assert(normalViews[0].x==enemyViews[0].x&&normalViews[0].y==enemyViews[0].y);
  }
  assert(f.mem.scans==125);f.mem.scans=0;
  for(int i=0;i<1000;++i){f.mem.put<int>(F::tactical+0xB0,-300+i*3);f.sample();}
  assert(f.mem.scans>0&&f.mem.scans<=126);
  std::cout<<"PASS 1000 shared 2ms display samples: exact position/interpolation agreement with carrier labels; 125 frame scans; continuous camera movement at most 126 scans\n";
 }
 {
  F f;f.mem.put<int>(F::foe+0x9C,70*256);f.mem.put<int>(F::foe+0xA0,20*256);f.sample();assert(!f.has(2));f.mem.put<int>(0xB0CE30,1920);f.sample();assert(f.has(2));
  f.mem.put<int>(F::foe+0x9C,INT32_MIN);f.sample();assert(!f.has(2));
  assert(!external::enemies::onScreen(-1,10,1600,1000)&&!external::enemies::onScreen(1600,10,1600,1000)&&external::enemies::onScreen(1599,999,1600,1000));
  auto m=f.marks();assert(external::enemies::fresh(m,m.match,m.tick+249)&&!external::enemies::fresh(m,m.match,m.tick+250)&&!external::enemies::fresh(m,m.match+1,m.tick));
  m.tick=0xfffffff0u;assert(external::enemies::fresh(m,m.match,7));
  std::vector<uint32_t> objects{F::unit};for(int i=0;i<external::enemies::Maximum+1;++i){uint32_t p=0x100000+uint32_t(i+16)*0x800;f.object(p,100+i,F::enemy,2,F::enemyType);objects.push_back(p);}f.world(objects);f.next();f.sample();assert(f.marks().count==external::enemies::Maximum&&f.marks().omitted==1);
  f.mem.put<int>(0xA8EC78+16,20000);f.next();f.sample();assert(f.marks().count==0);
  std::cout<<"PASS tactical viewport/sidebar bounds, invalid coordinates, freshness/epoch/wrap, 2048-marker limit and corrupt object-array bounds\n";
 }
}

