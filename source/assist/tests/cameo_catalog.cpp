#include "../../external_reader/source/cameo_catalog.h"
#include "../../unit_monitor/source/core.h"
#include <cassert>
#include <map>
#include <iostream>
using namespace external::assets;
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
 std::cout<<"PASS Rules Image -> Art Cameo/AltCameo, aliases, long filenames, explicit none, cycles, resource changes and flag-2 RLE\n";
}
