"""Production Files -> nested MIX -> host art capture -> SHP/TMP pixels.
Only Win32 file/memory boundaries are synthetic; no Windows visual assertion.
"""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
header=r'''
#pragma once
#include <cstdint>
#include <cstddef>
using DWORD=uint32_t;using HANDLE=void*;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
constexpr unsigned FILE_ATTRIBUTE_DIRECTORY=16,FILE_ATTRIBUTE_NORMAL=128,GENERIC_READ=1,FILE_SHARE_READ=1,FILE_SHARE_WRITE=2,FILE_SHARE_DELETE=4,OPEN_EXISTING=3,FILE_BEGIN=0,GetFileExInfoStandard=0;
struct FILETIME {DWORD dwLowDateTime=0,dwHighDateTime=0;};
struct WIN32_FILE_ATTRIBUTE_DATA {DWORD dwFileAttributes=0;FILETIME ftCreationTime{},ftLastAccessTime{},ftLastWriteTime{};DWORD nFileSizeHigh=0,nFileSizeLow=0;};
struct WIN32_FIND_DATAW:WIN32_FILE_ATTRIBUTE_DATA {wchar_t cFileName[260]{};};
struct LARGE_INTEGER {int64_t QuadPart=0;};
DWORD GetTickCount();uint64_t GetTickCount64();bool GetFileAttributesExW(const wchar_t*,unsigned,WIN32_FILE_ATTRIBUTE_DATA*);
HANDLE CreateFileW(const wchar_t*,unsigned,unsigned,void*,unsigned,unsigned,void*);
bool SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,unsigned);bool ReadFile(HANDLE,void*,DWORD,DWORD*,void*);bool CloseHandle(HANDLE);
HANDLE FindFirstFileW(const wchar_t*,WIN32_FIND_DATAW*);bool FindNextFileW(HANDLE,WIN32_FIND_DATAW*);bool FindClose(HANDLE);
'''
body=r'''
#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include <cwchar>
#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>
#include "source/external_reader/source/game_archive.h"
static std::map<std::string,std::vector<uint8_t>> disk;
static int fileReads=0,rawReads=0;
static std::vector<uint8_t> memory(0x1000000);
static DWORD clockValue=1000,pid=1;static bool imagePathAvailable=true;static uint32_t commandMatch(){return 1;}
namespace external {struct ReadSession {
 bool imagePath(wchar_t*p,DWORD*n){if(!imagePathAvailable)return false;std::wcscpy(p,L"C:\\Game\\gamemd.exe");*n=std::wcslen(p);return true;}
 bool readFresh(uint32_t p,void*out,size_t n){++rawReads;if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}
};}
static external::ReadSession externalReader;
struct OpenFile {const std::vector<uint8_t>*data;size_t pos;};
static std::string filename(const wchar_t*p){std::wstring w=p;auto i=w.find_last_of(L"\\/");w=w.substr(i+1);return external::assets::name(std::string(w.begin(),w.end()));}
DWORD GetTickCount(){return clockValue;}uint64_t GetTickCount64(){return clockValue;}
bool GetFileAttributesExW(const wchar_t*p,unsigned,WIN32_FILE_ATTRIBUTE_DATA*out){auto i=disk.find(filename(p));if(i==disk.end())return false;*out={};out->nFileSizeLow=i->second.size();return true;}
HANDLE CreateFileW(const wchar_t*p,unsigned,unsigned,void*,unsigned,unsigned,void*){auto i=disk.find(filename(p));return i==disk.end()?INVALID_HANDLE_VALUE:new OpenFile{&i->second,0};}
bool SetFilePointerEx(HANDLE h,LARGE_INTEGER n,LARGE_INTEGER*,unsigned){if(n.QuadPart<0)return false;static_cast<OpenFile*>(h)->pos=n.QuadPart;return true;}
bool ReadFile(HANDLE h,void*out,DWORD n,DWORD*got,void*){++fileReads;auto&f=*static_cast<OpenFile*>(h);if(f.pos+n>f.data->size()){*got=0;return false;}memcpy(out,f.data->data()+f.pos,n);f.pos+=n;*got=n;return true;}
bool CloseHandle(HANDLE h){delete static_cast<OpenFile*>(h);return true;}
struct Search {std::vector<std::string> names;size_t at=0;};
bool FindNextFileW(HANDLE h,WIN32_FIND_DATAW*out){auto&s=*static_cast<Search*>(h);if(s.at==s.names.size())return false;auto&n=s.names[s.at++];*out={};std::wstring w(n.begin(),n.end());std::wcscpy(out->cFileName,w.c_str());out->nFileSizeLow=disk.at(n).size();return true;}
HANDLE FindFirstFileW(const wchar_t*,WIN32_FIND_DATAW*out){auto*s=new Search;for(auto&[n,b]:disk)if(n.size()>=4&&n.substr(n.size()-4)==".MIX")s->names.push_back(n);if(FindNextFileW(s,out))return s;delete s;return INVALID_HANDLE_VALUE;}
bool FindClose(HANDLE h){delete static_cast<Search*>(h);return true;}
#include "source/assist/source/game_assets_host.h"
#include "source/external_reader/source/camera_raster.h"
static void word(std::vector<uint8_t>&b,size_t at,uint32_t v){memcpy(b.data()+at,&v,4);}
static void half(std::vector<uint8_t>&b,size_t at,uint16_t v){memcpy(b.data()+at,&v,2);}
static std::vector<uint8_t> partShape(int frames,int x,int firstColor){
 std::vector<uint8_t> b(8+frames*25);half(b,2,2);half(b,4,2);half(b,6,frames);
 for(int f=0;f<frames;++f){int at=8+f*24;half(b,at,1+x);half(b,at+2,1);half(b,at+4,1);half(b,at+6,1);word(b,at+20,8+frames*24+f);b[8+frames*24+f]=uint8_t(firstColor+f);}return b;
}
static std::vector<uint8_t> mix(const std::map<std::string,std::vector<uint8_t>>&files){
 size_t head=10+12*files.size(),size=0;for(auto&[n,b]:files)size+=b.size();std::vector<uint8_t> out(head+size);half(out,4,files.size());word(out,6,size);size_t slot=10,at=0;
 for(auto&[n,b]:files){word(out,slot,external::assets::hash(n));word(out,slot+4,at);word(out,slot+8,b.size());memcpy(out.data()+head+at,b.data(),b.size());slot+=12;at+=b.size();}return out;
}
int main(int argc,char**argv){
 std::vector<uint8_t> shp(8+24*24+24*18);half(shp,2,4);half(shp,4,3);half(shp,6,24);
 for(int f=0;f<24;++f){int at=8+f*24,data=8+24*24+f*18;half(shp,at+4,4);half(shp,at+6,3);word(shp,at+8,2);word(shp,at+20,data);for(int row=0;row<3;++row){half(shp,data+row*6,6);std::fill(shp.begin()+data+row*6+2,shp.begin()+data+row*6+6,3+f);}}
 std::vector<uint8_t> pal(768,32),tmp(20+52+1800,3);word(tmp,0,1);word(tmp,4,1);word(tmp,8,60);word(tmp,12,30);word(tmp,16,20);std::fill(tmp.begin()+20,tmp.begin()+72,0);word(tmp,32,952);
 disk["RA2MD.MIX"]=mix({{"LOCALMD.MIX",mix({{"UNITTEM.PAL",pal},{"ISOTEM.PAL",pal},{"LONGINFANTRY.SHP",shp}})},{"ISOTEMMD.MIX",mix({{"CLEAR01.TEM",tmp}})}});
 // Incomplete higher-priority subarchive must not hide the base E1 resource.
 disk["EXPANDMD99.MIX"]=mix({{"LOCALMD.MIX",mix({{"UNRELATED.SHP",{0}}})}});
 // Real direction LUT order and distinct palette entries make facing test visible.
 for(int i=1;i<256;++i){pal[i*3]=i%64;pal[i*3+1]=(i*2)%64;pal[i*3+2]=(i*3)%64;}
 std::vector<uint8_t> customPal(768,12),theaterPal(768,22);for(int i=1;i<256;++i){customPal[i*3]=i%64;customPal[i*3+1]=(i*2)%64;customPal[i*3+2]=(i*3)%64;}
 std::string artText="[LONGINFANTRY]\nPalette=custom\n[NAWALL]\nNewTheater=yes\n";
 std::string rulesText="[ORE]\nTiberium=yes\n[CUTORE]\nTiberium=yes\n[LIVEORE]\nTiberium=yes\n[NAWEAP]\nNoShadow=yes\n";
 // A shared indexed cutout travels through both archived and loaded SHP paths.
 std::vector<uint8_t> oreCutout(8+24+12);half(oreCutout,2,4);half(oreCutout,4,3);half(oreCutout,6,1);
 half(oreCutout,8+4,4);half(oreCutout,8+6,3);word(oreCutout,8+20,32);
 const uint8_t orePixels[12]={0,3,3,0,3,3,0,3,0,3,3,0};memcpy(oreCutout.data()+32,orePixels,12);
 disk["ECACHEMD98.MIX"]=mix({{"CUTORE.SHP",oreCutout},{"CUTDECOR.SHP",oreCutout},{"ARTMD.INI",{artText.begin(),artText.end()}},{"RULESMD.INI",{rulesText.begin(),rulesText.end()}},{"CUSTOMTEM.PAL",customPal},{"TEMPERAT.PAL",theaterPal},{"NTWALL.SHP",shp},{"ORE.SHP",shp}});
 constexpr uint32_t p=0x10000,t=0x20000,house=0x30000,tileType=0x40000,sequence=0x50000;
 word(memory,p,0x7EB058);word(memory,p+0x10,42);word(memory,p+0x6C0,t);word(memory,p+0x21C,house);memcpy(memory.data()+t+0x24,"E1",3);memcpy(memory.data()+t+0x1F8,"LONGINFANTRY",13);
 memcpy(memory.data()+tileType+0x2F5,"CLEAR01.TEM",12);memcpy(memory.data()+0x7E1B78+68,"ISOTEM",7);memcpy(memory.data()+0x7E1B78+78,"TEM",4);memcpy(memory.data()+0x7E1B78+48,"TEMPERAT",9);memory[0x7E1B78+86]='T';
 word(memory,t+0xE3C,sequence);word(memory,sequence+4,1);word(memory,sequence+8,1);const int dirs[32]={7,7,6,6,6,6,5,5,5,5,4,4,4,4,3,3,3,3,2,2,2,2,1,1,1,1,0,0,0,0,7,7};for(int i=0;i<32;++i)word(memory,0x7EAEFC+i*4,dirs[i]);word(memory,sequence+36,8);word(memory,sequence+40,2);word(memory,sequence+44,2);
 gameassets::update();assert(gameassets::files.count()>=5);
 // The production Context derives voxel sample density from the observed
 // source/target pixels, and keys geometry only by the chosen raster scale.
 {
  gameassets::Context resolution(externalReader);resolution.identity(pid,41);resolution.presentationDetail(1);
  assert(resolution.presentationResolution(100,80,100,80)&&resolution.presentationRasterScale()==1);auto oneRevision=resolution.revision();
  assert(resolution.presentationResolution(100,80,110,88)&&resolution.presentationRasterScale()==2);auto twoRevision=resolution.revision();assert(twoRevision!=oneRevision);
  assert(resolution.presentationResolution(100,80,150,120)&&resolution.presentationRasterScale()==2&&resolution.revision()==twoRevision);
  assert(resolution.presentationResolution(100,80,300,240)&&resolution.presentationRasterScale()==3&&resolution.revision()!=twoRevision);
  assert(!resolution.presentationResolution(0,80,100,80)&&resolution.presentationRasterScale()==0);
  assert(!resolution.presentationResolution(100,80,401,320)&&resolution.presentationRasterScale()==0);
  resolution.presentationDetail(0);auto nativeRevision=resolution.revision();assert(resolution.presentationRasterScale()==1);
  assert(resolution.presentationResolution(100,80,300,240)&&resolution.presentationRasterScale()==1&&resolution.revision()==nativeRevision);
  assert(!resolution.presentationResolution(100,80,-1,80)&&resolution.presentationRasterScale()==1&&resolution.revision()==nativeRevision);
 }
 std::cout<<"PASS production Context observed-resolution raster selection, same-scale revision reuse, unsupported-ratio fallback and native-detail isolation\n";
 gameassets::ActorArt art;assert(gameassets::captureActor(p,t,42,art));assert(!art.voxel&&!art.shape.empty());assert(art.colors[3]==0xff0c1824u);
 external::GameView game(gameassets::context.read);external::voxel::Image image;
 {gameassets::Context other(externalReader);other.identity(pid,commandMatch());gameassets::ActorArt captured;assert(other.captureActor(p,t,42,captured));
  external::visual::Pose pose;std::shared_ptr<const external::voxel::Image>a,b,c;assert(gameassets::context.renderSharedActor(art,pose,a));auto before=external::nativeimages::bank().stats();
  assert(other.renderSharedActor(captured,pose,b)&&a==b&&external::nativeimages::bank().stats().hits==before.hits+1);
  auto identical=captured;identical.object+=0x1000;++identical.id;assert(other.renderSharedActor(identical,pose,c)&&c==a);
  pose.frame=1;assert(other.renderSharedActor(captured,pose,c)&&c!=b);pose.frame=0;captured.colors[3]^=1;assert(other.renderSharedActor(captured,pose,c)&&c!=b);
  other.identity(pid,commandMatch()+1);assert(other.renderSharedActor(captured,pose,c)&&c!=b);
  std::cout<<"PASS production main/watch/map contexts share identical unscaled SHP results; pose, palette and world changes cannot reuse them\n";
 }

 for(int heading=0;heading<32;heading+=4){external::visual::Pose pose;pose.frame=external::visual::previewFrame(game,t,1,heading);assert(gameassets::renderActor(art,pose,image));assert(image.width==4&&image.height==3&&std::count_if(image.pixels.begin(),image.pixels.end(),[](auto c){return c!=0;})==12);assert(pose.frame==dirs[(8+heading)&31]);assert(image.pixels[0]==external::voxel::premultiply(art.colors[3+pose.frame],1,255));}
 // Walk sequence: two frames per facing, distinct indexed colors at each frame.
 word(memory,p+0x6C4,1);for(int body=0;body<32;body+=4)for(int stage=0;stage<2;++stage){half(memory,p+0x388,uint16_t(body*2048));word(memory,p+0xF8,stage);auto pose=external::visual::pose(game,p,t,1,0);assert(pose.frame==8+dirs[body]*2+stage);assert(gameassets::renderActor(art,pose,image));assert(image.pixels[0]==external::voxel::premultiply(art.colors[3+pose.frame],1,255));}
 std::array<uint32_t,256> colors{};assert(gameassets::palette(true,colors));assert(gameassets::tile(tileType,0,colors,image));assert(std::count_if(image.pixels.begin(),image.pixels.end(),[](auto c){return c!=0;})==900);
 // NewTheater overlay names and resource-specific palettes follow FA2 loading.
 constexpr uint32_t wall=0x60000,ore=0x70000;memcpy(memory.data()+wall+0x24,"NAWALL",7);memcpy(memory.data()+ore+0x24,"ORE",4);
 assert(gameassets::overlay(wall,0,0,image));assert(image.pixels[0]!=0);
 assert(gameassets::overlay(ore,0,0,image));assert(image.pixels[0]==external::voxel::premultiply(0xff585858u,1,255));
 // Resource and ordinary overlay colors are solid over either terrain color, with index zero
 // still transparent. Use production Context -> decoder -> Surface composition.
 constexpr uint32_t cutOre=0x72000,liveOre=0x74000,loadedOre=0x76000,cutDecor=0x78000,liveDecor=0x7A000;
 memcpy(memory.data()+cutOre+0x24,"CUTORE",7);memcpy(memory.data()+liveOre+0x24,"LIVEORE",8);
 memcpy(memory.data()+cutDecor+0x24,"CUTDECOR",9);memcpy(memory.data()+liveDecor+0x24,"LIVEDECOR",10);
 memcpy(memory.data()+loadedOre,oreCutout.data(),oreCutout.size());word(memory,liveOre+0xA4,loadedOre);
 for(auto entry:std::vector<std::array<uint32_t,3>>{{cutOre,0,0xff585858},{liveOre,loadedOre,0xff585858},{cutDecor,0,0xff808080},{liveDecor,loadedOre,0xff808080}}){
  assert(gameassets::overlay(entry[0],entry[1],0,image));
  assert(image.width==4&&image.height==3&&image.pixels.size()==12);
  external::camera::Surface dark,bright;assert(dark.reset(4,3,0xff000000)&&bright.reset(4,3,0xffffffff));
  dark.blit(image,2,1,1,1,0,{});bright.blit(image,2,1,1,1,0,{});
  for(size_t n=0;n<image.pixels.size();++n){
   if(orePixels[n]){assert(image.pixels[n]==entry[2]&&image.indices[n]==3);assert(dark.color[n]==entry[2]&&bright.color[n]==entry[2]);}
   else{assert(image.pixels[n]==0&&image.indices[n]==0);assert(dark.color[n]==0xff000000&&bright.color[n]==0xffffffff);}
  }
 }
 // Deliberately translucent formation previews still use the decoder default.
 external::assets::Memory preview{oreCutout};std::array<uint32_t,256> previewPal{};previewPal.fill(0xff585858);
 assert(external::voxel::sprite(preview,external::assets::Memory::Base,previewPal,image)&&image.pixels[1]>>24==124);
 std::cout<<"PASS opaque archived/live resource and ordinary overlays, exact palette colors over black/white ground, transparent index-zero cutouts and isolated translucent previews\n";
 // An all-black uninitialized live scheme must not suppress disk palette fallback.
 word(memory,house+0x16054,0);word(memory,0xB054D0+4,0x80000);word(memory,0xB054D0+16,1);word(memory,0x80000,0x90000);
 assert(!external::voxel::palette(gameassets::context.read,t,house,colors));

 // Cross-theater fallback must carry the matched palette, not the map palette.
 constexpr uint32_t tree=0xa0000;memcpy(memory.data()+tree+0x24,"TREE",5);memory[tree+0x22c]=1;
 disk["TREE.TEM"]=shp;disk["ISOTEM.PAL"]=pal;
 memcpy(memory.data()+0x7E1B78+78,"SNO",4);memcpy(memory.data()+0x7E1B78+68,"ISOSNO",7);memory[0x7E1B78+86]='A';
 clockValue+=6000;gameassets::update();assert(gameassets::overlay(tree,0,0,image,true));assert(image.pixels[0]==external::voxel::premultiply(0xff0c1824u,1,255));
 assert(image.shadowsAfter&&image.attachments.size()==1&&image.attachments[0].shadow&&image.attachments[0].indices[0]==15);
 assert(image.writeDepth&&image.attachments[0].writeDepth);
 memory[tree+0x2B3]=1;assert(gameassets::overlay(tree,0,1,image,true));assert(!image.writeDepth&&!image.attachments[0].writeDepth&&image.attachments[0].indices[0]==16);memory[tree+0x2B3]=0;
 std::cout<<"PASS production archived Terrain body plus actual second-half shadow, shared draw point and animated read-only Z policy\n";
 assert(gameassets::overlay(ore,0,0,image));assert(image.pixels[0]==external::voxel::premultiply(0xff585858u,1,255));
 memcpy(memory.data()+0x7E1B78+78,"TEM",4);memcpy(memory.data()+0x7E1B78+68,"ISOTEM",7);memory[0x7E1B78+86]='T';
 // Native Terrain.Draw selects the resource converter for SpawnsTiberium,
 // not an ISO palette inferred from the current theater or SHP extension.
 // The ID intentionally is not TIBTRE*: the runtime flag owns this rule.
 constexpr uint32_t mine=0x2B8000,mineConvert=0x2C0000,minePalette=0x2C2000;
 memcpy(memory.data()+mine+0x24,"OREMINE",8);memory[mine+0x22C]=1;memory[mine+0x2B1]=1;
 disk["OREMINE.SNO"]=shp;disk["OREMINE.TEM"]=shp;disk["ISOSNO.PAL"]=std::vector<uint8_t>(768,17);disk["UNITSNO.PAL"]=std::vector<uint8_t>(768,29);
 word(memory,0x87F6BC,mineConvert);word(memory,mineConvert,0x7E5358);word(memory,mineConvert+4,2);word(memory,mineConvert+0x16C,53);word(memory,mineConvert+0x174,minePalette);
 for(int i=0;i<256;++i)half(memory,minePalette+i*2,0xBDF7);half(memory,minePalette+3*2,0xF800);
 memcpy(memory.data()+0x7E1B78+78,"SNO",4);memcpy(memory.data()+0x7E1B78+68,"ISOSNO",7);clockValue+=6000;gameassets::update();
 assert(gameassets::overlay(mine,0,0,image,true)&&image.pixels[0]==external::voxel::premultiply(0xFFFF0000u,1,255)&&"ore mine must use the native resource converter, not ISO colors");int mineY=image.y;
 bool overlayChanged=false;half(memory,minePalette+3*2,0x001f);
 assert(gameassets::context.refreshOverlayPalette(mine,0,0,true,overlayChanged)&&overlayChanged);
 assert(gameassets::context.refreshOverlayPalette(mine,0,0,true,overlayChanged)&&!overlayChanged);
 assert(gameassets::overlay(mine,0,0,image,true)&&image.pixels[0]==external::voxel::premultiply(0xff0000ffu,1,255));
 word(memory,mineConvert+0x174,0);assert(!gameassets::context.refreshOverlayPalette(mine,0,0,true,overlayChanged));
 word(memory,mineConvert+0x174,minePalette);assert(gameassets::context.refreshOverlayPalette(mine,0,0,true,overlayChanged)&&!overlayChanged);
 half(memory,minePalette+3*2,0xF800);assert(gameassets::overlay(mine,0,0,image,true));
 memory[mine+0x2B1]=0;assert(gameassets::overlay(mine,0,0,image,true)&&image.pixels[0]==external::voxel::premultiply(0xFF444444u,1,255)&&image.y==mineY+16);
 memory[mine+0x2B1]=1;memcpy(memory.data()+0x7E1B78+78,"TEM",4);memcpy(memory.data()+0x7E1B78+68,"ISOTEM",7);
 assert(gameassets::overlay(mine,0,0,image,true)&&image.pixels[0]==external::voxel::premultiply(0xFFFF0000u,1,255)&&image.y==mineY);
 // Plain ConvertClass has no +188 raw-palette field. If its live table is
 // unavailable, use the original initialization's UNITSNO file as fallback.
 word(memory,mineConvert+0x174,0);assert(gameassets::overlay(mine,0,0,image,true)&&image.pixels[0]==external::voxel::premultiply(0xFF747474u,1,255)&&image.y==mineY);
 word(memory,mineConvert+0x174,minePalette);word(memory,mineConvert+4,1);assert(gameassets::overlay(mine,0,0,image,true)&&image.pixels[0]==external::voxel::premultiply(0xFF747474u,1,255));
 word(memory,mineConvert+4,2);word(memory,0x87F6BC,0);assert(gameassets::overlay(mine,0,0,image,true)&&image.pixels[0]==external::voxel::premultiply(0xFF747474u,1,255));
 std::cout<<"PASS runtime SpawnsTiberium resource palette, ISO tree separation, native -16 offset, theater independence and safe palette fallback\n";
 // Real art through an unloaded GENERICMD archive and RULES -> ART Image aliases.
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> real((std::istreambuf_iterator<char>(f)),{});assert(real.size()>32);
  std::string rules="[E1]\nImage=ALIAS\n[ORE]\nTiberium=yes\n[NAWEAP]\nNoShadow=yes\n",artText="[ALIAS]\nImage=LIVEYURI\n[LIVEYURI]\nPalette=custom\n";
  disk["RULESMD.INI"]={rules.begin(),rules.end()};disk["ARTMD.INI"]={artText.begin(),artText.end()};
  disk["RA2MD.MIX"]=mix({{"GENERICMD.MIX",mix({{"LIVEYURI.SHP",real},{"UNITTEM.PAL",pal},{"ISOTEM.PAL",pal}})}});
  memset(memory.data()+t+0x1F8,0,25);clockValue+=6000;gameassets::update();assert(gameassets::captureActor(p,t,42,art));assert(art.filename=="LIVEYURI.SHP");
  int frames=external::voxel::u16(real.data()+6);for(int frame=0;frame<frames;++frame){external::visual::Pose pose;pose.frame=frame;assert(gameassets::renderActor(art,pose,image));}
  // The same bytes also traverse the overlay-specific lookup/palette branch.
  constexpr uint32_t realOverlay=0xb0000;memcpy(memory.data()+realOverlay+0x24,"LIVEYURI",9);assert(gameassets::overlay(realOverlay,0,5,image));assert(std::any_of(image.pixels.begin(),image.pixels.end(),[](auto c){return c!=0;}));
  std::cout<<"PASS real SHP host pipeline, "<<frames<<" actor frames, overlay frame, GENERICMD and Image aliases\n";
 }

 // Production animation capture must follow native building slots with NULL
 // OwnerObject, then load the selected theater SHP and render Value+Start.
 constexpr uint32_t building=0xc0000,buildingType=0xc8000,anim=0xd0000,animType=0xd8000;
 word(memory,building,0x7E3EBC);word(memory,building+0x10,501);word(memory,building+0x6C,100);word(memory,building+0x520,buildingType);word(memory,building+0x21C,house);word(memory,building+0x55C+3*4,anim);
 word(memory,anim,0x7E3354);word(memory,anim+0x10,502);word(memory,anim+0xC8,animType);memory[anim+0x90]=1;memory[anim+0x118]=1;word(memory,anim+0x9C,26112);word(memory,anim+0xA0,21760);word(memory,anim+0xA4,208);word(memory,animType+0x364,2);word(memory,animType+0x2B4,2);word(memory,anim+0xAC,3);
 memcpy(memory.data()+buildingType+0x24,"CAOILD",7);memcpy(memory.data()+animType+0x24,"CAOILD_A",9);memory[buildingType+0x237]=1;
 disk["CTOILD_A.SHP"]=shp;clockValue+=6000;gameassets::update();
 auto states=external::animations::attached(gameassets::context.read,building,501);assert(states.size()==1&&states[0].frame==5&&states[0].x==26112);
 assert(gameassets::captureAnimation(anim,animType,502,building,501,art));assert(art.kind==5&&art.filename=="CTOILD_A.SHP");
 external::visual::Pose ap;ap.frame=states[0].frame;assert(gameassets::renderActor(art,ap,image));assert(image.pixels[0]==external::voxel::premultiply(art.colors[8],1,255));
 // Current native blend enters the real SHP renderer and remains intrinsic,
 // rather than being turned opaque by a later camera preparation pass.
 const unsigned animationAlpha[]{255,192,128,64};
 for(int blend=1;blend<=3;++blend){ap.variant=blend;assert(gameassets::renderActor(art,ap,image)&&image.intrinsicAlpha);for(auto pixel:image.pixels)if(pixel>>24){unsigned a=pixel>>24;assert(a==animationAlpha[blend]&&((pixel>>16)&255)<=a&&((pixel>>8)&255)<=a&&(pixel&255)<=a);}}
 ap.variant=states[0].blend;assert(ap.variant==0&&gameassets::renderActor(art,ap,image)&&!image.intrinsicAlpha);
 // The animation's assigned LightConvert supplies both palette and tint.
 // A parent's generic unit palette must not replace its actual custom drawer.
 constexpr uint32_t animConvert=0x1C0000,animPalette=0x1C1000,cellConvert=0x1C2000,cellPalette=0x1C3000,lightCell=0x2F0000;
 word(memory,anim+0xD4,animConvert);word(memory,anim+0xFC,1320);word(memory,animConvert,0x7ED0A4);word(memory,animConvert+0x188,animPalette);word(memory,animConvert+0x16C,53);
 for(int i=1;i<256;++i){memory[animPalette+i*3]=208;memory[animPalette+i*3+1]=216;memory[animPalette+i*3+2]=224;}
 for(int off:{0x198,0x19C,0x1A0})word(memory,animConvert+off,1000);
 assert(gameassets::captureAnimation(anim,animType,502,building,501,art)&&art.colors[8]==0xffd0d8e0);assert(gameassets::renderActor(art,ap,image)&&image.pixels[0]==art.colors[8]);
 // Same converter and RGB pointer, changed contents: update palette only.
 bool paletteChanged=false;auto retainedShape=art.shape.data();auto retainedBytes=art.shape.size();uint64_t paletteRevision=art.paletteRevision;
 memory[animPalette+8*3]=90;memory[animPalette+8*3+1]=140;memory[animPalette+8*3+2]=190;
 assert(gameassets::context.refreshActorPalette(art,paletteChanged)&&paletteChanged&&art.colors[8]==0xff5a8cbe&&art.paletteRevision==paletteRevision+1);
 assert(art.shape.data()==retainedShape&&art.shape.size()==retainedBytes);assert(gameassets::renderActor(art,ap,image)&&image.pixels[0]==0xff5a8cbe);
 assert(gameassets::context.refreshActorPalette(art,paletteChanged)&&!paletteChanged);
 word(memory,animConvert,0);auto coherent=art.colors;assert(!gameassets::context.refreshActorPalette(art,paletteChanged)&&art.colors==coherent);
 word(memory,animConvert,0x7ED0A4);assert(gameassets::context.refreshActorPalette(art,paletteChanged)&&!paletteChanged);
 auto assignedLight=external::lighting::animation(gameassets::context.read,anim,animType,26112,21760,208,{});assert(assignedLight.intensity==1320&&assignedLight.tint==external::lighting::State{}.tint);
 // An explicit UseCellLightConvert animation uses the terrain palette/tint;
 // NormalLight leaves that tint in place and only fixes intensity to 1000.
 word(memory,0x87F7E8+0x13C,0x1E0000);word(memory,0x87F7E8+0x140,512*512);word(memory,0x1E0000+4*(85*512+102),lightCell);half(memory,lightCell+0x24,102);half(memory,lightCell+0x26,85);
 word(memory,lightCell+0x34,cellConvert);word(memory,lightCell+0x104,65536);half(memory,lightCell+0x10A,1056);half(memory,lightCell+0x10C,900);half(memory,lightCell+0x10E,1056);
 word(memory,cellConvert,0x7ED0A4);word(memory,cellConvert+0x188,cellPalette);word(memory,cellConvert+0x16C,53);word(memory,cellConvert+0x198,750);word(memory,cellConvert+0x19C,800);word(memory,cellConvert+0x1A0,1000);
 for(int i=1;i<256;++i){memory[cellPalette+i*3]=160;memory[cellPalette+i*3+1]=180;memory[cellPalette+i*3+2]=200;}
 memory[anim+0x196]=1;memory[animType+0x35D]=1;
 assert(gameassets::captureAnimation(anim,animType,502,building,501,art)&&art.colors[8]==0xffa0b4c8);
 auto cellLight=external::lighting::animation(gameassets::context.read,anim,animType,26112,21760,208,{});assert(cellLight.intensity==1000&&(cellLight.tint==std::array<int,3>{750,800,1000}));
 memory[anim+0x196]=memory[animType+0x35D]=0;word(memory,anim+0xD4,0);assert(gameassets::captureAnimation(anim,animType,502,building,501,art));
 assert(!image.writeDepth&&image.pixels[0]>>24==255);memory[anim+0x11A]=1;word(memory,anim+0x11C,9);word(memory,anim+0xAC,0);word(memory,animType+0x2B4,0);
 states=external::animations::attached(gameassets::context.read,building,501);assert(states.size()==1&&states[0].frame==0);ap.frame=0;assert(gameassets::renderActor(art,ap,image)&&image.pixels[0]!=0);
 // A detached/reused pointer cannot load using a stale parent association.
 word(memory,building+0x55C+3*4,0);assert(!gameassets::captureAnimation(anim,animType,502,building,501,art));
 // A voxel still uses 3D depth internally, but not as a terrain depth plane.
 gameassets::ActorArt v;v.voxel=true;v.shadow=false;v.colors.fill(0xff808080);external::voxel::Face face;face.color=1;face.p[0]={0,0,0};face.p[1]={8,0,0};face.p[2]={8,8,0};face.p[3]={0,8,0};v.model.faces.push_back(face);
 assert(gameassets::renderActor(v,{},image)&&image.depth.empty()&&!image.writeDepth&&image.depthBottom!=std::numeric_limits<int>::min());
 // The full production asset renderer forwards observed native posture,
 // retaining source geometry while it rerasterizes a changed slope.
 constexpr uint32_t slopeActor=0x380000,slopeType=0x384000,slopeLoco=0x388000;
 word(memory,slopeActor,0x7F5C70);word(memory,slopeActor+0x674,slopeLoco);word(memory,slopeType+0xB0,1);
 word(memory,slopeLoco,0x7E7EB0);word(memory,slopeLoco+8,slopeActor);word(memory,slopeLoco+0x18,1);word(memory,slopeLoco+0x1C,1);
 half(memory,slopeActor+0x388,8*2048);half(memory,slopeActor+0x38C,8*2048);word(memory,slopeActor+0x390,-1);
 std::array<float,12> slopeMatrix{{.96f,0,.28f,0,0,1,0,0,-.28f,0,.96f,0}};memcpy(memory.data()+0xB45188+48,slopeMatrix.data(),48);
 constexpr uint32_t shadowCell=0x38C000;word(memory,0x1E0000+(30*512+20)*4,shadowCell);half(memory,shadowCell+0x24,20);half(memory,shadowCell+0x26,30);memory[shadowCell+0x11C]=1;
 word(memory,slopeActor+0x9C,20*256+128);word(memory,slopeActor+0xA0,30*256+128);word(memory,slopeActor+0xA4,52);
 word(memory,0x89E7C0,104);memory[0x89E770]=7;double slopeScale=104./256,zScale=.14433756729740643;memcpy(memory.data()+0x89E740,&slopeScale,8);memcpy(memory.data()+0xB0CD48,&zScale,8);
 std::array<double,5> floorTerms{{1,0,0,104,0}};memcpy(memory.data()+0x81C900,floorTerms.data(),sizeof floorTerms);
 auto liveSlope=external::visual::pose(game,slopeActor,slopeType,2,10);assert(liveSlope.voxel&&liveSlope.posture.active&&liveSlope.posture.available&&liveSlope.shadow.known&&liveSlope.shadow.posture.available&&liveSlope.shadow.drop==0);
 auto geometryData=v.model.faces.data();auto geometrySize=v.model.faces.size();int slopeFileReads=fileReads;
 external::voxel::Image flatVoxel,slopedVoxel;assert(gameassets::renderActor(v,{},flatVoxel)&&gameassets::renderActor(v,liveSlope,slopedVoxel));
 assert(flatVoxel.pixels!=slopedVoxel.pixels&&v.model.faces.data()==geometryData&&v.model.faces.size()==geometrySize&&fileReads==slopeFileReads);
 // GPU mode defers the expensive pixel raster while retaining current native
 // lighting/posture. The same packet materializes only on CPU fallback.
 gameassets::context.presentationGPU(true);external::voxel::Image gpuVoxel,materialized;
 assert(gameassets::renderActor(v,liveSlope,gpuVoxel)&&gpuVoxel.gpuRaster&&gpuVoxel.pixels.empty()&&gpuVoxel.indices.empty()&&gpuVoxel.depth.empty()&&!gpuVoxel.writeDepth);
 assert(external::camera::pixels(gpuVoxel)>=size_t(gpuVoxel.width)*gpuVoxel.height&&external::gpuvoxel::materialize(gpuVoxel,materialized));
 assert(materialized.pixels==slopedVoxel.pixels&&materialized.indices==slopedVoxel.indices&&materialized.depth.empty()&&!materialized.gpuRaster&&!materialized.writeDepth);
 gameassets::context.presentationGPU(false);v.shadow=true;external::voxel::Image cpuShadowed;assert(gameassets::renderActor(v,liveSlope,cpuShadowed));
 auto flatShadow=liveSlope;flatShadow.shadow.posture.ramp=external::voxelposture::identity();flatShadow.shadow.posture.active=false;
 auto broadFootprint=v;for(auto&f:broadFootprint.model.faces)for(auto&p:f.p){p.x*=4;p.y*=4;}
 external::voxel::Image broadSloped,broadFlat;assert(gameassets::renderActor(broadFootprint,liveSlope,broadSloped)&&gameassets::renderActor(broadFootprint,flatShadow,broadFlat));
 assert(broadSloped.attachments[0].pixels!=broadFlat.attachments[0].pixels&&"Cell ramp changes the shadow footprint independently of the hull draw matrix");
 gameassets::context.presentationGPU(true);assert(gameassets::renderActor(v,liveSlope,gpuVoxel)&&gpuVoxel.attachments.size()==1&&gpuVoxel.attachments[0].gpuRaster&&gpuVoxel.attachments[0].shadow);
 external::camera::Surface cpuFallback,expectedFallback;assert(cpuFallback.reset(128,128)&&expectedFallback.reset(128,128));cpuFallback.blit(gpuVoxel,64,64,1,1,100,{});expectedFallback.blit(cpuShadowed,64,64,1,1,100,{});
 assert(cpuFallback.color==expectedFallback.color&&cpuFallback.depth==expectedFallback.depth&&v.model.faces.data()==geometryData&&fileReads==slopeFileReads);
 gameassets::context.presentationGPU(false);v.shadow=false;
 std::cout<<"PASS production GPU VXL/flattened shadow deferral, current posture/color packet, bounded cache accounting and identical same-frame CPU fallback\n";
 std::cout<<"PASS production native Drive posture -> retained VXL model -> changed slope pixels without geometry decode or disk IO\n";
 // SHP vehicles have an independent 32-facing turret bank in this same SHP.
 // Exercise both owned archive bytes and direct in-memory SHP decode paths.
 constexpr uint32_t shpActor=0x390000,shpType=0x394000,shpPointer=0x398000;word(memory,shpType+0xB0,0);memory[shpType+0xCA1]=1;memory[shpType+0xE5C]=1;word(memory,shpActor+0x6C0,uint32_t(-1));word(memory,shpActor+0x3A8,uint32_t(-1));
 auto turretShape=std::vector<uint8_t>(8+96*26);half(turretShape,2,2);half(turretShape,4,1);half(turretShape,6,96);
 for(int f=0;f<96;++f){size_t at=8+f*24,data=8+96*24+f*2;half(turretShape,at,1);half(turretShape,at+2,1);half(turretShape,at+4,2);half(turretShape,at+6,1);word(turretShape,at+20,uint32_t(data));}
 turretShape[8+96*24]=5;turretShape[8+96*24+1]=6;turretShape[8+96*24+12*2+1]=8;turretShape[8+96*24+28*2]=9;
 memcpy(memory.data()+shpPointer,turretShape.data(),turretShape.size());
 gameassets::ActorArt shpVehicle;shpVehicle.kind=2;shpVehicle.pointer=shpPointer;shpVehicle.shadow=false;shpVehicle.colors[5]=0xff00aa00;shpVehicle.colors[6]=0xff0000bb;shpVehicle.colors[8]=0xffcc0000;shpVehicle.colors[9]=0xffdddd00;
 for(bool archived:{true,false}){
  shpVehicle.shape=archived?turretShape:std::vector<uint8_t>{};half(memory,shpActor+0x3A0,0);half(memory,shpActor+0x3A4,0);
  auto pose=external::visual::pose(game,shpActor,shpType,2,10);assert(pose.frame==0&&pose.turretFrame==12);external::voxel::Image first;
  assert(gameassets::renderActor(shpVehicle,pose,first)&&first.attachments.empty()&&!first.incomplete&&first.indices==std::vector<uint8_t>({5,8}));
  external::camera::Surface surface;assert(surface.reset(64,64));surface.blit(first,20,20,1,1,100,{});size_t leftPixel=size_t(20+first.y)*64+20+first.x;assert(surface.color[leftPixel]==0xff00aa00&&surface.color[leftPixel+1]==0xffcc0000);
  half(memory,shpActor+0x3A0,16*2048);half(memory,shpActor+0x3A4,16*2048);pose=external::visual::pose(game,shpActor,shpType,2,10);assert(pose.frame==0&&pose.turretFrame==28);
  assert(gameassets::renderActor(shpVehicle,pose,image)&&image.indices==std::vector<uint8_t>({9,6}));assert(surface.reset(64,64));surface.blit(image,20,20,1,1,100,{});assert(surface.color[leftPixel]==0xffdddd00&&surface.color[leftPixel+1]==0xff0000bb);
  pose.turretFrame=1000;assert(gameassets::renderActor(shpVehicle,pose,image)&&image.incomplete&&gameassets::failure.find("vehicle-turret/decode:")==0);
 }
 std::cout<<"PASS SHP vehicle independent turret frame: archive/live same-shape palette and origin, transparent cutouts, unchanged hull and missing-turret retry\n";
 std::cout<<"PASS production absolute-slot animation -> theater SHP -> Value+Start pixels, power-off first frame, stale slot rejection and voxel scene-depth handoff\n";
 // Successful archived bytes remain resident; repeated frames do no file reads.
 std::vector<uint8_t> bytes;assert(gameassets::files.load("CUSTOMTEM.PAL",bytes));int reads=fileReads;
 for(int i=0;i<50;++i)assert(gameassets::files.load("CUSTOMTEM.PAL",bytes));assert(fileReads==reads);
 // Separate make art, native stage and selling reverse order.
 memcpy(memory.data()+buildingType+0xE5C,"CAOILDMK",9);memory[buildingType+0x237]=1;word(memory,buildingType+0xF04,0);word(memory,buildingType+0xF08,12);word(memory,buildingType+0xA0,100);word(memory,building+0x534,0);word(memory,building+0xF8,3);word(memory,building+0xAC,18);
 disk["CTOILDMK.SHP"]=shp;assert(gameassets::captureActor(building,buildingType,501,art)&&art.filename=="CTOILDMK.SHP");
 ap.frame=gameassets::buildingFrame(building,buildingType);assert(ap.frame==3&&gameassets::renderActor(art,ap,image));
 word(memory,building+0xAC,19);assert(gameassets::buildingFrame(building,buildingType)==8);ap.frame=8;assert(gameassets::renderActor(art,ap,image));
 assert(external::visual::pose(game,building,buildingType,3,0).variant==-2);
 // Independent factory parts are SHPReference fields, not AnimClass slots.
 // All six references deliberately remain unloaded; actual file discovery and
 // per-part filenames must supply their bytes without substituting the body.
 constexpr uint32_t factory=0x100000,factoryType=0x108000,links=0x110000,unit=0x118000,unitType=0x120000,runtimeRules=0x128000;
 word(memory,factory,0x7E3EBC);word(memory,factory+0x10,701);word(memory,factory+0x520,factoryType);word(memory,factory+0x21C,house);word(memory,factory+0x6C,100);word(memory,factory+0x534,1);memory[factory+0x6E4]=1;word(memory,factoryType+0xA0,100);memory[factoryType+0x237]=1;
 memcpy(memory.data()+factoryType+0x24,"NAWEAP",7);memcpy(memory.data()+factoryType+0xDD0,"CUSTOM",7);disk["NTWEAP.SHP"]=partShape(4,0,3);
 const char* names[]={"NGWEAPBB.SHP","NGWEAP_2.SHP","NGWEAP_4.SHP","NGWEAP_1.SHP","NGWEAP_3.SHP","NGWEAPDO.SHP"};const int xs[]={3,0,0,6,6,9};
 size_t partBytes=0;
 for(int i=0;i<external::buildings::PartCount;++i){uint32_t reference=0x130000+i*0x1000;half(memory,reference,65535);word(memory,reference+8,reference+0x100);memcpy(memory.data()+reference+0x100,names[i],strlen(names[i])+1);word(memory,factoryType+external::buildings::partOffsets[i],reference);disk[names[i]]=partShape(32,xs[i],20+i*32);partBytes+=disk[names[i]].size();}
 disk["CTOILDMK.SHP"]=shp;memcpy(memory.data()+factoryType+0xE5C,"CAOILDMK",9);word(memory,factoryType+0xF08,12);
 clockValue+=6000;gameassets::update();
 word(memory,0x8871E0,runtimeRules);double yellow=.5;memcpy(memory.data()+runtimeRules+0x1700,&yellow,8);
 gameassets::ActorArt factoryArt;assert(gameassets::captureActor(factory,factoryType,701,factoryArt)&&factoryArt.filename=="NTWEAP.SHP"&&!factoryArt.shadow);
 for(int i=0;i<external::buildings::PartCount;++i)assert(factoryArt.buildingParts[i].filename==names[i]&&!factoryArt.buildingParts[i].shape.empty());
 assert(gameassets::artBytes(factoryArt)>=factoryArt.shape.size()+partBytes);
 external::visual::Pose fp;gameassets::buildingPose(factory,factoryType,fp);assert(fp.frame==0&&fp.doorFrame==-1&&fp.variant==-1);
 assert(gameassets::renderActor(factoryArt,fp,image)&&image.attachments.size()==1);assert(image.attachments[0].flat&&image.attachments[0].writeDepth&&image.attachments[0].depthBias==1&&image.attachments[0].indices[0]==20);
 auto beyond=fp;beyond.frame=20;assert(gameassets::renderActor(factoryArt,beyond,image)&&image.indices[0]==5&&image.attachments[0].indices[0]==40); // all bodies clamp at half, Bib keeps GetCurrentFrame
 word(memory,factory+0x6C,50);gameassets::buildingPose(factory,factoryType,fp);assert(fp.frame==1);assert(gameassets::renderActor(factoryArt,fp,image)&&image.indices[0]==4&&image.attachments[0].indices[0]==21); // equality is damaged for body and Bib
 word(memory,factory+0x6C,100);word(memory,factory+0xAC,16);word(memory,factoryType+0xF00,8);word(memory,factory+0x358,90);word(memory,factory+0x360,20);word(memory,factory+0x364,20);memory[factory+0x368]=memory[factory+0x369]=1;word(memory,0xA8ED84,100);
 gameassets::buildingPose(factory,factoryType,fp);assert(fp.doorFrame==4&&gameassets::renderActor(factoryArt,fp,image));assert(image.indices[0]==52&&image.depthBias==20&&image.attachments.size()==3);
 beyond=fp;beyond.frame=20;assert(gameassets::renderActor(factoryArt,beyond,image)&&image.indices[0]==68&&image.attachments[0].indices[0]==40);assert(gameassets::renderActor(factoryArt,fp,image));
 assert(image.attachments[0].indices[0]==20&&image.attachments[1].indices[0]==116&&image.attachments[2].indices[0]==184);assert(image.attachments[1].flat&&image.attachments[1].writeDepth&&image.attachments[1].depthBias==0);assert(image.attachments[2].flat&&!image.attachments[2].writeDepth&&image.attachments[2].depthBias==5);
 external::camera::Surface composite;assert(composite.reset(24,12));composite.blit(image,3,3,1,1,30,{},2);
 for(const auto&part:image.attachments)assert(composite.color[3*24+3+part.x]==part.pixels[0]);assert(composite.color[3*24+3]==image.pixels[0]);
 auto doorPixel=image.attachments[2].pixels[0];word(memory,0xA8ED84,105);gameassets::buildingPose(factory,factoryType,fp);assert(fp.doorFrame==6&&gameassets::renderActor(factoryArt,fp,image)&&image.attachments[2].pixels[0]!=doorPixel); // body stays frame zero while the real door moves
 word(memory,factory+0xE4,links);word(memory,factory+0xE8,1);word(memory,links,unit);word(memory,unit,0x7F5C70);word(memory,unit+0x6C4,unitType);memory[unitType+0xD94]=1;
 word(memory,factory+0x6C,50);memory[factoryType+0x1700]=1;gameassets::buildingPose(factory,factoryType,fp);assert(fp.doorFrame==14&&gameassets::renderActor(factoryArt,fp,image));assert(image.indices[0]==85&&image.depthBias==40&&image.attachments[1].indices[0]==149&&image.attachments[2].indices[0]==194);
 // Building shadow depth is independent of the production body's +40 plane.
 // Native DrawObject draws each shadow after its color, even for a door which
 // does not write depth. Offset the roof shadow beyond the body to exercise a
 // foreground depth edge, rather than merely checking stored metadata.
 auto shadowArt=factoryArt;shadowArt.shadow=true;half(shadowArt.buildingParts[external::buildings::RoofDeploy].shape,8+(fp.frame+16)*24,2);
 assert(gameassets::renderActor(shadowArt,fp,image)&&image.attachments.size()==4&&image.shadowsAfter);assert(image.attachments[0].shadow&&image.attachments[0].depthBias==2);
 for(size_t i=1;i<image.attachments.size();++i){const auto&part=image.attachments[i];assert(part.attachments.size()==1&&part.attachments[0].shadow&&part.attachments[0].depthBias==2&&part.shadowsAfter);assert(part.attachments[0].writeDepth==part.writeDepth);}
 assert(composite.reset(24,12));composite.color[3*24+4]=0xffe0c080;composite.depth[3*24+4]=25;composite.blit(image,3,3,1,1,30,{},2);assert(composite.color[3*24+3]==image.pixels[0]&&composite.color[3*24+4]==0xffe0c080);
 const auto&doorImage=image.attachments.back();uint32_t halfDoor=(doorImage.pixels[0]&0xff000000u)|((doorImage.pixels[0]&0xfefefeu)>>1);assert(composite.color[3*24+12]==halfDoor);
 // With only process memory available, an unloaded Bib must not hide the
 // already decoded factory. Cached partial art becomes complete when the
 // engine later populates that same SHPReference; no synthetic frame advance.
 auto partialArt=factoryArt;auto&bib=partialArt.buildingParts[external::buildings::Bib];bib.shape.clear();assert(gameassets::renderActor(partialArt,fp,image)&&image.incomplete&&image.attachments.size()==2&&image.indices[0]==85);
 constexpr uint32_t loadedBib=0x1A0000;memcpy(memory.data()+loadedBib,disk[names[0]].data(),disk[names[0]].size());word(memory,bib.pointer+12,loadedBib);assert(gameassets::renderActor(partialArt,fp,image)&&!image.incomplete&&image.attachments.size()==3);word(memory,bib.pointer+12,0);
 word(memory,factory+0xAC,0);gameassets::buildingPose(factory,factoryType,fp);assert(gameassets::renderActor(factoryArt,fp,image)&&image.indices[0]==4&&image.attachments.size()==1); // never loop idle factories through production art
 // Construction and early placement both use make art. Only BState controls
 // vanilla Bib visibility; ActuallyPlacedOnMap independently selects GetImage.
 word(memory,factory+0x534,0);word(memory,factory+0xF8,3);gameassets::buildingPose(factory,factoryType,fp);assert(fp.variant==-2&&gameassets::captureActor(factory,factoryType,701,art)&&art.filename=="CTOILDMK.SHP");assert(gameassets::renderActor(art,fp,image)&&image.attachments.empty());
 word(memory,factory+0x534,1);memory[factory+0x6E4]=0;gameassets::buildingPose(factory,factoryType,fp);assert(fp.variant==-2&&gameassets::captureActor(factory,factoryType,701,art)&&art.filename=="CTOILDMK.SHP");
 memory[factory+0x6E4]=1;word(memory,factory+0xF8,0);gameassets::buildingPose(factory,factoryType,fp);assert(fp.variant==-1&&gameassets::captureActor(factory,factoryType,701,art)&&art.filename=="NTWEAP.SHP"&&gameassets::renderActor(art,fp,image));
 // A completed sample can precede the live art-mode switch by a frame.
 // Capture the requested make/normal bank, never file bytes from the later
 // BState under the earlier cache key. Exercise the actual Context provider.
 auto sampledNormal=fp,sampledMake=fp;sampledMake.variant=-2;sampledMake.frame=3;
 assert(gameassets::context.captureActor(factory,factoryType,701,art,&sampledMake)&&art.filename=="CTOILDMK.SHP");
 assert(gameassets::renderActor(art,sampledMake,image)&&image.indices[0]==6);
 word(memory,factory+0x534,0);
 assert(gameassets::context.captureActor(factory,factoryType,701,art,&sampledNormal)&&art.filename=="NTWEAP.SHP");
 assert(gameassets::renderActor(art,sampledNormal,image)&&image.indices[0]==3+sampledNormal.frame);
 word(memory,factory+0x534,1);
 // A naval yard's base is its normal body. Its production is slot 8 and must
 // keep native Value+Start, independent of all optional factory-door pointers.
 constexpr uint32_t yard=0x180000,yardType=0x188000,yardAnim=0x190000,yardAnimType=0x198000;
 word(memory,yard,0x7E3EBC);word(memory,yard+0x10,801);word(memory,yard+0x520,yardType);word(memory,yard+0x21C,house);word(memory,yard+0x6C,100);word(memory,yard+0x534,1);memory[yard+0x6E4]=1;word(memory,yardType+0xA0,100);memcpy(memory.data()+yardType+0x24,"GAYARD",7);memcpy(memory.data()+yardType+0xDD0,"CUSTOM",7);memory[yardType+0x237]=1;disk["GTYARD.SHP"]=partShape(4,0,7);disk["GTYARD_D.SHP"]=partShape(32,6,30);
 word(memory,yard+0x55C+8*4,yardAnim);word(memory,yardAnim,0x7E3354);word(memory,yardAnim+0x10,802);word(memory,yardAnim+0xC8,yardAnimType);memory[yardAnim+0x90]=1;memory[yardAnim+0x118]=1;word(memory,yardAnim+0x9C,26112);word(memory,yardAnim+0xA0,21760);word(memory,yardAnimType+0x364,2);word(memory,yardAnimType+0x2B4,2);word(memory,yardAnim+0xAC,3);memcpy(memory.data()+yardAnimType+0x24,"GAYARD_D",9);
 assert(gameassets::captureActor(yard,yardType,801,art)&&art.filename=="GTYARD.SHP");gameassets::buildingPose(yard,yardType,fp);assert(gameassets::renderActor(art,fp,image));for(const auto&part:art.buildingParts)assert(!part.pointer);auto yardBase=image;
 states=external::animations::attached(gameassets::context.read,yard,801);assert(states.size()==1&&states[0].frame==5);assert(gameassets::captureAnimation(yardAnim,yardAnimType,802,yard,801,art)&&art.filename=="GTYARD_D.SHP");fp={};fp.frame=states[0].frame;assert(gameassets::renderActor(art,fp,image)&&image.indices[0]==35);
 word(memory,yardAnim+0xAC,4);states=external::animations::attached(gameassets::context.read,yard,801);fp.frame=states[0].frame;assert(gameassets::renderActor(art,fp,image)&&image.indices[0]==36);
 word(memory,yardAnim+0xAC,0);word(memory,yardAnimType+0x2B4,0);states=external::animations::attached(gameassets::context.read,yard,801);fp.frame=states[0].frame;assert(fp.frame==0&&gameassets::renderActor(art,fp,image)&&image.indices[0]==30);
 assert(composite.reset(24,12));composite.blit(yardBase,3,3,1,1,30,{},2);auto basePixel=composite.color[3*24+3];composite.blit(image,3,3,1,1,30,{},2);assert(composite.color[3*24+3]==basePixel&&composite.color[3*24+9]==image.pixels[0]);
 word(memory,yard+0x55C+8*4,0);assert(!gameassets::captureAnimation(yardAnim,yardAnimType,802,yard,801,art));word(memory,yard+0x55C+8*4,yardAnim);word(memory,yard+0x10,803);assert(!gameassets::captureAnimation(yardAnim,yardAnimType,802,yard,801,art));
 std::cout<<"PASS production asset pipeline: unloaded Bib/factory parts, normal/roof unload, native door and damage frames, composition, construction gate, naval base and production slot\n";
 // Standalone warhead/explosion art uses ANIM.PAL, with no building slot.
 disk["ANIM.PAL"]=pal;disk["EXPLODE.SHP"]=shp;memcpy(memory.data()+animType+0x24,"EXPLODE",8);memory[anim+0x118]=0;
 assert(gameassets::captureAnimation(anim,animType,502,0,0,art)&&art.filename=="EXPLODE.SHP");ap.frame=0;assert(gameassets::renderActor(art,ap,image));
 // Garrisoned civilian houses change the body SHP frame immediately; they
 // keep the same source art/palette. No synthetic animation is needed.
 constexpr uint32_t civic=0x2E0000,civicType=0x2E8000;
 word(memory,civic,0x7E3EBC);word(memory,civic+0x10,901);word(memory,civic+0x520,civicType);word(memory,civic+0x21C,house);word(memory,civic+0x6C,100);word(memory,civic+0x534,1);memory[civic+0x6E4]=1;
 word(memory,civicType+0xA0,100);word(memory,civicType+0x634,uint32_t(-1));memory[civicType+0x157B]=1;memcpy(memory.data()+civicType+0x24,"CIVHOUSE",9);memcpy(memory.data()+civicType+0xDD0,"CUSTOM",7);
 disk["CIVHOUSE.SHP"]=partShape(8,0,120);double red=.25;memcpy(memory.data()+runtimeRules+0x1708,&red,8);clockValue+=6000;gameassets::update();
 gameassets::ActorArt civicArt;assert(gameassets::captureActor(civic,civicType,901,civicArt)&&civicArt.filename=="CIVHOUSE.SHP");auto civicColors=civicArt.colors;
 external::visual::Pose civicPose;for(int count:{0,1,3,0}){word(memory,civic+0x694,count);gameassets::buildingPose(civic,civicType,civicPose);assert(civicPose.frame==(count?2:0));assert(gameassets::renderActor(civicArt,civicPose,image)&&image.indices[0]==120+civicPose.frame&&civicArt.colors==civicColors);}
 word(memory,civic+0x694,1);word(memory,civic+0x6C,40);gameassets::buildingPose(civic,civicType,civicPose);assert(civicPose.frame==2&&gameassets::renderActor(civicArt,civicPose,image)&&image.indices[0]==122);
 word(memory,civic+0x6C,25);gameassets::buildingPose(civic,civicType,civicPose);assert(civicPose.frame==1&&gameassets::renderActor(civicArt,civicPose,image)&&image.indices[0]==121);
 word(memory,civicType+0x634,1);gameassets::buildingPose(civic,civicType,civicPose);assert(civicPose.frame==3&&gameassets::renderActor(civicArt,civicPose,image)&&image.indices[0]==123);
 word(memory,civicType+0x634,uint32_t(-1));word(memory,civic+0x6C,30);red=.4;memcpy(memory.data()+runtimeRules+0x1708,&red,8);gameassets::buildingPose(civic,civicType,civicPose);assert(civicPose.frame==1);
 red=.25;memcpy(memory.data()+runtimeRules+0x1708,&red,8);gameassets::buildingPose(civic,civicType,civicPose);assert(civicPose.frame==2);
 std::cout<<"PASS garrison entry/exit body frames, unchanged source palette, civilian/buildable damage states and live ConditionRed\n";
 // TerrainPalette is a runtime building flag, independent of INI fallback.
 // Give ISO art a distinct color so an accidental unit/custom palette fails.
 disk["ISOTEM.PAL"]=std::vector<uint8_t>(768,17);clockValue+=6000;gameassets::update();
 word(memory,factory+0x534,1);word(memory,factory+0xAC,0);memory[factory+0x6E4]=1;memory[factoryType+0x1702]=1;
 assert(gameassets::captureActor(factory,factoryType,701,art)&&art.colors[3]==0xff444444);
 memory[factoryType+0x1702]=0;assert(gameassets::captureActor(factory,factoryType,701,art)&&art.colors[3]!=0xff444444);
 // No selected bit, memory SHP, VXL, palette or live MIX index was supplied.
 assert(memory[p+0x83]==0);disk.clear();clockValue+=6000;gameassets::update();assert(!gameassets::tile(tileType,0,colors,image));assert(gameassets::failure.find("tile-not-found:CLEAR01.TEM")==0);
 std::cout<<"PASS production file discovery, nested base/override fallback, unloaded unselected infantry at eight headings, TMP pixels and missing-resource diagnostic\n";
 // VXL geometry can already be captured from memory when VOXELS.VPL cannot
 // be opened from disk. The original renderer's loaded table remains the
 // authoritative color mapping, including after a cached fallback pose.
 gameassets::Context lighting(externalReader);lighting.identity(pid,2);
 v.model.faces[0].normalType=4;v.model.faces[0].normalIndex=0;v.model.faces[0].hasNormal=true;
 v.colors[1]=0xff303030;v.colors[2]=0xff4080c0;v.colors[3]=0xffc04080;v.colors[7]=0xff80c040;
 auto checkVoxel=[&](uint8_t index){assert(lighting.renderActor(v,{},image));unsigned covered=0;
  for(size_t i=0;i<image.pixels.size();++i)if(image.pixels[i]){++covered;assert(image.indices[i]==index&&image.pixels[i]==v.colors[index]);}
  assert(covered>10);
 };
 auto liveVpl=[&](uint8_t index){word(memory,0xB2FB58,32);std::fill(memory.begin()+0xB41178,memory.begin()+0xB43178,index);};
 auto fileVpl=[&](uint8_t index){std::vector<uint8_t>b(784+32*256,index);word(b,8,32);disk["VOXELS.VPL"]=std::move(b);};
 assert(!lighting.voxelLight.ready()&&lighting.renderActor(v,{},image));
 uint64_t geometryRevision=lighting.revision(),missingRevision=lighting.voxelLightingRevision(),vplFileRevision=lighting.files.revision();
 liveVpl(7);clockValue+=1001;lighting.update();
 assert(lighting.voxelLight.ready()&&"loaded native VPL must recover from an earlier missing-file fallback");
 assert(lighting.files.revision()==vplFileRevision&&lighting.voxelLightingRevision()!=missingRevision);checkVoxel(7);
 uint64_t liveRevision=lighting.voxelLightingRevision();fileVpl(2);reads=fileReads;
 clockValue+=1001;lighting.update();checkVoxel(7);assert(lighting.voxelLightingRevision()==liveRevision&&fileReads==reads);
 // A live mapping change alters lighting revision even with identical
 // geometry metadata, unit identity, heading, and normal index.
 liveVpl(3);clockValue+=1001;lighting.update();assert(lighting.voxelLightingRevision()!=liveRevision);checkVoxel(3);liveRevision=lighting.voxelLightingRevision();
 for(int i=0;i<32;++i){lighting.update();checkVoxel(3);}assert(lighting.voxelLightingRevision()==liveRevision&&fileReads==reads);
 // A transient invalid read keeps the last coherent table. A new match
 // clears that snapshot and can use the independent file fallback.
 word(memory,0xB2FB58,33);clockValue+=1001;lighting.update();assert(lighting.voxelLightingRevision()==liveRevision);checkVoxel(3);
 word(memory,0xB2FB58,0);lighting.identity(pid,3);checkVoxel(2);vplFileRevision=lighting.files.revision();
 liveVpl(7);clockValue+=1001;lighting.update();assert(lighting.files.revision()==vplFileRevision);checkVoxel(7);
 // A loose VPL can appear without any MIX stamp or directory revision.
 word(memory,0xB2FB58,0);disk.erase("VOXELS.VPL");lighting.identity(pid,4);assert(!lighting.voxelLight.ready());
 missingRevision=lighting.voxelLightingRevision();vplFileRevision=lighting.files.revision();fileVpl(2);clockValue+=1001;lighting.update();
 assert(lighting.files.revision()==vplFileRevision&&lighting.voxelLightingRevision()!=missingRevision);checkVoxel(2);reads=fileReads;
 clockValue+=1001;lighting.update();checkVoxel(2);assert(fileReads==reads);
 // Both sources already exist at attach time: use the engine table directly.
 liveVpl(7);lighting.identity(pid,5);checkVoxel(7);assert(fileReads==reads);
 // File discovery can be unavailable while native memory remains readable.
 // Its revision is then zero in both matches, so identity must clear VPL
 // independently before a new process/match can inherit an old snapshot.
 imagePathAvailable=false;lighting.identity(pid,6);assert(lighting.files.revision()==0);checkVoxel(7);
 word(memory,0xB2FB58,0);lighting.identity(pid,7);
 assert(!lighting.voxelLight.ready()&&"match identity must clear VPL even when file discovery has revision zero");
 liveVpl(3);clockValue+=1001;lighting.update();checkVoxel(3);
 word(memory,0xB2FB58,0);lighting.identity(pid+1,7);assert(!lighting.voxelLight.ready());imagePathAvailable=true;
 // Presentation capture is fresh on every pass, with no 1000ms delay;
 // changed VPL changes raster colors but never invalidates the source geometry.
 liveVpl(3);lighting.identity(pid+1,8);geometryRevision=lighting.revision();auto retainedModel=v.model.faces.data();
 lighting.beginPresentation();assert(gameassets::Context::liveVoxel(v));checkVoxel(3);
 liveVpl(7);lighting.beginPresentation();checkVoxel(7);assert(lighting.revision()==geometryRevision&&v.model.faces.data()==retainedModel);
 word(memory,0xB2FB58,0);lighting.beginPresentation();assert(!lighting.renderActor(v,{},image)&&lighting.failure=="voxel-current-vpl-unavailable");
 liveVpl(3);lighting.beginPresentation();checkVoxel(3);assert(lighting.revision()==geometryRevision);
 std::cout<<"PASS production VPL memory priority, legacy recovery, same-clock live presentation, geometry retention and unavailable-source rejection\n";
 // Bullet visuals are neither Techno health objects nor ordinary unit art.
 // Exercise production capture/render using live SHP, plain converters and
 // the real VXL/HVA decoder with synthetic memory (no private game art).
 constexpr uint32_t shot=0x700000,shotType=0x701000,shotShape=0x702000,shotConvert=0x704000,shotColors=0x705000,animShotConvert=0x706000,animShotColors=0x707000;
 word(memory,shot,external::projectiles::Vtable);word(memory,shot+0x10,9001);word(memory,shot+0xAC,shotType);word(memory,shotType,external::projectiles::TypeVtable);
 memory[shot+0x90]=1;word(memory,shot+0x94,2);word(memory,shot+0x9C,25600);word(memory,shot+0xA0,25600);word(memory,shot+0xA4,416);memory[shotType+0x2A1]=1;
 memcpy(memory.data()+shotType+0x24,"TESTPROJECTILE",15);word(memory,shotType+0xA4,shotShape);auto projectileShp=partShape(32,0,100);memcpy(memory.data()+shotShape,projectileShp.data(),projectileShp.size());
 auto plain=[&](uint32_t drawer,uint32_t palette,uint16_t color){word(memory,drawer,0x7E5358);word(memory,drawer+4,2);word(memory,drawer+0x16C,53);word(memory,drawer+0x174,palette);for(int i=0;i<256;++i)half(memory,palette+i*2,color);};
 plain(shotConvert,shotColors,0xF800);plain(animShotConvert,animShotColors,0x07E0);word(memory,0x87F6C4,shotConvert);word(memory,0x87F6C0,animShotConvert);memory[shotType+0x29A]=1;
 external::projectiles::State projectile;assert(external::projectiles::capture(gameassets::context.read,shot,projectile));
 word(memory,shot+0x6C,0);assert(gameassets::captureProjectile(shot,shotType,9001,art)&&art.kind==6&&!art.voxel&&art.flat&&art.colors[100]==0xffff0000);
 assert(gameassets::renderActor(art,projectile.pose,image)&&image.pixels[0]==0xffff0000&&image.attachments.size()==1&&image.attachments[0].shadow&&image.attachments[0].airborneShadow);
 assert(image.attachments[0].pixels==image.pixels&&"projectile shadows reuse this frame, not half-SHP shadow frames");
 // Plain converter middle RGB565 table also changes in place.
 auto projectileShape=art.shape.data();auto projectileGeometry=art.model.faces.size();auto projectileRevision=art.paletteRevision;
 half(memory,shotColors+100*2,0x001f);assert(gameassets::context.refreshActorPalette(art,paletteChanged)&&paletteChanged&&art.colors[100]==0xff0000ff);
 assert(art.shape.data()==projectileShape&&art.model.faces.size()==projectileGeometry&&art.paletteRevision==projectileRevision+1);
 assert(gameassets::renderActor(art,projectile.pose,image)&&image.pixels[0]==0xff0000ff);
 plain(shotConvert,shotColors,0xF800);
 memory[shotType+0x2A8]=1;assert(external::projectiles::capture(gameassets::context.read,shot,projectile)&&gameassets::captureProjectile(shot,shotType,9001,art));
 assert(gameassets::renderActor(art,projectile.pose,image)&&image.pixels[0]==0xff00ff00&&"AnimPalette must precede FirersPalette");
 // Missing live resource drawer falls back to PALETTE.PAL, not UNIT*.PAL.
 memory[shotType+0x2A8]=0;word(memory,0x87F6C4,0);disk["PALETTE.PAL"]=std::vector<uint8_t>(768,13);clockValue+=6000;gameassets::update();
 assert(gameassets::captureProjectile(shot,shotType,9001,art)&&art.colors[100]==0xff343434);word(memory,0x87F6C4,shotConvert);
 // Optional precision uses a known source PAL only when all nontransparent
 // entries reproduce the authoritative live RGB565 codes. It is never a
 // blanket RGB stretch or a substitute for a changed mod/runtime palette.
 {
  auto oldProjectilePal=disk["PALETTE.PAL"];auto oldMinePal=disk.find("UNITSNO.PAL");bool hadMinePal=oldMinePal!=disk.end();std::vector<uint8_t> oldMineBytes=hadMinePal?oldMinePal->second:std::vector<uint8_t>{};
  auto oldAnimPal=disk.find("ANIM.PAL");bool hadAnimPal=oldAnimPal!=disk.end();std::vector<uint8_t> oldAnimBytes=hadAnimPal?oldAnimPal->second:std::vector<uint8_t>{};
  std::vector<uint8_t> oldShot(memory.begin()+shotColors,memory.begin()+shotColors+512),oldMine(memory.begin()+minePalette,memory.begin()+minePalette+512),oldAnim(memory.begin()+animShotColors,memory.begin()+animShotColors+512);
  std::vector<uint8_t> precisionPal(768);for(int i=0;i<256;++i){precisionPal[i*3]=i%64;precisionPal[i*3+1]=(i*3)%64;precisionPal[i*3+2]=(i*7)%64;}
  std::array<uint32_t,256> sourceColors{},expandedColors{},recovered{};assert(external::assets::pal(precisionPal,sourceColors,external::pixelupgrade::PaletteRange::FullRGB));
  auto expand565=[](uint16_t c){unsigned r=(c>>11)&31,g=(c>>5)&63,b=c&31;return 0xff000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);};
  for(int i=0;i<256;++i){uint16_t code=external::paletterecovery::rgb565(sourceColors[i]);half(memory,shotColors+i*2,code);half(memory,minePalette+i*2,code);half(memory,animShotColors+i*2,code);expandedColors[i]=expand565(code);}expandedColors[0]=0;
  assert(sourceColors!=expandedColors);disk["PALETTE.PAL"]=disk["UNITSNO.PAL"]=disk["ANIM.PAL"]=precisionPal;word(memory,0x87F6BC,mineConvert);
  gameassets::Context precise(externalReader);precise.identity(pid,31);precise.presentationPalette(external::pixelupgrade::PaletteRange::FullRGB);
  assert(external::projectiles::capture(precise.read,shot,projectile));
  assert(precise.projectilePalette(projectile,recovered)&&recovered==expandedColors&&"precision defaults off");
  auto beforePrecision=precise.revision();precise.presentationColorPrecision(true);assert(precise.revision()!=beforePrecision);
  assert(precise.projectilePalette(projectile,recovered)&&recovered==sourceColors);
  assert(precise.spritePalette(mine,"OREMINE.TEM",false,0,recovered,true)&&recovered==sourceColors);
  auto rawState=projectile;rawState.converter=animConvert;std::array<uint32_t,256> rawColors{};assert(precise.convertPalette(animConvert,rawColors));
  assert(precise.projectilePalette(rawState,recovered)&&recovered==rawColors&&recovered[8]==0xff5a8cbe&&"already-live RGB888 is neither source-replaced nor stretched");
  gameassets::ActorArt preciseArt;assert(precise.captureProjectile(shot,shotType,9001,preciseArt)&&preciseArt.paletteLive&&preciseArt.colors==sourceColors);
  auto precisionShape=preciseArt.shape.data();auto precisionPaletteRevision=preciseArt.paletteRevision;
  uint16_t changedCode=external::paletterecovery::rgb565(sourceColors[174])^0x8000;half(memory,shotColors+174*2,changedCode);auto changedColors=expandedColors;changedColors[174]=expand565(changedCode);
  assert(precise.refreshActorPalette(preciseArt,paletteChanged)&&paletteChanged&&preciseArt.colors==changedColors&&preciseArt.paletteLive);
  assert(preciseArt.shape.data()==precisionShape&&preciseArt.paletteRevision==precisionPaletteRevision+1&&"a live palette mutation keeps source geometry");
  assert(precise.projectilePalette(projectile,recovered)&&recovered==changedColors&&"one mismatched entry rejects the entire source candidate");
  half(memory,shotColors+174*2,external::paletterecovery::rgb565(sourceColors[174]));
  assert(precise.refreshActorPalette(preciseArt,paletteChanged)&&paletteChanged&&preciseArt.colors==sourceColors);
  disk["PALETTE.PAL"][174*3]^=16;precise.identity(pid,33);assert(precise.projectilePalette(projectile,recovered)&&recovered==expandedColors&&"mismatched source file cannot replace live RGB565");
  disk["PALETTE.PAL"]=precisionPal;precise.identity(pid,34);
  memory[shotType+0x2A8]=1;assert(external::projectiles::capture(precise.read,shot,projectile));assert(precise.projectilePalette(projectile,recovered)&&recovered==sourceColors);memory[shotType+0x2A8]=0;
  precise.presentationPalette(external::pixelupgrade::PaletteRange::OriginalDAC);assert(external::projectiles::capture(precise.read,shot,projectile));
  assert(precise.projectilePalette(projectile,recovered)&&recovered==expandedColors&&"original DAC range never silently promotes precision");
  precise.presentationPalette(external::pixelupgrade::PaletteRange::FullRGB);disk.erase("PALETTE.PAL");precise.identity(pid,32);precise.failure="live-diagnostic";
  assert(precise.projectilePalette(projectile,recovered)&&recovered==expandedColors&&precise.failure=="live-diagnostic"&&"missing optional PAL preserves live data and diagnostics");
  disk["PALETTE.PAL"]=oldProjectilePal;if(hadMinePal)disk["UNITSNO.PAL"]=oldMineBytes;else disk.erase("UNITSNO.PAL");if(hadAnimPal)disk["ANIM.PAL"]=oldAnimBytes;else disk.erase("ANIM.PAL");
  memcpy(memory.data()+shotColors,oldShot.data(),512);memcpy(memory.data()+minePalette,oldMine.data(),512);memcpy(memory.data()+animShotColors,oldAnim.data(),512);word(memory,0x87F6BC,0);
 }
 std::cout<<"PASS optional source-PAL RGB565 precision, live mutation and missing/mismatched-source safety, source provenance and geometry retention\n";
 // Minimal one-voxel VXL and HVA: all faces share a real raw normal index.
 constexpr uint32_t bulletVxl=0x710000,bulletTail=0x711000,bulletBody=0x712000,bulletHva=0x713000,bulletMatrix=0x714000,bulletItems=0x715000,bulletScheme=0x716000,bulletDrawer=0x717000,bulletPalette=0x718000;
 word(memory,bulletVxl+4,1);word(memory,bulletVxl+8,1);word(memory,bulletVxl+12,13);word(memory,bulletVxl+20,bulletTail);word(memory,bulletVxl+24,bulletBody);
 word(memory,bulletTail,bulletBody);word(memory,bulletTail+4,bulletBody+4);word(memory,bulletTail+8,bulletBody+8);word(memory,bulletBody,0);word(memory,bulletBody+4,4);
 memory[bulletBody+8]=0;memory[bulletBody+9]=1;memory[bulletBody+10]=5;memory[bulletBody+11]=0;memory[bulletBody+12]=1;
 for(int i=0;i<8;++i){float bounds[3]={float((i&1)?8:0),float((i&2)?2:0),float((i&4)?2:0)};memcpy(memory.data()+bulletTail+0x40+i*12,bounds,sizeof bounds);}
 memory[bulletTail+0xA0]=memory[bulletTail+0xA1]=memory[bulletTail+0xA2]=1;memory[bulletTail+0xA3]=4;
 word(memory,bulletHva+4,1);word(memory,bulletHva+8,1);word(memory,bulletHva+12,bulletMatrix);float matrix[12]={1,0,0,0,0,1,0,0,0,0,1,0};memcpy(memory.data()+bulletMatrix,matrix,sizeof matrix);
 memory[shotType+0x236]=1;word(memory,shotType+0xB0,bulletVxl);word(memory,shotType+0xB4,bulletHva);word(memory,shotType+0x2D4,0);
 word(memory,0xB054D4,bulletItems);word(memory,0xB054E0,1);word(memory,bulletItems,bulletScheme);word(memory,bulletScheme+0x30C,bulletDrawer);word(memory,bulletDrawer,0x7ED0A4);word(memory,bulletDrawer+0x188,bulletPalette);
 for(int i=1;i<256;++i){memory[bulletPalette+i*3]=uint8_t(i);memory[bulletScheme+4+i*3]=uint8_t(i);}
 word(memory,0xB2FB58,32);for(int page=0;page<32;++page)std::fill(memory.begin()+0xB41178+page*256,memory.begin()+0xB41178+(page+1)*256,uint8_t(page+40));clockValue+=1001;gameassets::update();
 for(auto velocity:std::vector<std::array<double,3>>{{1,0,0},{1,2,3},{-2,1,-3}}){memcpy(memory.data()+shot+0xE8,velocity.data(),24);assert(external::projectiles::capture(gameassets::context.read,shot,projectile));
  assert(gameassets::captureProjectile(shot,shotType,9001,art)&&art.voxel&&art.model.faces.size()==6&&!art.shadow);
  auto nativeLight=external::voxel::nativeVoxelLight(external::visual::radians(projectile.pose.body),external::visual::radians(projectile.pose.turret));
  unsigned expected=external::voxel::nativeProjectilePage(external::voxel::Normals4[0],nativeLight)+40;
  assert(gameassets::renderActor(art,projectile.pose,image)&&!image.indices.empty()&&image.depth.size()==image.pixels.size());
  for(auto depth:image.depth)assert(std::isnan(depth));
  for(auto index:image.indices)assert(!index||index==expected);
  assert(std::any_of(image.pixels.begin(),image.pixels.end(),[](auto color){return color!=0;}));
  external::camera::Surface blocked;blocked.reset(64,64,0xff888888);std::fill(blocked.depth.begin(),blocked.depth.end(),1e20f);blocked.blit(image,32,32,1,1,0,{});
  assert(std::any_of(blocked.color.begin(),blocked.color.end(),[](auto color){return color!=0xff888888;})&&"non-flat VXL projectile does not read scene Z");
  for(auto depth:blocked.depth)assert(depth==1e20f);
 }
 memory[shotType+0x2F7]=1;assert(external::projectiles::capture(gameassets::context.read,shot,projectile)&&projectile.layer==1);
 assert(gameassets::captureProjectile(shot,shotType,9001,art)&&gameassets::renderActor(art,projectile.pose,image)&&image.flat&&image.depth.empty()&&!image.writeDepth);
 external::camera::Surface flatBlocked;flatBlocked.reset(64,64,0xff888888);std::fill(flatBlocked.depth.begin(),flatBlocked.depth.end(),1e20f);flatBlocked.blit(image,32,32,1,1,0,{});
 for(auto color:flatBlocked.color)assert(color==0xff888888&&"Flat VXL projectile retains the native read-only scene Z test");
 memory[shotType+0x2F7]=0;assert(external::projectiles::capture(gameassets::context.read,shot,projectile)&&projectile.layer==3);
 // Ordinary VXL/SHP ColorScheme raw RGB changes retain model geometry.
 gameassets::ActorArt ordinary;ordinary.kind=2;ordinary.type=shotType;ordinary.owner=house;ordinary.voxel=true;ordinary.model=v.model;
 word(memory,house+0x16054,0);assert(gameassets::context.spritePalette(shotType,"",false,house,ordinary.colors));ordinary.paletteLive=true;
 auto modelFaces=ordinary.model.faces.data();auto modelCount=ordinary.model.faces.size();
 memory[bulletScheme+4+43*3]=201;memory[bulletScheme+4+43*3+1]=77;memory[bulletScheme+4+43*3+2]=31;
 assert(gameassets::context.refreshActorPalette(ordinary,paletteChanged)&&paletteChanged&&ordinary.colors[43]==0xffc94d1f);
 assert(ordinary.model.faces.data()==modelFaces&&ordinary.model.faces.size()==modelCount);
 // Immutable prepared packets survive per-presentation image invalidation.
 // Every native VPL capture still runs before the cache lookup.
 assert(gameassets::captureProjectile(shot,shotType,9001,art)&&art.geometryIdentity);
 gameassets::context.presentationGPU(true);gameassets::context.beginPresentation();
 external::voxel::Image cachedGPU;assert(gameassets::renderActor(art,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster);auto firstPacket=cachedGPU.gpuRaster;auto firstBuilds=gameassets::context.gpuPreparedBuilds();
 for(int repeat=0;repeat<40;++repeat){gameassets::context.beginPresentation();assert(gameassets::renderActor(art,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster==firstPacket);}
 assert(gameassets::context.gpuPreparedBuilds()==firstBuilds&&gameassets::context.gpuPreparedHits()>=40&&gameassets::context.gpuPreparedCacheBytes()<=64*1024*1024);
 auto copiedArt=art;auto movedArt=std::move(copiedArt);assert(movedArt.geometryIdentity==art.geometryIdentity&&gameassets::renderActor(movedArt,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster==firstPacket);
 auto unitArt=art;unitArt.kind=2;unitArt.shadow=true;assert(gameassets::renderActor(unitArt,liveSlope,cachedGPU)&&cachedGPU.attachments.size()==1);
 auto bodyPacket=cachedGPU.gpuRaster,shadowPacket=cachedGPU.attachments[0].gpuRaster;auto unitBuilds=gameassets::context.gpuPreparedBuilds();
 for(int repeat=0;repeat<10;++repeat){gameassets::context.beginPresentation();assert(gameassets::renderActor(unitArt,liveSlope,cachedGPU)&&cachedGPU.gpuRaster==bodyPacket&&cachedGPU.attachments[0].gpuRaster==shadowPacket);}
 assert(gameassets::context.gpuPreparedBuilds()==unitBuilds);auto unitPosture=liveSlope;unitPosture.posture.forwards+=.02f;
 assert(gameassets::renderActor(unitArt,unitPosture,cachedGPU)&&cachedGPU.gpuRaster!=bodyPacket&&cachedGPU.attachments[0].gpuRaster==shadowPacket);
 auto unitShadow=liveSlope;unitShadow.shadow.posture.ramp=external::voxelposture::identity();unitShadow.shadow.posture.active=false;
 assert(gameassets::renderActor(unitArt,unitShadow,cachedGPU)&&cachedGPU.gpuRaster==bodyPacket&&cachedGPU.attachments[0].gpuRaster!=shadowPacket);
 auto originalColor=art.colors[1];art.colors[1]^=0x00010000;assert(gameassets::renderActor(art,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster!=firstPacket);art.colors[1]=originalColor;
 assert(gameassets::renderActor(art,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster==firstPacket);
 auto changedPose=projectile.pose;changedPose.body=(changedPose.body+1)&31;changedPose.bodyFine=-1;
 assert(gameassets::renderActor(art,changedPose,cachedGPU)&&cachedGPU.gpuRaster!=firstPacket);
 changedPose=projectile.pose;changedPose.posture.sideways+=.01f;assert(gameassets::renderActor(art,changedPose,cachedGPU)&&cachedGPU.gpuRaster!=firstPacket);
 gameassets::context.presentationDetail(1);assert(gameassets::context.presentationResolution(64,64,128,128));assert(gameassets::renderActor(art,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster!=firstPacket&&cachedGPU.rasterScale==2);gameassets::context.presentationDetail(0);
 auto previousVPL=gameassets::context.voxelLightingRevision();memory[0xB41178+17]^=1;gameassets::context.beginPresentation();
 assert(gameassets::context.voxelLightingRevision()!=previousVPL&&gameassets::renderActor(art,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster!=firstPacket);memory[0xB41178+17]^=1;gameassets::context.beginPresentation();
 auto previousGeometry=art.geometryIdentity;auto previousPacket=cachedGPU.gpuRaster;assert(gameassets::captureProjectile(shot,shotType,9001,art)&&art.geometryIdentity!=previousGeometry);
 assert(gameassets::renderActor(art,projectile.pose,cachedGPU)&&cachedGPU.gpuRaster!=previousPacket);
 gameassets::context.presentationGPU(false);
 std::cout<<"PASS production immutable GPU packet reuse across40 fresh VPL captures; exact palette/pose/posture/raster/VPL invalidation, capture nonce recapture and copy/move identity\n";
 // Sample each source once in a presentation, without resampling live INI
 // and palette memory for every identical instance. The next pass is fresh.
 {
  constexpr uint32_t owner=0x760000,type=0x770000,vector=0x751000,items=0x751100,scheme=0x752000;
  word(memory,type+0xDF0,vector);word(memory,owner+0x16054,0);word(memory,vector+4,items);word(memory,vector+16,1);word(memory,items,scheme);
  memset(memory.data()+scheme+4,31,768);gameassets::Context sharedPalette(externalReader);sharedPalette.identity(pid,101);sharedPalette.beginPresentation();
  std::array<uint32_t,256>first{},again{};assert(sharedPalette.spritePalette(type,"",false,owner,first));int readsAfterFirst=rawReads;
  for(int n=0;n<299;++n)assert(sharedPalette.spritePalette(type,"",false,owner,again)&&again==first);
  assert(rawReads==readsAfterFirst&&sharedPalette.paletteSamples()==1&&sharedPalette.paletteSampleHits()==299);
  memory[scheme+7]=63;sharedPalette.beginPresentation();assert(sharedPalette.spritePalette(type,"",false,owner,again)&&again!=first&&sharedPalette.paletteSamples()==1);
  std::cout<<"PASS production palette source sampled once for300 instances; 299 cache hits add zero remote reads, next presentation observes changed RGB\n";
 }
 // Missing VPL must not substitute the vehicle Lambert approximation for
 // the distinct projectile selector. Source readiness will trigger retry.
 word(memory,0xB2FB58,0);disk.erase("VOXELS.VPL");gameassets::context.identity(pid+4,99);
 assert(gameassets::captureProjectile(shot,shotType,9001,art)&&!gameassets::renderActor(art,projectile.pose,image)&&gameassets::failure=="projectile-vpl-unavailable");
 memory[shotType+0x29E]=1;assert(!gameassets::captureProjectile(shot,shotType,9001,art));
 std::cout<<"PASS live LightConvert/RGB565 palette in-place changes, stable-content reuse, unavailable-source rejection and retained geometry\n";
 std::cout<<"PASS production projectile SHP -> fixed-1000 plain/ANIM palette -> same-frame shadow, PALETTE.PAL fallback, VXL/HVA capture -> quantized yaw/pitch -> projectile diffuse VPL pixels\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'windows.h').write_text(header);(p/'test.cpp').write_text(body)
 exe=p/'test'
 subprocess.run(['g++','-std=c++17','-O2','-I'+str(p),'-I'+str(root),str(p/'test.cpp'),str(root/'source/third_party/xbrz/xbrz.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),*sys.argv[1:]],check=True)
