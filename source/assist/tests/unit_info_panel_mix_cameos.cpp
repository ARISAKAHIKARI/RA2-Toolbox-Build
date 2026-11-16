// The production unit panel icon cache, native SHP decoder and MIX asset
// callback are exercised with synthetic memory/files. No actual PE or MOD
// contents are checked into the repository.
#include "../source/unit_info_panel_cameos.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <cstdio>

struct Fixture {
 std::map<uint32_t,std::vector<uint8_t>> regions;
 std::map<std::string,std::vector<uint8_t>> files;
 unsigned calls=0,loads=0,resolves=0;
 template<class T>void put(uint32_t at,const T&v){
  const auto*p=reinterpret_cast<const uint8_t*>(&v);
  regions[at]=std::vector<uint8_t>(p,p+sizeof v);
 }
 void blob(uint32_t at,std::vector<uint8_t>v){regions[at]=std::move(v);}
 bool read(uint32_t at,void*out,size_t n){
  ++calls;
  for(const auto&record:regions){
   if(at>=record.first&&uint64_t(at)+n<=uint64_t(record.first)+record.second.size()){
    std::memcpy(out,record.second.data()+size_t(at-record.first),n);return true;
   }
  }
  return false;
 }
 bool load(const char*name,std::vector<uint8_t>&out){
  ++loads;auto it=files.find(name);
  if(it==files.end())return false;
  out=it->second;return true;
 }
 void resolve(um::Type&t){
  ++resolves;
  std::memcpy(t.cameo,"MODUNIT.SHP",12);
 }
 void setup(uint32_t type,uint8_t nativeIndex=7){
  auto name=std::vector<uint8_t>(23,0);std::memcpy(name.data(),"TESTUNIT",8);
  blob(type+0x24,std::move(name));
  const uint32_t nativePtr=0x350000;
  put(type+1776,nativePtr);blob(nativePtr,shape(nativeIndex));
  const uint32_t converter=0x360000,paletteAt=0x361000;
  put(uint32_t(0x87F6B0),converter);put(converter+4,uint32_t(2));
  put(converter+0x174,paletteAt);
  uint16_t pal[256]{};pal[7]=0x07E0;pal[9]=0xF800;
  regions[paletteAt]=std::vector<uint8_t>(reinterpret_cast<uint8_t*>(pal),
                               reinterpret_cast<uint8_t*>(pal)+sizeof pal);
 }
 static std::vector<uint8_t> shape(uint8_t pixel){
  std::vector<uint8_t> s(32+um::IW*um::IH,pixel);
  std::fill(s.begin(),s.begin()+32,0);
  auto word=[&](int at,uint16_t value){s[at]=uint8_t(value);s[at+1]=uint8_t(value>>8);};
  word(2,um::IW);word(4,um::IH);word(12,um::IW);word(14,um::IH);
  s[28]=32;return s;
 }
 static std::vector<uint8_t> colors(){
  std::vector<uint8_t> pal(768,0);
  pal[9*3]=63; // file pixel 9: red 252, not native converter green
  pal[7*3+2]=63; // native pixel 7 when file fallback: blue 252
  return pal;
 }
};
static Fixture* current=nullptr;
static bool load(const char*name,std::vector<uint8_t>&out){
 assert(current);return current->load(name,out);
}
static external::unitpanel::Model model(uint32_t match,uint32_t type){
 external::unitpanel::Model m;m.reset(match,100);
 m.selectedType(type,2);m.single.type=type;m.single.kind=2;
 return m;
}
int main(){
 constexpr uint32_t type=0x250000,pid=771;
 auto run=[](unitpanelcameo::Cache&cache,Fixture&data,
             const external::unitpanel::Model&m,uint32_t now){
  current=&data;
  cache.refresh(m,[&](uint32_t at,void*out,size_t n){return data.read(at,out,n);},
   [&](um::Type&t){data.resolve(t);},load,pid,m.match,now);
 };
 {
  Fixture f;f.setup(type);f.files["MODUNIT.SHP"]=Fixture::shape(9);
  f.files["CAMEO.PAL"]=Fixture::colors();
  unitpanelcameo::Cache c;auto m=model(14,type);
  run(c,f,m,1000);
  assert(c.get(type)&&c.get(type)->argb[0]==0xfffc0000u);
  assert(f.resolves==1&&f.loads>=2);
  auto rev=c.revision(),loads=f.loads;
  run(c,f,m,1100);run(c,f,m,1200);
  assert(c.revision()==rev&&f.loads==loads); // no hot-paint re-decoding
 }
 {
  // A broken archive SHP cannot suppress the native in-memory cameo.
  Fixture f;f.setup(type);f.files["MODUNIT.SHP"]={1,2,3,4};
  f.files["CAMEO.PAL"]=Fixture::colors();
  unitpanelcameo::Cache c;auto m=model(15,type);
  run(c,f,m,1000);
  assert(c.get(type)&&c.get(type)->argb[0]==0xff0000fcu);
 }
 {
  // When MIX is absent, preserve the existing native +0x6F0 converter path.
  Fixture f;f.setup(type);unitpanelcameo::Cache c;auto m=model(16,type);
  run(c,f,m,1000);
  assert(c.get(type)&&c.get(type)->argb[0]==0xff00ff00u);
 }
 {
  // Missing type ID still allows the native fallback, without guessing a
  // Rules/Art filename from old or invalid bytes.
  Fixture f;f.setup(type);f.regions.erase(type+0x24);
  unitpanelcameo::Cache c;auto m=model(17,type);
  run(c,f,m,1000);
  assert(c.get(type)&&f.resolves==0);
 }
 {
  // A changed session identity cannot borrow a stale icon of an identical
  // pointer: the new match gets the new MOD file pixels and palette.
  Fixture f;f.setup(type);f.files["CAMEO.PAL"]=Fixture::colors();
  f.files["MODUNIT.SHP"]=Fixture::shape(9);
  unitpanelcameo::Cache c;
  run(c,f,model(18,type),1000);
  assert(c.get(type)&&c.get(type)->argb[0]==0xfffc0000u);
  f.files["MODUNIT.SHP"]=Fixture::shape(7);
  run(c,f,model(19,type),1001);
  assert(c.get(type)&&c.get(type)->argb[0]==0xff0000fcu);
 }
 {
  // Two different selected type icons must be decoded in distinct slots,
  // at least 180ms apart, rather than blocking paint or scanning per frame.
  Fixture f;f.setup(type);
  constexpr uint32_t second=0x260000;
  auto id=std::vector<uint8_t>(23,0);std::memcpy(id.data(),"SECOND",6);
  f.blob(second+0x24,std::move(id));
  const uint32_t nativePtr=0x370000;
  f.put(second+1776,nativePtr);f.blob(nativePtr,Fixture::shape(7));
  f.files["CAMEO.PAL"]=Fixture::colors();
  unitpanelcameo::Cache c;auto m=model(20,type);m.selectedType(second,2);
  run(c,f,m,1000);assert(c.get(type)&&!c.get(second));
  run(c,f,m,1179);assert(!c.get(second));
  run(c,f,m,1180);assert(c.get(second));
 }
 std::printf("unit-panel MIX: file first, native fallback, palette, session and throttle PASS\n");
}
