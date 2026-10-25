#include "../../external_reader/source/scene_lighting.h"
#include "../../external_reader/source/projection.h"
#include "../../external_reader/source/camera_raster.h"
#include <vector>
#include <cassert>
#include <iostream>
using namespace external::lighting;
int main(){
 std::vector<uint8_t> memory(0xC00000);auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;};
 auto put=[&](uint32_t p,auto value){memcpy(memory.data()+p,&value,sizeof value);};
 assert(sample(read)==State{});put(0xA8B230,0x10000u);put(0x1352C,100);
 for(int at:{0x13534,0x13538,0x1353C})put(at,100);
 assert(sample(read)==State{});put(0x1352C,75);put(0x13538,80);auto s=sample(read);assert(s.ambient==75&&s.tint[1]==800);
 assert(pixel(0x7C646464,s)==0x7C4B3C4Bu);assert(pixel(0,s)==0);
 put(0xB054D4,0x20000u);put(0xB054E0,2);put(0x20000,0x21000u);put(0x20004,0x22000u);
 put(0x21310,1);put(0x22310,53);put(0x2230C,0x23000u);
 for(int at:{0x23198,0x2319C,0x231A0})put(at,1000);
 put(0x231A4,1200);put(0x231A8,500);put(0x231AC,800);put(0x231B0,uint8_t(1));
 s=sample(read);assert((s.tint==std::array<int,3>({1200,500,800})));assert(pixel(0x7C646464,s)==0x7C5A253Cu);
 put(0x231B0,uint8_t(0));assert(sample(read).tint==State{}.tint);
 put(0x1352C,200);s=sample(read);assert(pixel(0x7C646464,s)==0x7C7C7C7Cu); // clamp RGB to alpha
 put(0x1352C,0);assert(pixel(0xFF808080,sample(read))==0xFF000000u); // intentional black scene
 put(0x1352C,100);put(0x23198,9000);assert(sample(read).tint[1]==800); // invalid converter falls back to scenario
 auto race=[&](uint32_t p,void*out,size_t n){bool ok=read(p,out,n);if(p==0xA8B230)put(0xA8B230,0x30000u);return ok;};assert(sample(race)==State{});
 put(0xA8B230,0xfffffff0u);assert(sample(read)==State{});
 // Cell values already include ambient, Ground, Level and intensity scale.
 State fallback;fallback.ambient=50;constexpr uint32_t c=0x40000;
 assert(cell(read,c,fallback)==fallback);put(c+0x104,65536u);put(c+0x34,0x23000u);
 put(0x23198,1000);put(0x2319C,800);put(0x231A0,1200);
 put(c+0x10A,int16_t(1100));put(c+0x10C,int16_t(825));put(c+0x10E,int16_t(1350));
 auto object=cell(read,c,fallback),terrain=cell(read,c,fallback,Layer::Terrain),bridge=cell(read,c,fallback,Layer::Bridge);
 assert(object.gains()[0]==1.1f&&terrain.gains()[0]==.825f&&bridge.gains()[0]==1.35f);
 assert(object.tint[1]==800&&cell(read,c,fallback,Layer::Object,false).tint[1]==1000);
 put(c+0x10A,int16_t(0));assert(pixel(0xFF808080,cell(read,c,fallback))==0xFF000000);
 put(c+0x10A,int16_t(3000));assert(cell(read,c,fallback)==fallback);put(c+0x10A,int16_t(1100));
 int reads=0;auto cellRace=[&](uint32_t p,void*out,size_t n){bool ok=read(p,out,n);if(p==c+0x10A&&++reads==1)put(c+0x10C,int16_t(900));return ok;};assert(cell(cellRace,c,fallback)==fallback);
 put(0x87F7E8+0x13C,0x100000u);put(0x87F7E8+0x140,512*512);put(0x100000+(3*512+2)*4,c);
 put(c+0x24,int16_t(2));put(c+0x26,int16_t(3));put(c+0x140,0x80u);put(c+0x11B,int8_t(2));put(0x89E7C0,104);
 assert(at(read,640,896,208,fallback).intensity==1100&&at(read,640,896,624,fallback).intensity==1350);
 assert(at(read,640,896,624,fallback,true).intensity==900);assert(cellAt(read,-1,896)==0);put(c+0x26,int16_t(4));assert(cellAt(read,640,896)==0);
 put(0xB0CD48,.144337567);assert(external::projectionZScale(read)==.144337567);
 assert(external::screenHeight(104,external::projectionZScale(read))==15&&external::screenHeight(728,external::projectionZScale(read))==106);
 put(0xB0CD48,0.);assert(external::projectionZScale(read)>0);put(0xB0CD48,10.);assert(external::projectionZScale(read)<1);
 // The snow terrain drawer can be blue while a building/vehicle's owner
 // drawer is neutral. Native 705D70/705EC1/70718F select them independently.
 constexpr uint32_t house=0x200000,type=0x220000,scheme=0x240000,convert=0x250000,
  customVector=0x260000,customScheme=0x270000,customConvert=0x280000,customItems=0x290000,
  anim=0x2A0000,animType=0x2B0000,plainConvert=0x2C0000,uiScheme=0x2D0000,uiConvert=0x2E0000;
 put(c+0x26,int16_t(3));put(c+0x10A,int16_t(1000));put(c+0x10C,int16_t(900));
 put(0x23198,800);put(0x2319C,830);put(0x231A0,1000);
 put(house+0x16054,1);put(0x20004,scheme);put(scheme+0x30C,convert);put(scheme+0x310,53);put(convert+0x16C,53);
 for(int off:{0x198,0x19C,0x1A0})put(convert+off,1000);
 assert(objectConverter(read,type,house)==convert);
 auto vehicle=external::lighting::object(read,type,house,2,640,896,208,State{});
 auto building=external::lighting::object(read,type,house,3,640,896,208,State{});
 assert(vehicle.intensity==1000&&building==vehicle&&building.tint==State{}.tint);
 auto snow=at(read,640,896,208,State{},true);assert(snow.intensity==900&&(snow.tint==std::array<int,3>{800,830,1000}));
 // A real opaque source pixel stays independent of a dark or white canvas.
 external::voxel::Image gray;gray.width=gray.height=1;gray.pixels={0xffd6d7d6};gray.indices={48};gray.objectPalette=true;
 external::camera::Surface black,white;assert(black.reset(1,1,0xff000000)&&white.reset(1,1,0xffffffff));
 black.blit(gray,0,0,1,1,0,building);white.blit(gray,0,0,1,1,0,building);
 assert(black.color[0]==gray.pixels[0]&&white.color[0]==gray.pixels[0]);
 assert(external::camera::lit(gray.pixels[0],48,true,at(read,640,896,208,State{}))!=black.color[0]);
 put(type+0x1548,int16_t(100));building=external::lighting::object(read,type,house,3,640,896,208,State{});assert(building.intensity==1100);
 assert(external::lighting::object(read,type,house,2,640,896,624,State{}).intensity==1350); // existing bridge intensity remains separate
 // Temporary lighting is read every sample, without recoloring cached art.
 put(convert+0x1A4,1200);put(convert+0x1A8,500);put(convert+0x1AC,800);put(convert+0x1B0,uint8_t(1));
 assert((external::lighting::object(read,type,house,2,640,896,208,State{}).tint==std::array<int,3>{1200,500,800}));
 assert(gray.pixels[0]==0xffd6d7d6);put(convert+0x1B0,uint8_t(0));
 // CustomPalette's vector supplies the tint as well as the raw palette.
 put(type+0xDF0,customVector);put(customVector+4,customItems);put(customVector+16,2);put(customItems+4,customScheme);put(customScheme+0x30C,customConvert);put(customConvert+0x16C,53);
 put(customConvert+0x198,900);put(customConvert+0x19C,1100);put(customConvert+0x1A0,700);
 auto custom=external::lighting::object(read,type,house,3,640,896,208,State{});assert(custom.intensity==1100&&(custom.tint==std::array<int,3>{900,1100,700}));
 put(type+0x1702,uint8_t(1));auto isoBuilding=external::lighting::object(read,type,house,3,640,896,208,State{});assert(isoBuilding==snow); // native overrides ExtraLight
 put(type+0x236,uint8_t(1));assert(external::lighting::object(read,type,house,3,640,896,208,State{})==custom); // voxel still uses owner/custom scheme
 put(type+0x1702,uint8_t(0));put(type+0x236,uint8_t(0));
 put(customVector+16,513);assert(objectConverter(read,type,house)==0);assert(external::lighting::object(read,type,house,3,640,896,208,State{}).tint==State{}.tint);put(customVector+16,2);
 int indexReads=0;auto ownerRace=[&](uint32_t p,void*out,size_t n){bool ok=read(p,out,n);if(p==house+0x16054&&++indexReads==1)put(house+0x16054,0);return ok;};
 assert(objectConverter(ownerRace,type,house)==0);put(house+0x16054,1);
 assert(objectConverter(read,0xfffffff0u,house)==0&&objectConverter(read,type,0xfffffff0u)==0);
 // Native 423114: assigned building light, explicit cell light, ordinary
 // ANIM, AltPalette and IsVeins are distinct branches. NormalLight only
 // overrides brightness, preserving the selected drawer's current RGB.
 put(anim+0xD4,customConvert);put(anim+0xFC,1320);
 auto animLight=animation(read,anim,animType,640,896,208,State{});assert(animLight.intensity==1320&&animLight.tint==custom.tint);
 put(animType+0x35D,uint8_t(1));animLight=animation(read,anim,animType,640,896,208,State{});assert(animLight.intensity==1000&&animLight.tint==custom.tint);
 put(anim+0x196,uint8_t(1));animLight=animation(read,anim,animType,640,896,208,State{});assert(animLight.intensity==1000&&animLight.tint==snow.tint);
 put(animType+0x35D,uint8_t(0));assert(animation(read,anim,animType,640,896,208,State{})==snow);
 put(0xA83D4C,house);put(animType+0x355,uint8_t(1));animLight=animation(read,anim,animType,640,896,208,State{});assert(animLight.intensity==900&&animLight.tint==State{}.tint);
 put(animType+0x355,uint8_t(0));put(anim+0x196,uint8_t(0));put(anim+0xD4,0u);put(0x87F6C0,plainConvert);put(plainConvert+0x16C,53);
 assert(animation(read,anim,animType,640,896,208,State{})==vehicle); // plain Convert has no RGB fields
 put(0x20000,uiScheme);put(uiScheme+0x30C,uiConvert);put(uiConvert+0x16C,1);for(int off:{0x198,0x19C,0x1A0})put(uiConvert+off,1000);
 put(animType+0x361,uint8_t(1));put(c+0x10A,int16_t(800));assert(animation(read,anim,animType,640,896,208,State{}).intensity==1000);
 std::cout<<"PASS ambient, normal/tinted converter, map changes, bounded reads, owner/custom vs terrain RGB, animation source/NormalLight, opaque final pixels and live lighting updates\n";
}
