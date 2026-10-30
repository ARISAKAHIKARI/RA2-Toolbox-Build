#include "../../external_reader/source/feedback_cursor.h"
#include <map>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <functional>
using namespace external::nativefeedback;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)){ std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
struct Memory {
 std::map<uint32_t,uint8_t> bytes;size_t calls=0;std::function<void(uint32_t,size_t)> hook;
 template<class T>void set(uint32_t p,const T&v){const auto*b=reinterpret_cast<const uint8_t*>(&v);for(size_t n=0;n<sizeof v;++n)bytes[p+uint32_t(n)]=b[n];}
 void zero(uint32_t p,size_t n){for(size_t i=0;i<n;++i)bytes[p+uint32_t(i)]=0;}
 bool read(uint32_t p,void*out,size_t n){++calls;if(hook)hook(p,n);if(uint64_t(p)+n>0x100000000ull)return false;auto*b=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i){auto it=bytes.find(p+uint32_t(i));if(it==bytes.end())return false;b[i]=it->second;}return true;}
};
static Memory mouseMemory(){
 Memory m;uint32_t p=0x100000,vt=0x200000,shape=0x300000,conv=0x400000,row=0x500000;
 m.set(MousePointer,p);m.zero(p,0x44);m.set(p,vt);m.set(p+4,shape);m.set(p+8,int32_t(0));m.set(p+12,int32_t(0));m.set(p+0x3C,int32_t(11));m.set(p+0x40,int32_t(12));
 m.set(vt,std::array<uint32_t,5>{{0,0x7B8A00,0,0x7B9930,0x7B9750}});
 m.zero(shape,56);m.set(shape+2,uint16_t(55));m.set(shape+4,uint16_t(43));m.set(shape+6,uint16_t(2));
 m.set(shape+8,int16_t(3));m.set(shape+10,int16_t(4));m.set(shape+12,uint16_t(4));m.set(shape+14,uint16_t(3));
 m.set(shape+32,int16_t(5));m.set(shape+34,int16_t(6));m.set(shape+36,uint16_t(4));m.set(shape+38,uint16_t(3));
 m.set(0x87F6C8,conv);m.set(conv,uint32_t(0x7E5358));m.set(conv+4,uint32_t(2));m.set(conv+0x16C,int(1));m.set(conv+0x174,row);
 std::array<uint16_t,256> colors{};colors[1]=0xffff;colors[2]=0xf800;m.set(row,colors);m.set(0x8A0DD0,std::array<int,6>{{11,3,0,3,5,2}});return m;
}
static void bands(){
 Memory m;uint32_t tactical=0x100000;m.set(Map+0x11CF,uint8_t(1));m.set(tactical+0xD90,std::array<int,4>{{0,0,100,40}});
 auto read=[&](uint32_t p,void*out,size_t n){return m.read(p,out,n);};auto original=m.bytes;
 Band b;CHECK(band(read,tactical,b)&&b.active);CHECK(m.bytes==original);
 auto p=bandStrips(b,{10,20,400,200},2,2);CHECK(p.size()==4);CHECK(p[0].x==10&&p[0].y==20&&p[0].w==202);
 b.edges={100,40,0,0};auto reversed=bandStrips(b,{10,20,400,200},2,2);CHECK(reversed.size()==p.size());
 for(size_t i=0;i<p.size();++i)CHECK(p[i].x==reversed[i].x&&p[i].y==reversed[i].y&&p[i].w==reversed[i].w&&p[i].h==reversed[i].h);
 CHECK(bandStrips(b,{0,0,20,20},0,1).empty());CHECK(bandStrips(b,{0,0,20,20},NAN,1).empty());
 b.edges={INT32_MIN,0,INT32_MAX,40};CHECK(bandStrips(b,{0,0,20,20},1,1).empty());
 m.set(Map+0x11CF,uint8_t(0));CHECK(band(read,tactical,b)&&!b.active);m.set(Map+0x11CF,uint8_t(2));CHECK(!band(read,tactical,b));
 m.set(Map+0x11CF,uint8_t(1));unsigned count=0;m.hook=[&](uint32_t p,size_t){if(p==tactical+0xD90&&++count==2)m.set(p,int32_t(42));};CHECK(!band(read,tactical,b));
 CHECK(!band(read,0xfffffff0,b));
}
static void cursors(){
 auto m=mouseMemory();auto read=[&](uint32_t p,void*out,size_t n){return m.read(p,out,n);};auto original=m.bytes;
 Mouse sample;CHECK(mouse(read,sample));CHECK(sample.width==55&&sample.hotX==11&&sample.hotY==12);CHECK(m.bytes==original);
 std::array<uint32_t,256>pal{};CHECK(mousePalette(read,sample.converter,pal));CHECK(pal[0]==0&&pal[1]==0xfff8fcf8&&pal[2]==0xfff80000);
 CursorCache cache;CursorFrame frame;unsigned decodes=0;
 auto decode=[&](const Mouse&src,const std::array<uint32_t,256>&,CursorImage&out){++decodes;out={4,3,int16_t(u16(src.frameHeader.data())),int16_t(u16(src.frameHeader.data()+2)),std::vector<uint32_t>(12,0xffffffff)};return true;};
 CHECK(cache.capture(read,decode,frame)&&decodes==1);auto pixels=frame.image;
 CHECK(cache.capture(read,decode,frame)&&decodes==1&&pixels==frame.image);
 m.set(0x100008,int32_t(1));CHECK(cache.capture(read,decode,frame)&&decodes==2&&frame.mouse.frame==1);
 m.set(0x100008,int32_t(2));CHECK(!cache.capture(read,decode,frame)&&!frame.image);
 m.set(0x100008,int32_t(0));CHECK(cache.capture(read,decode,frame)&&decodes==3);
 m.set(0x10003C,int32_t(55));CHECK(!mouse(read,sample));m.set(0x10003C,int32_t(11));
 auto badDecode=[](const Mouse&,const std::array<uint32_t,256>&,CursorImage&out){out={4,3,3,4,std::vector<uint32_t>(12,0x00ffffff)};return true;};cache.clear();CHECK(!cache.capture(read,badDecode,frame));
 m.set(0x8A0DD0,std::array<int,6>{{10,3,0,3,5,3}});CHECK(mousePalette(read,0x400000,pal));
 m.set(0x8A0DD0,std::array<int,6>{{1000,3,0,3,5,3}});CHECK(!mousePalette(read,0x400000,pal));
 m=mouseMemory();unsigned count=0;m.hook=[&](uint32_t p,size_t){if(p==MousePointer&&++count==2)m.set(0x100008,int32_t(1));};CHECK(!mouse(read,sample));
 m=mouseMemory();m.set(0x100004,uint32_t(0xfffffff8));CHECK(!mouse(read,sample));
 sample.refCount=0;CHECK(cursorVisible(true,false,false,sample));CHECK(!cursorVisible(false,false,false,sample));CHECK(!cursorVisible(true,true,false,sample));CHECK(!cursorVisible(true,false,true,sample));sample.refCount=-1;CHECK(!cursorVisible(true,false,false,sample));
 auto rect=cursorRect(100,100,3,4,4,3,11,12,2,2);CHECK(rect.x==84&&rect.y==84&&rect.w==8&&rect.h==6);
 CHECK(!cursorRect(100,100,3,4,4,3,11,12,NAN,1).valid());
 auto clip=intersect(cursorRect(319,199,0,0,55,43,0,0,1,1),{0,0,320,200});CHECK(clip.x==319&&clip.y==199&&clip.w==1&&clip.h==1);
}
static void selections(){
 Memory m;uint32_t p=0x100000,type=0x200000,owner=0x300000,ptr=0x400000;
 m.set(0xA8ECB8,std::array<uint32_t,6>{{0,ptr,8,0,1,0}});m.set(ptr,p);
 m.set(p+0x10,uint32_t(7));m.set(p+0x21C,owner);m.set(p+0x80,std::array<uint8_t,4>{{0,0,0,1}});m.set(p+0x90,uint8_t(1));m.set(p+0x6C,int(25));m.set(type+0xA0,int(100));
 auto read=[&](uint32_t p,void*out,size_t n){return m.read(p,out,n);};auto original=m.bytes;
 std::vector<Object> objects={{p,7,type,owner,{30,40,50,30}}};std::vector<Selected> out;
 CHECK(selected(read,objects,out)&&out.size()==1&&out[0].health==25);CHECK(m.bytes==original);
 Band b;auto primitives=selectionPrimitives(out,b,{0,0,320,200},1,1);CHECK(primitives.size()==10);
 CHECK(primitives.back().color==0xffff4040);Canvas canvas;Rect bounds;CHECK(paint(primitives,canvas,bounds));CHECK(bounds.x==29&&bounds.y==34);
 auto full=selectionPrimitives({{{0,0,40,30},100,100}},b,{0,0,20,20},1,1);for(auto q:full){CHECK(q.rect.x>=0&&q.rect.y>=0&&q.rect.x+q.rect.w<=20&&q.rect.y+q.rect.h<=20);}
 std::array<uint32_t,600> manySelected{};manySelected[0]=p;for(unsigned n=1;n<manySelected.size();++n)manySelected[n]=0x300000+n*0x100;
 m.set(ptr,manySelected);m.set(0xA8ECB8,std::array<uint32_t,6>{{0,ptr,600,0,600,0}});CHECK(selected(read,objects,out)&&out.size()==1);
 m.set(ptr,p);m.set(0xA8ECB8,std::array<uint32_t,6>{{0,ptr,1,0,1,0}});
 objects[0].id=8;CHECK(!selected(read,objects,out)&&out.empty());objects[0].id=7;
 m.set(p+0x83,uint8_t(0));CHECK(!selected(read,objects,out));m.set(p+0x83,uint8_t(1));
 unsigned count=0;m.hook=[&](uint32_t at,size_t){if(at==0xA8ECB8&&++count==2)m.set(at+16,uint32_t(0));};CHECK(!selected(read,objects,out));m.hook={};
 m.set(0xA8ECB8,std::array<uint32_t,6>{{0,0,0,0,0,0}});CHECK(selected(read,objects,out)&&out.empty());
 m.set(0xA8ECB8,std::array<uint32_t,6>{{0,ptr,uint32_t(MaxBolts+1),0,uint32_t(MaxBolts+1),0}});CHECK(!selected(read,objects,out));
}
static void effects(){
 Memory m;uint32_t p=0x100000,ptr=0x200000;m.set(Bolts,std::array<uint32_t,6>{{0,ptr,1,0,1,0}});m.set(ptr,p);m.zero(p,0x30);
 m.set(p,Point3{1000,2000,0});m.set(p+12,Point3{2000,3000,20});m.set(p+0x1C,uint32_t(42));m.set(p+0x28,uint32_t(8));m.set(p+0x2C,uint8_t(1));
 auto read=[&](uint32_t p,void*out,size_t n){return m.read(p,out,n);};auto original=m.bytes;std::vector<Bolt> list;
 CHECK(bolts(read,list)&&list.size()==1&&list[0].seed==42&&list[0].alternate);CHECK(m.bytes==original);
 // More than 128 native objects remain a valid complete snapshot.
 std::array<uint32_t,600> many{};
 for(unsigned n=0;n<many.size();++n){many[n]=0x300000+n*0x100;m.zero(many[n],0x30);m.set(many[n],Point3{1000,2000,0});m.set(many[n]+12,Point3{2000,3000,20});m.set(many[n]+0x28,uint32_t(8));}
 m.set(ptr,many);m.set(Bolts,std::array<uint32_t,6>{{0,ptr,600,0,600,0}});CHECK(bolts(read,list)&&list.size()==600);
 m.set(ptr,p);m.set(Bolts,std::array<uint32_t,6>{{0,ptr,1,0,1,0}});
 m.set(p+0x28,uint32_t(0));CHECK(bolts(read,list)&&list.empty());m.set(p+0x28,uint32_t(8));
 unsigned count=0;m.hook=[&](uint32_t at,size_t n){if(at==p&&n==0x30&&++count==2)m.set(p+0x28,uint32_t(4));};CHECK(!bolts(read,list));m.hook={};
 m.set(Bolts,std::array<uint32_t,6>{{0,ptr,uint32_t(MaxBolts+1),0,uint32_t(MaxBolts+1),0}});CHECK(!bolts(read,list));
 Canvas a,b;CHECK(a.reset(128,64)&&b.reset(128,64));bolt(a,-100,32,228,32,42,false,1);bolt(b,-100,32,228,32,42,false,1);CHECK(a.pixels==b.pixels);
 CHECK(std::any_of(a.pixels.begin(),a.pixels.end(),[](uint32_t p){return p!=0;}));
 auto before=a.pixels;bolt(a,NAN,0,1,1,0,false,1);CHECK(a.pixels==before);
 std::vector<uint8_t>mask(a.pixels.size(),0);CHECK(applyVisibility(a,&mask));CHECK(std::all_of(a.pixels.begin(),a.pixels.end(),[](uint32_t p){return p==0;}));
 CHECK(a.reset(2,1));a.pixels={0x80800000,0xffffffff};mask={64,127};CHECK(applyVisibility(a,&mask));CHECK(a.pixels[0]==0x41410000&&a.pixels[1]==0xffffffff);
 mask={2,2};CHECK(applyVisibility(a,&mask));CHECK(a.pixels[0]==0&&a.pixels[1]==0);mask={127};CHECK(!applyVisibility(a,&mask));
 CHECK(!a.reset(9000,9000)&&a.pixels.empty()&&a.width==0&&a.height==0);
}
static void randomized(){
 std::mt19937 rng(20261028);Canvas c;CHECK(c.reset(64,48));
 for(int n=0;n<3000;++n){
  int x=int(rng()%1000)-500,y=int(rng()%1000)-500,w=int(rng()%600)+1,h=int(rng()%600)+1;
  auto r=intersect({x,y,w,h},{0,0,64,48});if(r.valid()){CHECK(r.x>=0&&r.y>=0&&r.x+r.w<=64&&r.y+r.h<=48);}
  c.line(x,y,x+w,y+h,0x80402010,int(rng()%20));
  Band band{true,{{x,y,x+w,y+h}}};auto strips=bandStrips(band,{0,0,64,48},.5,2);
  for(auto q:strips)CHECK(q.x>=0&&q.y>=0&&q.x+q.w<=64&&q.y+q.h<=48);
 }
 for(auto p:c.pixels){unsigned a=p>>24;CHECK((p&255)<=a&&((p>>8)&255)<=a&&((p>>16)&255)<=a);}
}
int main(){bands();cursors();selections();effects();randomized();std::printf("PASS: %u native-feedback checks; read-only identity/cursor/selection snapshots, clipping, cache, palette, fog, independent electric raster\n",checks);}
