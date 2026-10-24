#pragma once
#include "core.h"
#include <vector>
#include <string>
namespace um {
using AssetLoad=bool(*)(const char*,std::vector<uint8_t>&);
template<class Read,class Resolve>bool readCameo(Read read,Resolve resolve,Type&t,uint8_t*buffer,size_t capacity,AssetLoad files=nullptr){
 if(files&&t.cameo[0]){std::vector<uint8_t> bytes;std::string n=t.cameo;if(n.find('.')==std::string::npos)n+=".shp";if(files(n.c_str(),bytes)&&decode(bytes.data(),bytes.size(),t.icon)){t.iconReady=1;t.iconCached=0;return true;}}
 auto get32=[&](uint32_t at){uint32_t v=0;read(at,&v,4);return v;};uint32_t p=get32(t.ptr+1776);if(!p)return false;uint16_t tag=0;if(!read(p,&tag,2))return false;
 if(tag==65535){uint32_t decoded=get32(p+12);if(!decoded)decoded=resolve(p);p=decoded;}if(!p)return false;
 uint8_t header[32];if(!read(p,header,32))return false;uint32_t offset=uint32_t(header[28])|(uint32_t(header[29])<<8)|(uint32_t(header[30])<<16)|(uint32_t(header[31])<<24);unsigned width=header[12]|(header[13]<<8),height=header[14]|(header[15]<<8),flags=header[16]&3;
 if(!width||!height||width>256||height>256||offset<32||offset>1024*1024||capacity<32)return false;memcpy(buffer,header,32);buffer[28]=32;buffer[29]=buffer[30]=buffer[31]=0;size_t used=32;uint32_t at=p+offset;
 for(unsigned y=0;y<height;++y){unsigned length=width;if(flags>=2){uint16_t row=0;if(!read(at,&row,2)||row<2||row>1024)return false;length=row;}if(used+length>capacity||!read(at,buffer+used,length))return false;at+=length;used+=length;}
 if(!decode(buffer,used,t.icon))return false;t.iconReady=1;t.iconCached=0;return true;
}
template<class Read>bool readCameoPalette(Read read,Snapshot&s,AssetLoad files=nullptr){
 // Building and statistics icons resolve the same MIX/loose-file palette.
 if(files){std::vector<uint8_t> colors;if(files("cameo.pal",colors)&&colors.size()==768){bool valid=true;for(auto c:colors)valid&=c<=63;if(valid){for(int i=0;i<256;++i)s.palette[i]=0xff000000u|(uint32_t(colors[i*3])*4<<16)|(uint32_t(colors[i*3+1])*4<<8)|uint32_t(colors[i*3+2])*4;s.palette[0]=0;s.paletteReady=1;return true;}}}

 uint32_t conv=0,mid=0;int mode=0;uint16_t palette[256];if(!read(0x87F6B0,&conv,4)||!conv||!read(conv+4,&mode,4)||mode!=2||!read(conv+0x174,&mid,4)||!mid||!read(mid,palette,sizeof palette))return false;
 for(int i=0;i<256;++i){unsigned c=palette[i],r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;s.palette[i]=0xFF000000u|(r<<16)|(g<<8)|b;}s.palette[0]=0;s.paletteReady=1;return true;
}
}
