#include "../source/sampler.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

struct Memory {
 std::vector<uint8_t> data=std::vector<uint8_t>(0xB00000);
 uint32_t fail=0;int bookends=0;std::function<void()> duringSample;
 template<class T>void put(uint32_t p,T v){memcpy(data.data()+p,&v,sizeof v);}
 void text(uint32_t p,const char16_t* name){memset(data.data()+p,0,40);memcpy(data.data()+p,name,std::min<size_t>(40,std::char_traits<char16_t>::length(name)*2));}
 bool read(uint32_t p,void*out,size_t n){if((fail&&p<=fail&&uint64_t(p)+n>fail)||uint64_t(p)+n>data.size())return false;memcpy(out,data.data()+p,n);return true;}
 bool fresh(uint32_t p,void*out,size_t n){if(p==0xA8E9A0&&++bookends==2&&duringSample)duringSample();return read(p,out,n);}
 void vector(uint32_t at,uint32_t array,std::initializer_list<uint32_t> items){put(at+4,array);put(at+8,int(items.size()));put(at+16,int(items.size()));for(auto x:items){put(array,x);array+=4;}}
};
struct Read {Memory* m;bool operator()(uint32_t p,void*out,size_t n){return m->read(p,out,n);}bool fresh(uint32_t p,void*out,size_t n){return m->fresh(p,out,n);}};
struct Resolve {uint32_t operator()(uint32_t){return 0;}};
constexpr uint32_t Self=0x100000,Enemy=0x120000,Country=0x200000,Infantry=0x210000;
struct Fixture {
 Memory m;std::unique_ptr<um::Sampler<Read,Resolve>> sampler;
 Fixture(um::IconCache*cache=nullptr,um::CameoResolve cameo=nullptr){
  m.put<uint8_t>(0xA8E9A0,1);m.put(0xA83D4C,Self);m.put(0xA8ED84,100);
  m.vector(0xA80228,0x300000,{Self,Enemy});m.vector(0xA8E348,0x300100,{Infantry});
  m.put(Infantry+1588,1);m.put<uint32_t>(Infantry+1740,1);memcpy(m.data.data()+Infantry+1750,"E1ICON",7);memcpy(m.data.data()+Infantry+36,"E1",3);
  for(int i=0;i<2;++i){uint32_t h=i?Enemy:Self;m.put(h+0x10,uint32_t(100+i));m.put(h+0x30,i);m.put(h+0x34,Country);m.put<uint8_t>(h+493,1);
   m.put(h+um::OwnedOffsets[0]+4,uint32_t(0x310000+i*4));m.put(h+um::OwnedOffsets[0]+8,1);m.put(0x310000+i*4,5+i);
  }
  m.text(Self+90154,u"Me");m.text(Enemy+90154,u"Previous opponent");
  sampler=std::make_unique<um::Sampler<Read,Resolve>>(Read{&m},Resolve{},cache,nullptr,cameo);
 }
 const um::Snapshot& sample(){m.bookends=0;return sampler->sample(500,true);}
};
static std::vector<uint32_t> resolvedMatches;
static void resolveCameo(um::Type&t,uint32_t match){resolvedMatches.push_back(match);auto name="LIVE"+std::to_string(match)+".SHP";strcpy(t.cameo,name.c_str());}
static Memory* artFixture=nullptr;
static bool iconAvailable=false,sidebarUnavailable=false;
static bool fixtureArt(uint32_t p,void*out,size_t n){
 const uint32_t first=0x87F7E8+0x1544;
 if(sidebarUnavailable&&p>=first&&p<first+4*3988)return false;
 return artFixture&&artFixture->read(p,out,n);
}
static void fixtureResolveCameo(um::Type&t,uint32_t){
 assert(std::string(t.cameo)=="E1ICON");
 strcpy(t.cameo,iconAvailable?"RIGHT.SHP":"WRONG.SHP");
}
static bool fixtureAssets(const char*name,std::vector<uint8_t>&out){
 if(!strcmp(name,"cameo.pal")){out.assign(768,0);out[21]=63;return true;}
 if(!iconAvailable||strcmp(name,"RIGHT.SHP"))return false;
 out.assign(33,0);out[2]=out[4]=out[12]=out[14]=1;out[28]=32;out[32]=7;return true;
}
int main(){
 int cases=0;
 {Fixture f;artFixture=&f.m;constexpr uint32_t second=0x220000;
  memcpy(f.m.data.data()+second,f.m.data.data()+Infantry,4096);
  memcpy(f.m.data.data()+second+36,"E2",3);
  f.m.vector(0xA8E348,0x300100,{Infantry,second});
  f.m.put(Self+um::OwnedOffsets[0]+4,uint32_t(0x320000));f.m.put(Self+um::OwnedOffsets[0]+8,2);
  f.m.put(0x320000,5);f.m.put(0x320004,7);
  const uint32_t strip=0x87F7E8+0x1544+2*3988;
  f.m.put(strip+84,2);f.m.put(strip+88,1);f.m.put(strip+92,16);
  f.m.put(strip+140,0);f.m.put(strip+144,16);
  f.sampler=std::make_unique<um::Sampler<Read,Resolve>>(Read{&f.m},Resolve{},nullptr,nullptr,nullptr,fixtureArt);
  auto check=[&](uint32_t tick,bool reversed){
   f.m.bookends=0;const auto&s=f.sampler->sample(tick,true);
   assert(s.status==1&&s.typeCount==2&&s.playerCount==2);
   assert(s.types[0].index==(reversed?1:0)&&s.types[1].index==(reversed?0:1));
   assert(s.players[0].cells[0].owned==(reversed?7:5)&&s.players[0].cells[1].owned==(reversed?5:7));
  };
  check(100,true);
  f.m.put(strip+88,0);f.m.put(strip+140,1);sidebarUnavailable=true;check(700,true);
  sidebarUnavailable=false;check(1300,false);artFixture=nullptr;++cases;
 }
 {Fixture f;artFixture=&f.m;iconAvailable=false;
  f.sampler=std::make_unique<um::Sampler<Read,Resolve>>(Read{&f.m},Resolve{},nullptr,fixtureAssets,fixtureResolveCameo,fixtureArt);
  f.m.bookends=0;const auto&first=f.sampler->sample(100,true);
  assert(first.status==1&&first.typeCount==1&&first.types[0].iconReady==0&&first.missingIcons==1);
  iconAvailable=true;f.m.bookends=0;const auto&next=f.sampler->sample(700,true);
  assert(next.status==1&&next.types[0].iconReady==1&&next.types[0].icon[0]==7&&next.missingIcons==0);
  assert(std::string(next.types[0].cameo)=="RIGHT.SHP"&&next.players[0].cells[0].owned==5);
  artFixture=nullptr;iconAvailable=false;++cases;
 }
 {Fixture f(nullptr,resolveCameo);f.m.data[Infantry+1750]=0;auto&first=f.sample();assert(first.typeCount==1&&resolvedMatches.back()==first.match);auto old=first.match;
  f.m.put(0xA8ED84,1);auto&next=f.sample();assert(next.typeCount==1&&next.match!=old&&resolvedMatches.back()==next.match);assert(std::string(next.types[0].cameo)=="LIVE"+std::to_string(next.match)+".SHP");++cases;
 }
 {Fixture f;auto& a=f.sample();assert(a.status==1&&a.playerCount==2&&a.typeCount==1&&a.players[1].cells[0].owned==6);uint32_t epoch=a.match;
  f.m.put<uint8_t>(Self+501,1);f.m.put(0xA8ED84,101);auto& b=f.sample();
  assert(b.status==1&&b.match==epoch&&b.playerCount==1&&b.players[0].ptr==Enemy);++cases; // death does not end spectatorship
  // Identical allocation and IDs, missed low frames; local defeat->alive must reset.
  f.m.put<uint8_t>(Self+501,0);f.m.put(0xA8ED84,500);auto& c=f.sample();assert(c.status==1&&c.match!=epoch&&c.playerCount==2);++cases;
 }
 {Fixture f;uint32_t epoch=f.sample().match;f.m.text(Enemy+90154,u"Next opponent");f.m.put(0xA8ED84,200);f.m.put(0x310004,1);
  auto& a=f.sample();assert(a.status==1&&a.match!=epoch&&a.playerCount==2&&a.players[1].name[0]=='N'&&a.players[1].cells[0].owned==1);++cases;
 }
 {Fixture f;uint32_t epoch=f.sample().match;f.m.put<uint32_t>(Enemy+0x10,999);auto& a=f.sample();assert(a.status==1&&a.match!=epoch);++cases;}
 {Fixture f;uint32_t epoch=f.sample().match;f.m.put(0xA8ED84,1);assert(f.sample().match!=epoch);++cases;}
 {Fixture f;f.sample();f.m.put<uint8_t>(0xA8E9A0,0);auto& a=f.sample();assert(a.status==0&&a.playerCount==0&&a.typeCount==0&&a.players[0].name[0]==0);++cases;}
 {Fixture f;f.sample();f.m.duringSample=[&]{f.m.text(Enemy+90154,u"Another match");};auto& a=f.sample();
  assert(a.status==0&&a.playerCount==0&&a.typeCount==0&&f.sampler->probe().stage==external::layout::Stage::UnstableSnapshot);
  f.m.duringSample={};auto& b=f.sample();assert(b.status==1&&b.players[1].name[0]=='A');++cases;
 }
 {Fixture f;f.sample();f.m.duringSample=[&]{f.m.put<uint8_t>(0xA8E9A0,0);};assert(f.sample().status==0);++cases;}
 {Fixture f;f.sample();f.m.fail=Enemy+90154;auto& a=f.sample();assert(a.status==0&&a.playerCount==0);++cases;}
 {Fixture f;f.sample();f.m.fail=Enemy+501;auto& a=f.sample();assert(a.status==1&&a.playerCount==1&&a.players[0].ptr==Self&&a.readErrors>0);++cases;}
 {Fixture f;f.sample();f.m.vector(0xA80228,0x300000,{Enemy});auto& a=f.sample();assert(a.status==0&&a.playerCount==0);++cases;}
 {Fixture f;uint32_t epoch=f.sample().match;f.sampler->reset();auto& a=f.sample();assert(a.status==1&&a.match!=epoch&&a.playerCount==2);++cases;}
 {Fixture f;f.m.vector(0xA8E348,0x300100,{});assert(f.sample().typeCount==0);f.m.vector(0xA8E348,0x300100,{Infantry});assert(f.sample().typeCount==1);++cases;}
 // A cached reader must not satisfy the bookend's uncached checks.
 {struct Reader {bool cached=false,freshCalled=false;bool operator()(uint32_t,void*,size_t){cached=true;return false;}bool fresh(uint32_t,void*,size_t){freshCalled=true;return false;}} reader;
  um::roundstate::Stamp stamp;external::layout::Result error;assert(!um::roundstate::capture(reader,stamp,error)&&reader.freshCalled&&!reader.cached);++cases;
 }

 {namespace fs=std::filesystem;auto dir=fs::temp_directory_path()/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(dir);auto exe=dir/"game.exe";{std::ofstream f(exe);f<<"fixture";}
  auto scope=um::iconcache::scope(exe);um::iconcache::Record icon{};icon.category=0;strcpy(icon.id,"E1");strcpy(icon.cameo,"E1ICON");icon.icon[20]=4;icon.palette[4]=0xff123456;assert(um::iconcache::write(dir/"cache"/(std::to_string(scope)+".bin"),scope,{icon}));
  {um::IconCache cache;cache.open(exe,dir/"cache");for(int i=0;i<200&&!cache.loaded;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
   Fixture f(&cache);auto&a=f.sample();auto old=a.match;assert(a.types[0].iconReady==2&&a.types[0].icon[20]==4&&a.palette[4]==0xff123456);
   f.m.put(0xA8ED84,1);f.m.text(Enemy+90154,u"New player");const auto&b=f.sample();assert(b.match!=old&&b.types[0].iconReady==2&&b.types[0].icon[20]==4&&b.paletteReady&&b.players[1].name[0]=='N');++cases;
  }fs::remove_all(dir);
 }
 {Fixture f;constexpr uint32_t second=0x220000;auto old=f.sample().match;
  memcpy(f.m.data.data()+second+36,"E2",3);memcpy(f.m.data.data()+second+1750,"E2ICON",7);f.m.put(second+1588,1);f.m.put<uint32_t>(second+1740,1);
  f.m.vector(0xA8E348,0x300100,{Infantry,second});f.m.put(0xA8ED84,101);
  auto&a=f.sampler->sample(1100,true);assert(a.typeCount==2&&a.match==old);++cases;
  f.m.put<uint32_t>(second+1740,0);assert(f.sampler->sample(1700,true).typeCount==1);++cases;
  f.m.put<uint32_t>(second+1740,1);assert(f.sampler->sample(2300,true).typeCount==2);++cases;
 }
 // Cash belongs to the same sampled player and round, with explicit unknown on read failure.
 {Fixture f;f.m.put(Self+0x30C,12345);f.m.put(Enemy+0x30C,67890);auto& a=f.sample();
  assert(a.players[0].creditsKnown&&a.players[0].credits==12345&&a.players[1].creditsKnown&&a.players[1].credits==67890);++cases;
  f.m.put(Self+0x30C,0);f.m.put(Enemy+0x30C,2147483647);auto& b=f.sample();
  assert(b.players[0].creditsKnown&&b.players[0].credits==0&&b.players[1].credits==2147483647);++cases;
  f.m.fail=Enemy+0x30C;auto& c=f.sample();assert(c.status==1&&c.playerCount==2&&c.players[0].creditsKnown&&!c.players[1].creditsKnown&&c.readErrors>0);++cases;
  f.m.fail=0;f.m.text(Enemy+90154,u"Fresh opponent");f.m.put(Enemy+0x30C,500);f.m.put(0xA8ED84,1);auto& d=f.sample();
  assert(d.players[1].name[0]=='F'&&d.players[1].creditsKnown&&d.players[1].credits==500);++cases;
 }
 {Fixture f;auto epoch=f.sample().match;
  for(uint32_t address:{uint32_t(0xA8E9A0),uint32_t(Enemy+90154)}){f.m.fail=address;assert(f.sample().status==0);f.m.fail=0;f.m.put(0xA8ED84,200);assert(f.sample().match==epoch);++cases;}
  f.m.put<uint8_t>(0xA8E9A0,0);assert(f.sample().status==0);f.m.put<uint8_t>(0xA8E9A0,1);assert(f.sample().match!=epoch);++cases;
 }
 std::cout<<cases<<" round-transition sampler regressions passed\n";return 0;
}
