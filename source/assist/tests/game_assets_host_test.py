"""Production Files -> nested MIX -> host art capture -> SHP/TMP pixels.
Only Win32 file/memory boundaries are synthetic; no Windows visual assertion.
"""
from pathlib import Path
import subprocess,tempfile
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
DWORD GetTickCount();bool GetFileAttributesExW(const wchar_t*,unsigned,WIN32_FILE_ATTRIBUTE_DATA*);
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
#include "source/external_reader/source/game_archive.h"
static std::map<std::string,std::vector<uint8_t>> disk;
static std::vector<uint8_t> memory(0x1000000);
static DWORD clockValue=1000,pid=1;static uint32_t commandMatch(){return 1;}
static struct Reader {
 bool imagePath(wchar_t*p,DWORD*n){std::wcscpy(p,L"C:\\Game\\gamemd.exe");*n=std::wcslen(p);return true;}
 bool readFresh(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}
}externalReader;
struct OpenFile {const std::vector<uint8_t>*data;size_t pos;};
static std::string filename(const wchar_t*p){std::wstring w=p;auto i=w.find_last_of(L"\\/");w=w.substr(i+1);return external::assets::name(std::string(w.begin(),w.end()));}
DWORD GetTickCount(){return clockValue;}
bool GetFileAttributesExW(const wchar_t*p,unsigned,WIN32_FILE_ATTRIBUTE_DATA*out){auto i=disk.find(filename(p));if(i==disk.end())return false;*out={};out->nFileSizeLow=i->second.size();return true;}
HANDLE CreateFileW(const wchar_t*p,unsigned,unsigned,void*,unsigned,unsigned,void*){auto i=disk.find(filename(p));return i==disk.end()?INVALID_HANDLE_VALUE:new OpenFile{&i->second,0};}
bool SetFilePointerEx(HANDLE h,LARGE_INTEGER n,LARGE_INTEGER*,unsigned){if(n.QuadPart<0)return false;static_cast<OpenFile*>(h)->pos=n.QuadPart;return true;}
bool ReadFile(HANDLE h,void*out,DWORD n,DWORD*got,void*){auto&f=*static_cast<OpenFile*>(h);if(f.pos+n>f.data->size()){*got=0;return false;}memcpy(out,f.data->data()+f.pos,n);f.pos+=n;*got=n;return true;}
bool CloseHandle(HANDLE h){delete static_cast<OpenFile*>(h);return true;}
struct Search {std::vector<std::string> names;size_t at=0;};
bool FindNextFileW(HANDLE h,WIN32_FIND_DATAW*out){auto&s=*static_cast<Search*>(h);if(s.at==s.names.size())return false;auto&n=s.names[s.at++];*out={};std::wstring w(n.begin(),n.end());std::wcscpy(out->cFileName,w.c_str());out->nFileSizeLow=disk.at(n).size();return true;}
HANDLE FindFirstFileW(const wchar_t*,WIN32_FIND_DATAW*out){auto*s=new Search;for(auto&[n,b]:disk)if(n.size()>=4&&n.substr(n.size()-4)==".MIX")s->names.push_back(n);if(FindNextFileW(s,out))return s;delete s;return INVALID_HANDLE_VALUE;}
bool FindClose(HANDLE h){delete static_cast<Search*>(h);return true;}
#include "source/assist/source/game_assets_host.h"
static void word(std::vector<uint8_t>&b,size_t at,uint32_t v){memcpy(b.data()+at,&v,4);}
static void half(std::vector<uint8_t>&b,size_t at,uint16_t v){memcpy(b.data()+at,&v,2);}
static std::vector<uint8_t> mix(const std::map<std::string,std::vector<uint8_t>>&files){
 size_t head=10+12*files.size(),size=0;for(auto&[n,b]:files)size+=b.size();std::vector<uint8_t> out(head+size);half(out,4,files.size());word(out,6,size);size_t slot=10,at=0;
 for(auto&[n,b]:files){word(out,slot,external::assets::hash(n));word(out,slot+4,at);word(out,slot+8,b.size());memcpy(out.data()+head+at,b.data(),b.size());slot+=12;at+=b.size();}return out;
}
int main(){
 std::vector<uint8_t> shp(8+24*24+24*14);half(shp,2,4);half(shp,4,3);half(shp,6,24);
 for(int f=0;f<24;++f){int at=8+f*24,data=8+24*24+f*14;half(shp,at+4,4);half(shp,at+6,3);word(shp,at+8,2);word(shp,at+20,data);half(shp,data,6);std::fill(shp.begin()+data+2,shp.begin()+data+14,3+f);}
 std::vector<uint8_t> pal(768,32),tmp(20+52+1800,3);word(tmp,0,1);word(tmp,4,1);word(tmp,8,60);word(tmp,12,30);word(tmp,16,20);std::fill(tmp.begin()+20,tmp.begin()+72,0);word(tmp,32,952);
 disk["RA2MD.MIX"]=mix({{"LOCALMD.MIX",mix({{"UNITTEM.PAL",pal},{"ISOTEM.PAL",pal},{"LONGINFANTRY.SHP",shp}})},{"ISOTEMMD.MIX",mix({{"CLEAR01.TEM",tmp}})}});
 // Incomplete higher-priority subarchive must not hide the base E1 resource.
 disk["EXPANDMD99.MIX"]=mix({{"LOCALMD.MIX",mix({{"UNRELATED.SHP",{0}}})}});
 // Real direction LUT order and distinct palette entries make facing test visible.
 for(int i=1;i<256;++i){pal[i*3]=i%64;pal[i*3+1]=(i*2)%64;pal[i*3+2]=(i*3)%64;}
 std::vector<uint8_t> customPal(768,12),theaterPal(768,22);for(int i=1;i<256;++i){customPal[i*3]=i%64;customPal[i*3+1]=(i*2)%64;customPal[i*3+2]=(i*3)%64;}
 std::string artText="[LONGINFANTRY]\nPalette=custom\n[NAWALL]\nNewTheater=yes\n";
 std::string rulesText="[ORE]\nTiberium=yes\n";
 disk["ECACHEMD98.MIX"]=mix({{"ARTMD.INI",{artText.begin(),artText.end()}},{"RULESMD.INI",{rulesText.begin(),rulesText.end()}},{"CUSTOMTEM.PAL",customPal},{"TEMPERAT.PAL",theaterPal},{"NTWALL.SHP",shp},{"ORE.SHP",shp}});
 constexpr uint32_t p=0x10000,t=0x20000,house=0x30000,tileType=0x40000,sequence=0x50000;
 word(memory,p,0x7EB058);word(memory,p+0x10,42);word(memory,p+0x6C0,t);word(memory,p+0x21C,house);memcpy(memory.data()+t+0x24,"E1",3);memcpy(memory.data()+t+0x1F8,"LONGINFANTRY",13);
 memcpy(memory.data()+tileType+0x2F5,"CLEAR01.TEM",12);memcpy(memory.data()+0x7E1B78+68,"ISOTEM",7);memcpy(memory.data()+0x7E1B78+78,"TEM",4);memcpy(memory.data()+0x7E1B78+48,"TEMPERAT",9);memory[0x7E1B78+86]='T';
 word(memory,t+0xE3C,sequence);word(memory,sequence+4,1);word(memory,sequence+8,1);const int dirs[32]={7,7,6,6,6,6,5,5,5,5,4,4,4,4,3,3,3,3,2,2,2,2,1,1,1,1,0,0,0,0,7,7};for(int i=0;i<32;++i)word(memory,0x7EAEFC+i*4,dirs[i]);word(memory,sequence+36,8);word(memory,sequence+40,2);word(memory,sequence+44,2);
 gameassets::update();assert(gameassets::files.count()>=5);
 gameassets::ActorArt art;assert(gameassets::captureActor(p,t,42,art));assert(!art.voxel&&!art.shape.empty());assert(art.colors[3]==0xff0c1824u);
 external::GameView game(&gameassets::read);external::voxel::Image image;
 for(int heading=0;heading<32;heading+=4){external::visual::Pose pose;pose.frame=external::visual::previewFrame(game,t,1,heading);assert(gameassets::renderActor(art,pose,image));assert(image.width==4&&image.height==3&&std::count_if(image.pixels.begin(),image.pixels.end(),[](auto c){return c!=0;})==12);assert(pose.frame==dirs[(8-heading)&31]);assert(image.pixels[0]==external::voxel::premultiply(art.colors[3+pose.frame],1));}
 // Walk sequence: two frames per facing, distinct indexed colors at each frame.
 word(memory,p+0x6C4,1);for(int body=0;body<32;body+=4)for(int stage=0;stage<2;++stage){half(memory,p+0x388,uint16_t(body*2048));word(memory,p+0xF8,stage);auto pose=external::visual::pose(game,p,t,1,0);assert(pose.frame==8+dirs[body]*2+stage);assert(gameassets::renderActor(art,pose,image));assert(image.pixels[0]==external::voxel::premultiply(art.colors[3+pose.frame],1));}
 std::array<uint32_t,256> colors{};assert(gameassets::palette(true,colors));assert(gameassets::tile(tileType,0,colors,image));assert(std::count_if(image.pixels.begin(),image.pixels.end(),[](auto c){return c!=0;})==900);
 // NewTheater overlay names and resource-specific palettes follow FA2 loading.
 constexpr uint32_t wall=0x60000,ore=0x70000;memcpy(memory.data()+wall+0x24,"NAWALL",7);memcpy(memory.data()+ore+0x24,"ORE",4);
 assert(gameassets::overlay(wall,0,0,image));assert(image.pixels[0]!=0);
 assert(gameassets::overlay(ore,0,0,image));assert(image.pixels[0]==external::voxel::premultiply(0xff585858u,1));
 // An all-black uninitialized live scheme must not suppress disk palette fallback.
 word(memory,house+0x16054,0);word(memory,0xB054D0+4,0x80000);word(memory,0xB054D0+16,1);word(memory,0x80000,0x90000);
 assert(!external::voxel::palette(gameassets::read,t,house,colors));
 // No selected bit, memory SHP, VXL, palette or live MIX index was supplied.
 assert(memory[p+0x83]==0);disk.clear();clockValue+=6000;gameassets::update();assert(!gameassets::tile(tileType,0,colors,image));assert(gameassets::failure=="tile-not-found:CLEAR01.TEM");
 std::cout<<"PASS production file discovery, nested base/override fallback, unloaded unselected infantry at eight headings, TMP pixels and missing-resource diagnostic\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'windows.h').write_text(header);(p/'test.cpp').write_text(body)
 exe=p/'test'
 subprocess.run(['g++','-std=c++17','-O2','-I'+str(p),'-I'+str(root),str(p/'test.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
