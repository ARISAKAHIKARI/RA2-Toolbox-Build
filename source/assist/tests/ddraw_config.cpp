#include "../../cursor/source/ddraw_config.h"
#include <cassert>
#include <iostream>
using namespace ddrawconfig;
int main(){
 std::string source="\xef\xbb\xbf;中文 / comments\r\n[ddraw]\r\nmaxfps=-1\r\nmaxgameticks=60\r\nvsync=true\r\n[gamemd]\r\nmaxfps=60 ; limit\r\nminfps=-1\r\ntshack=true\r\n[gamemd/2]\r\ncheckfile=mod.dll\r\nmaxfps=60\r\n[gamemd/wine]\r\nminfps=-1\r\n[other]\r\nmaxfps=30\r\n";
 for(unsigned rate:{0u,30u,60u,120u,144u,165u,240u,360u,500u}){
  auto e=prepare(source,Kind::Cnc,"GaMeMd",rate);assert(e.error.empty()&&e.sections==3);
  assert(e.result.find("maxfps="+std::to_string(rate?int(rate):-1)+" ; limit")!=e.result.npos);
  assert(e.result.find("maxgameticks=60\r\nvsync=true")!=e.result.npos&&e.result.find("tshack=true")!=e.result.npos&&e.result.find("checkfile=mod.dll")!=e.result.npos&&e.result.find("[other]\r\nmaxfps=30")!=e.result.npos);
  assert(prepare(e.result,Kind::Cnc,"gamemd",rate).result==e.result);
 }
 auto added=prepare("[ddraw]\nmaxfps=60",Kind::Cnc,"gamemd-spawn",144);assert(added.result=="[ddraw]\nmaxfps=60\n\n[gamemd-spawn]\nmaxfps=144\nminfps=-2\n");
 auto duplicate=prepare("[GAMEMD]\nMAXFPS=60\nmaxfps=30\nminfps=-1",Kind::Cnc,"gamemd",120);assert(duplicate.result=="[GAMEMD]\nMAXFPS=120\nmaxfps=120\nminfps=-2");
 auto ts=prepare("[ddraw]\nTargetFPS=60\nVSync=no\nRenderer=opengl\n",Kind::TS,"gamemd",0);assert(ts.result=="[ddraw]\nTargetFPS=0\nVSync=no\nRenderer=opengl\n");
 for(auto k:{Kind::Cnc,Kind::TS}){assert(!prepare(std::string("a\0b",3),k,"gamemd",60).error.empty());assert(!prepare("[broken",k,"gamemd",60).error.empty());assert(!prepare("[ddraw]",k,"gamemd",501).error.empty());}
 assert(!prepare(source,Kind::Unknown,"gamemd",144).error.empty());
 // Loaded-image fixture, including malformed header/name bounds. No DLL load.
 std::vector<uint8_t> pe(8192);auto word=[&](int at,uint32_t v){memcpy(pe.data()+at,&v,4);};
 word(0,0x5a4d);word(0x3c,128);word(128,0x4550);word(152,0x10b);word(248,512);word(252,512);word(536,3);word(544,640);
 const char*cnc[]={"DDEnableZoom","DDGetProcAddress","pvBmpBits"};for(int n=0;n<3;++n){word(640+4*n,1024+128*n);strcpy(reinterpret_cast<char*>(pe.data()+1024+128*n),cnc[n]);}
 auto read=[&](uint32_t at,void*out,size_t n){if(at<0x10000||uint64_t(at-0x10000)+n>pe.size())return false;memcpy(out,pe.data()+at-0x10000,n);return true;};
 assert(identify(read,0x10000,pe.size())==Kind::Cnc);word(536,5000);assert(identify(read,0x10000,pe.size())==Kind::Unknown);word(536,2);
 strcpy(reinterpret_cast<char*>(pe.data()+1024),"TSDDRAW");strcpy(reinterpret_cast<char*>(pe.data()+1152),"TargetFPS");assert(identify(read,0x10000,pe.size())==Kind::TS);
 word(544,8190);assert(identify(read,0x10000,pe.size())==Kind::Unknown);assert(identify(read,0xfffff000,8192)==Kind::Unknown);
 std::cout<<"PASS CnC/TS export detection, per-game and variant overrides, 9 rates, unchanged game ticks/vsync/compatibility, BOM/comments, duplicate keys, idempotence and malformed input\n";
}
