// Integration fixture for the production/owned unit monitor's real MIX
// directory and the same SHP/CAMEO.PAL decoder used by its Sampler.
// No game engine or proprietary artwork is needed.
#include "../../external_reader/source/game_archive.h"
#include "../../unit_monitor/source/cameo_reader.h"
#include <cassert>
#include <array>
#include <cstring>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>
#include <cstdint>

namespace {
using Bytes=std::vector<uint8_t>;
struct Store {
 std::map<std::string,Bytes> physical;
 external::assets::Directory directory;
 unsigned diskReads=0,assetLoads=0,nativeReads=0;
 uint32_t nativeType=0x120000,nativeShape=0x320000;
 Bytes native;
 bool readDisk(const std::string&file,uint64_t at,void*out,size_t n){
  ++diskReads;auto i=physical.find(file);
  if(i==physical.end()||at>i->second.size()||n>i->second.size()-at)return false;
  std::memcpy(out,i->second.data()+at,n);return true;
 }
 bool load(const char*nm,Bytes&out){
  ++assetLoads;
  return directory.load(nm,out,[&](const auto&file,uint64_t at,void*dst,size_t len){
   return readDisk(file,at,dst,len);
  },0);
 }
 bool memory(uint32_t at,void*out,size_t n){
  ++nativeReads;
  if(at==nativeType+1776&&n==4){std::memcpy(out,&nativeShape,4);return true;}
  if(at>=nativeShape&&size_t(at-nativeShape)<=native.size()&&
     n<=native.size()-size_t(at-nativeShape)){
   std::memcpy(out,native.data()+size_t(at-nativeShape),n);
   return true;
  }
  return false;
 }
 void build(){
  std::vector<external::assets::Slice> candidates;
  for(auto&entry:physical)candidates.push_back({entry.first,0,entry.second.size()});
  directory.build(candidates,{},[&](const auto&file,uint64_t at,void*out,size_t n){
   return readDisk(file,at,out,n);
  });
 }
};
Store* store=nullptr;
bool load(const char*n,Bytes&b){return store&&store->load(n,b);}
void put16(Bytes&b,size_t at,unsigned value){
 assert(at+2<=b.size());b[at]=uint8_t(value);b[at+1]=uint8_t(value>>8);
}
void put32(Bytes&b,size_t at,uint32_t value){
 assert(at+4<=b.size());
 for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));
}
Bytes shp(uint8_t pixel){
 Bytes b(32+um::IW*um::IH,pixel);
 std::fill(b.begin(),b.begin()+32,0);
 put16(b,2,um::IW);put16(b,4,um::IH);
 put16(b,12,um::IW);put16(b,14,um::IH);
 put32(b,28,32);
 return b;
}
Bytes pal(uint8_t redIndex,uint8_t blueIndex){
 Bytes b(768,0);
 b[size_t(redIndex)*3]=63;
 b[size_t(blueIndex)*3+2]=63;
 return b;
}
Bytes mix(const std::vector<std::pair<std::string,Bytes>>&entries){
 assert(!entries.empty()&&entries.size()<65536);
 const size_t header=6+entries.size()*12;
 Bytes b(header,0);put16(b,0,entries.size());
 size_t cursor=0;
 for(size_t i=0;i<entries.size();++i){
  const size_t at=6+i*12;
  put32(b,at,external::assets::hash(entries[i].first));
  put32(b,at+4,uint32_t(cursor));
  put32(b,at+8,uint32_t(entries[i].second.size()));
  cursor+=entries[i].second.size();
 }
 put32(b,2,uint32_t(cursor));
 for(const auto&e:entries)b.insert(b.end(),e.second.begin(),e.second.end());
 return b;
}
void verify(){
 Store f;store=&f;
 // The normal base game and the MOD's expansion both carry CAMEOMD.MIX.
 // The nested MOD override must win, even though the older base archive also
 // contains the same resource names.
 auto base=mix({{"MODUNIT.SHP",shp(7)},{"CAMEO.PAL",pal(7,9)}});
 auto mod=mix({{"MODUNIT.SHP",shp(9)},{"CAMEO.PAL",pal(9,7)}});
 f.physical["RA2MD.MIX"]=mix({{"CAMEOMD.MIX",base}});
 f.physical["EXPANDMD01.MIX"]=mix({{"CAMEOMD.MIX",mod}});
 f.native=shp(7);f.build();
 assert(f.directory.count()>=4);
 Bytes bytes;
 assert(f.load("MODUNIT.SHP",bytes)&&bytes==shp(9));
 assert(f.load("CAMEO.PAL",bytes)&&bytes==pal(9,7));

 um::Type type{};type.ptr=f.nativeType;
 std::memcpy(type.cameo,"MODUNIT.SHP",12);
 std::array<uint8_t,32+256*1024>scratch{};
 auto memory=[&](uint32_t p,void*out,size_t n){return f.memory(p,out,n);};
 auto resolve=[](uint32_t){return uint32_t(0);};
 assert(um::readCameo(memory,resolve,type,scratch.data(),scratch.size(),load));
 assert(type.iconReady==1&&type.icon[0]==9&&f.nativeReads==0);
 um::Snapshot s{};
 assert(um::readCameoPalette(memory,s,load)&&s.paletteReady);
 assert(s.palette[9]==0xfffc0000u); // the MOD CAMEO.PAL won over base

 // Corrupted MOD image file must not suppress the otherwise valid native
 // +0x6F0 pointer. This exercises the owned/queued panel's fallback as well.
 f.physical["EXPANDMD01.MIX"]=mix({{"CAMEOMD.MIX",mix({
   {"MODUNIT.SHP",Bytes{0,1,2}},{"CAMEO.PAL",pal(9,7)}
 })}});
 f.build();f.nativeReads=0;type.iconReady=0;
 assert(um::readCameo(memory,resolve,type,scratch.data(),scratch.size(),load));
 assert(type.icon[0]==7&&f.nativeReads>0);

 // A type without any native +0x6F0 pointer still succeeds from MIX.
 f.nativeShape=0;
 f.physical["EXPANDMD01.MIX"]=mix({{"CAMEOMD.MIX",mod}});
 f.build();type.iconReady=0;f.nativeReads=0;
 assert(um::readCameo(memory,resolve,type,scratch.data(),scratch.size(),load));
 assert(type.icon[0]==9&&f.nativeReads==0);

 std::puts("PASS production/owned Cameo MIX: nested MOD priority, palette, bad file/native fallback and missing native pointer");
}
}
int main(){verify();}
