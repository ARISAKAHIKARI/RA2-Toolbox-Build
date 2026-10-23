#include "../../external_reader/source/asset_decode.h"
#include <cassert>
#include <iostream>
using namespace external::assets;
static void put(std::vector<uint8_t>&v,size_t p,uint32_t n){assert(p+4<=v.size());memcpy(v.data()+p,&n,4);}
static void real(std::vector<uint8_t>&v,size_t p,float n){memcpy(v.data()+p,&n,4);}
static std::vector<uint8_t> mix(std::string file,std::vector<uint8_t> data){std::vector<uint8_t>b(22+data.size());b[4]=1;put(b,6,data.size());put(b,10,hash(file));put(b,14,0);put(b,18,data.size());memcpy(b.data()+22,data.data(),data.size());return b;}
int main(){
 // CRC32 vectors cross-checked with zlib and the documented TS padding.
 assert(hash("LOCALMD.MIX")==0xfbe0d09du);assert(hash("UNITTEM.PAL")==0x63da7359u);assert(hash("ISOTEM.PAL")==0x5f9d97b9u);assert(hash("TN01.TEM")==0x967a62e6u);assert(hash("CAMEO.SHP")==0x42163d7cu);

 std::map<std::string,std::vector<uint8_t>> disk;auto read=[&](const std::string&n,uint64_t at,void*out,size_t size){auto i=disk.find(name(n));if(i==disk.end()||at+size>i->second.size())return false;memcpy(out,i->second.data()+at,size_t(size));return true;};
 disk["RA2MD.MIX"]=mix("CAMEO.SHP",{1});disk["ECACHEMD01.MIX"]=mix("CAMEO.SHP",{2});disk["EXPANDMD01.MIX"]=mix("CAMEO.SHP",{3});disk["EXPANDMD99.MIX"]=mix("LOCALMD.MIX",mix("CAMEO.SHP",{4}));
 std::vector<Slice> files;for(auto&[n,b]:disk)files.push_back({n,0,b.size()});Directory d;d.build(files,{{"LOCALMD.MIX",{}}},read);std::vector<uint8_t> bytes;assert(d.load("cameo.shp",bytes,read,0)&&bytes==std::vector<uint8_t>{4});
 disk["CAMEO.SHP"]={5};assert(d.load("cameo.shp",bytes,read,1)&&bytes==std::vector<uint8_t>{5});assert(!d.load("../x",bytes,read,1));
 // Native standard nested packs must load without a live node or selected
 // model having been loaded by the game's renderer.
 disk.clear();disk["RA2MD.MIX"]=mix("ISOTEMMD.MIX",mix("SLOPE01.TEM",{7}));
 disk["RA2.MIX"]=mix("CACHE.MIX",mix("HTNK.VXL",{8}));files.clear();for(auto&[n,b]:disk)files.push_back({n,0,b.size()});
 d.build(files,{},read);assert(d.load("SLOPE01.TEM",bytes,read,0)&&bytes==std::vector<uint8_t>{7});assert(d.load("HTNK.VXL",bytes,read,0)&&bytes==std::vector<uint8_t>{8});
 assert(archiveName("C:\\Games\\RA2\\ra2md.mix")=="RA2MD.MIX");assert(archiveName("../").empty());
 // An encrypted directory uses the validated live index, not resource memory.
 disk["EXPANDMD99.MIX"].assign(109,0);put(disk["EXPANDMD99.MIX"],0,0x20000);disk["EXPANDMD99.MIX"][108]=6;files.clear();files.push_back({"EXPANDMD99.MIX",0,109});
 d.build(files,{{"EXPANDMD99.MIX",{{hash("CAMEO.SHP"),0,1}}}},read);assert(d.load("CAMEO.SHP",bytes,read,0)&&bytes==std::vector<uint8_t>{6});
 d.build(files,{},read);assert(!d.load("CAMEO.SHP",bytes,read,0));d.build(files,{{"EXPANDMD99.MIX",{{hash("CAMEO.SHP"),20,1}}}},read);assert(d.count()==0);
 std::array<uint32_t,256> colors{};std::vector<uint8_t> palette(768,63);assert(pal(palette,colors)&&colors[1]==0xfffcfcfc);palette[0]=64;assert(!pal(palette,colors));
 std::vector<uint8_t> tile(20+52+1800,1);put(tile,0,1);put(tile,4,1);put(tile,8,60);put(tile,12,30);put(tile,16,20);std::fill(tile.begin()+20,tile.begin()+72,0);put(tile,32,952);external::voxel::Image image;assert(tmp(tile,0,colors,image)&&image.width==60&&image.height==30);put(tile,16,0xfffffff0);assert(!tmp(tile,0,colors,image));
 // Real on-disk VXL/HVA layouts, independent of the runtime's relocated form.
 std::vector<uint8_t> v(802+28+13+92),h(24+16+48);memcpy(v.data(),"Voxel Animation",15);put(v,20,1);put(v,24,1);put(v,28,13);memcpy(v.data()+802,"body",4);size_t body=830,tail=843;put(v,body,0);put(v,body+4,4);v[body+8]=0;v[body+9]=1;v[body+10]=1;v[body+11]=0;v[body+12]=1;
 put(v,tail,0);put(v,tail+4,4);put(v,tail+8,8);real(v,tail+12,1);for(int i=0;i<3;++i)real(v,tail+76+i*4,1);v[tail+88]=v[tail+89]=v[tail+90]=1;v[tail+91]=4;
 put(h,16,1);put(h,20,1);memcpy(h.data()+24,"body",4);for(int i=0;i<3;++i)real(h,40+i*20,1);external::voxel::Model model;assert(vxl(v,h,model)&&model.faces.size()==6);assert(external::voxel::render(model,0,colors,image));
 v[body+12]=2;model={};assert(!vxl(v,h,model));put(h,20,99);assert(!vxl(v,h,model));
 std::cout<<"PASS loose/expand/ecache/base/nested precedence; encrypted live-index compatibility disk reads, malformed extents; TMP/PAL/VXL/HVA resource decode\n";
}
