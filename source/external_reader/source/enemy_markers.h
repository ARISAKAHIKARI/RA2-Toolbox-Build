#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
// Host display data only. Never part of Shared, a command, or a game write.
namespace external { namespace enemies {
constexpr int Maximum=2048;
template<class Read>bool special(Read read,uint32_t p,uint32_t t,int kind){
 auto get=[&](uint32_t at,auto&v){return read(at,&v,sizeof v);};
 uint8_t hidden=0,disguised=0,infiltrate=0;int cloak=0;
 if(kind<0||kind>2||!get(t+0xC9A,hidden)||hidden)return false;
 if(get(p+0x220,cloak)&&cloak>0)return true;
 if((kind==1||kind==2)&&get(p+0x1D8,disguised)&&disguised)return true;
 if(kind==1&&get(t+0xEBE,infiltrate)&&infiltrate)return true;
 char id[5]{};return kind==2&&read(t+0x24,id,sizeof id)&&id[0]=='M'&&id[1]=='G'&&id[2]=='T'&&id[3]=='K'&&!id[4];
}
struct Marker {
 uint32_t object=0,id=0,type=0,owner=0,color=0;
 int x=0,y=0,kind=-1,player=-1;
 int worldX=INT32_MIN,worldY=INT32_MIN,quantity=1;
 uint16_t name[40]{};
};
struct Frame {
 uint32_t match=0,tick=0,house=0;int frame=-1,count=0,omitted=0,width=0,height=0;
 Marker items[Maximum]{};
};
inline void copyFrame(Frame&to,const Frame&from){
 if(&to==&from)return;
 std::memcpy(&to,&from,offsetof(Frame,items));
 if(from.count>=0&&from.count<=Maximum)std::memcpy(to.items,from.items,size_t(from.count)*sizeof(Marker));
}
inline bool fresh(const Frame&f,uint32_t match,uint32_t now){
 return f.match==match&&f.tick&&uint32_t(now-f.tick)<250&&f.count>=0&&f.count<=Maximum;
}
inline bool onScreen(float x,float y,int width,int height){return x>=0&&y>=0&&x<width&&y<height;}
template<class Read> auto latest(Read&read,uint32_t at,void*out,size_t n,int)->decltype(read.fresh(at,out,n)){return read.fresh(at,out,n);}
template<class Read> bool latest(Read&read,uint32_t at,void*out,size_t n,long){return read(at,out,n);}
}}
