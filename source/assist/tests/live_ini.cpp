#include "../../external_reader/source/cameo_catalog.h"
#include <cassert>
#include <iostream>
using namespace external::assets;
struct Memory {
 std::vector<uint8_t>b=std::vector<uint8_t>(0x900000);size_t reads=0;
 bool operator()(uint32_t p,void*out,size_t n){++reads;if(uint64_t(p)+n>b.size())return false;memcpy(out,b.data()+p,n);return true;}
 void put(uint32_t p,uint32_t v){memcpy(b.data()+p,&v,4);}
 void text(uint32_t p,const std::string&s){assert(p+s.size()+1<=b.size());memcpy(b.data()+p,s.c_str(),s.size()+1);}
 void index(uint32_t at,uint32_t table,int count){put(at,table);put(at+4,count);put(at+8,count);}
 void ini(uint32_t root,const std::string&section,const std::string&key,const std::string&value){
  put(root,0x7E1AF4);index(root+0x28,0x14000,1);put(0x14004,0x13000);
  put(0x1300C,0x16000);text(0x16000,section);index(0x1302C,0x1A000,1);put(0x1A004,0x15000);
  put(0x1500C,0x16100);put(0x15010,0x16200);text(0x16100,key);text(0x16200,value);
 }
};
int main(){
 Memory m;auto read=[&](uint32_t p,void*out,size_t n){return m(p,out,n);};LiveINI ini;std::string out;
 m.ini(0x12000,"GI","Cameo"," liveicon.shp ");
 assert(ini.get(read,0x12000,1,"gi","cameo",out)&&out=="liveicon.shp");
 auto before=m.reads;assert(ini.get(read,0x12000,1,"GI","CAMEO",out)&&m.reads-before<10);
 m.text(0x16200,"updated.shp");assert(ini.get(read,0x12000,2,"GI","Cameo",out)&&out=="updated.shp");
 m.text(0x16200,"");assert(ini.get(read,0x12000,3,"GI","Cameo",out)&&out.empty());
 assert(!ini.get(read,0x12000,3,"GI","Missing",out));assert(!ini.get(read,0x12000,3,"Other","Cameo",out));
 m.put(0x12000,0);assert(!ini.get(read,0x12000,3,"GI","Cameo",out)); // invalid root even with warm cache
 m.ini(0x12000,"GI","Cameo","good");m.put(0x1202C,16385);assert(!ini.get(read,0x12000,4,"GI","Cameo",out));
 m.ini(0x12000,"GI","Cameo","good");m.put(0x12030,0);assert(!ini.get(read,0x12000,5,"GI","Cameo",out));
 m.ini(0x12000,"GI","Cameo","good");m.put(0x14004,0xFFFFFFF0);assert(!ini.get(read,0x12000,6,"GI","Cameo",out));
 m.ini(0x12000,"GI","Cameo",std::string(4096,'x'));assert(!ini.get(read,0x12000,7,"GI","Cameo",out));
 m.ini(0x12000,"GI","Cameo","good");int roots=0;
 auto race=[&](uint32_t p,void*out,size_t n){if(p==0x12028&&++roots==3)m.put(0x1202C,0);return m(p,out,n);};
 assert(!ini.get(race,0x12000,8,"GI","Cameo",out));
 m.ini(0x12000,"GI","Cameo","good");int keys=0;
 auto keyRace=[&](uint32_t p,void*out,size_t n){if(p==0x1302C&&++keys==2)m.put(0x13030,0);return m(p,out,n);};
 ini.clear();assert(!ini.get(keyRace,0x12000,9,"GI","Cameo",out));
 // Unreadable/truncated strings and missing keys fall back; explicit blanks do not.
 m.ini(0x12000,"GI","Cameo","");EffectiveINI<decltype(read)> effective(read,0x12000);
 std::string disk="[GI]\nCameo=disk.shp\nPalette=custom\n";effective.parse({disk.begin(),disk.end()});effective.generation(10);
 assert(effective.get("GI","Cameo").empty());assert(effective.get("GI","Palette")=="custom");
 m.put(0x12000,0);assert(effective.get("GI","Cameo")=="disk.shp");
 m.ini(0x12000,"GI","Cameo","actual.shp");m.put(LiveINI::RulesPointer,0x12000);
 EffectiveINI<decltype(read)> rules(read,LiveINI::RulesPointer,true);rules.generation(11);assert(rules.get("GI","Cameo")=="actual.shp");
 m.put(LiveINI::RulesPointer,0xFFFFFFF0);assert(rules.get("GI","Cameo").empty());
 // Both icon consumers use this same resolver. Native per-type overrides take
 // precedence over a live global INI, which need not contain map modifications.
 m.ini(LiveINI::ArtObject,"GI","Cameo","ingame.shp");m.put(LiveINI::RulesPointer,0);
 RuntimeCameos<decltype(read)> icons(read);std::map<std::string,std::string> files{{"RULESMD.INI","[E1]\nImage=GI\n"},{"ARTMD.INI","[GI]\nCameo=ondisk.shp\n"}};
 auto load=[&](const char*n,std::vector<uint8_t>&bytes){auto i=files.find(n);if(i==files.end())return false;bytes.assign(i->second.begin(),i->second.end());return true;};
 assert(icons.resolve(load,1,12,"E1","","")=="INGAME.SHP");
 assert(icons.resolve(load,1,12,"E1","","mapoverride.shp")=="MAPOVERRIDE.SHP");
 assert(icons.resolve(load,1,12,"E1","","none").empty());
 m.text(0x16200,"none");assert(icons.resolve(load,1,13,"E1","","").empty());
 m.put(LiveINI::ArtObject,0);assert(icons.resolve(load,1,14,"E1","","")=="ONDISK.SHP");
 // A key string ending at a readable allocation boundary still works.
 m.ini(0x12000,"GI","Cameo","edge");m.put(0x15010,uint32_t(m.b.size()-5));m.text(uint32_t(m.b.size()-5),"edge");
 assert(ini.get(read,0x12000,15,"GI","Cameo",out)&&out=="edge");
 std::cout<<"PASS live INI case lookup, bounded reads, invalid pointers/counts, concurrent index changes, empty values, cache generations and native/live/file precedence\n";
}
