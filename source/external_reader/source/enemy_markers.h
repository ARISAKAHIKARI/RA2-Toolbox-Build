#pragma once
#include <stdint.h>
#include <stddef.h>
// Host display data only. Never part of Shared, a command, or a game write.
namespace external { namespace enemies {
constexpr int Maximum=2048;
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
inline bool fresh(const Frame&f,uint32_t match,uint32_t now){
 return f.match==match&&f.tick&&uint32_t(now-f.tick)<250&&f.count>=0&&f.count<=Maximum;
}
inline bool onScreen(float x,float y,int width,int height){return x>=0&&y>=0&&x<width&&y<height;}
template<class Read> auto latest(Read&read,uint32_t at,void*out,size_t n,int)->decltype(read.fresh(at,out,n)){return read.fresh(at,out,n);}
template<class Read> bool latest(Read&read,uint32_t at,void*out,size_t n,long){return read(at,out,n);}
}}
