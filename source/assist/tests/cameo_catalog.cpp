#include "../../external_reader/source/cameo_catalog.h"
#include "../../unit_monitor/source/core.h"
#include "../../unit_monitor/source/cameo_reader.h"
#include <cassert>
#include <map>
#include <iostream>
using namespace external::assets;
static std::map<std::string,std::vector<uint8_t>> files;
static bool loadFile(const char* path,std::vector<uint8_t>& out){
 auto it=files.find(name(path));
 if(it==files.end())return false;
 out=it->second;return true;
}
struct Cache {
 int writes=0;
 bool restore(um::Type&,um::Snapshot&){return false;}
 void remember(const um::Type&,const um::Snapshot&){++writes;}
};
int main(){
 std::map<std::string,std::string> ini{{"RULESMD.INI","[E1]\nImage=GI\n[LONG]\nImage=LongArt\n[ALIAS]\nImage=Root\n[EMPTY]\nImage=NoIcon\n"},
 {"ARTMD.INI","[GI]\nCameo=giicon\nAltCameo=gielite.shp\n[LongArt]\nCameo=long_mod_specific_infantry_icon.shp\n[Root]\nImage=Final\n[Final]\nCameo=aliasicon.shp\n[NoIcon]\nCameo=none\n[LOOP]\nImage=LOOP\n"}};
 auto load=[&](const char*n,std::vector<uint8_t>&b){auto i=ini.find(n);if(i==ini.end())return false;b.assign(i->second.begin(),i->second.end());return true;};
 CameoCatalog c;c.update(load,1);
 assert(c.resolve("E1","E1","LIVEOVERRIDE")== "LIVEOVERRIDE.SHP");
 assert(c.resolve("E1","E1","")== "GIICON.SHP");
 assert(c.resolve("E1","","none").empty());
 assert(c.resolve("e1","","",true)=="GIELITE.SHP");
 assert(c.resolve("LONG","","").size()>24);
 assert(c.resolve("ALIAS","","")=="ALIASICON.SHP");
 assert(c.resolve("EMPTY","","").empty());
 assert(c.resolve("LOOP","","native")== "NATIVE.SHP");
 assert(c.resolve("UNKNOWN","","..\\private").empty());
 ini["ARTMD.INI"]="[GI]\nCameo=newicon.shp\n";c.update(load,2);assert(c.resolve("E1","","")=="NEWICON.SHP");
 // Flag 2 cameos use transparent-run RLE too. Odd-width right padding clips.
 std::vector<uint8_t> shp(40);auto put16=[&](int at,int v){shp[at]=v;shp[at+1]=v>>8;};
 put16(2,5);put16(4,1);put16(6,1);put16(12,5);put16(14,1);shp[16]=2;shp[28]=32;
 put16(32,8);shp[34]=2;shp[35]=0;shp[36]=2;shp[37]=3;shp[38]=0;shp[39]=2;
 uint8_t icon[um::IW*um::IH];assert(um::decode(shp.data(),shp.size(),icon));assert(icon[0]==2&&icon[12]==0&&icon[36]==3&&icon[48]==0);
 shp[39]=0;assert(!um::decode(shp.data(),shp.size(),icon));
 shp[39]=2;
 ini["RULESMD.INI"]="[FUTURE]\nImage=NOART\n";
 ini["ARTMD.INI"]="[FUTURE]\nCameo=FUTUREICON\n";
 c.update(load,3);
 files["FUTUREICON.SHP"]=shp;
 files["CAMEO.PAL"]=std::vector<uint8_t>(768,12);
 unsigned nativeReads=0;
 const auto unavailable=[&](uint32_t,void*,size_t){++nativeReads;return false;};
 const auto resolve=[&](um::Type&t){
  const auto file=c.resolve(t.id,"",t.cameo);
  std::memcpy(t.cameo,file.c_str(),file.size()+1);
 };
 um::Type future{};
 std::strcpy(future.id,"FUTURE");
 static um::Snapshot palette{};
 uint8_t buffer[256]{};
 Cache cache;
 assert(um::readCameoWithMetadata(unavailable,resolve,cache,future,palette,buffer,sizeof buffer,loadFile));
 assert(!nativeReads&&cache.writes==1&&future.icon[0]==2&&palette.palette[2]==0xff303030);
 future.ptr=0x400000;
 for(const auto* overrideName:{"UNAVAILABLE","MALFORMED"}){
  files["MALFORMED.SHP"]={1,2,3};
  std::strcpy(future.cameo,overrideName);
  assert(um::readCameoWithMetadata(unavailable,resolve,cache,future,palette,buffer,sizeof buffer,loadFile));
  assert(!nativeReads&&std::string(future.cameo)=="FUTUREICON.SHP");
 }
 future.ptr=0;std::strcpy(future.cameo,"NONE");
 assert(!um::readCameoWithMetadata(unavailable,resolve,cache,future,palette,buffer,sizeof buffer,loadFile));
 assert(!nativeReads&&cache.writes==3);
 std::cout<<"PASS Rules Image -> Art Cameo/AltCameo, aliases, explicit none, flag-2 RLE, and forecasts with unavailable native cameo names or pointers\n";
}
