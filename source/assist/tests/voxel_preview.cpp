#include "../../external_reader/source/voxel_preview.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <cstring>
using namespace external::voxel;
static std::vector<uint8_t> memory(2000000);constexpr uint32_t base=0x10000;
static bool read(uint32_t at,void*out,size_t n){if(at<base||uint64_t(at-base)+n>memory.size())return false;std::memcpy(out,memory.data()+at-base,n);return true;}
template<class T>void write(uint32_t at,T x){std::memcpy(memory.data()+at-base,&x,sizeof x);}
static void bytes(uint32_t at,const void*p,size_t n){std::memcpy(memory.data()+at-base,p,n);}
static Model capture(){Model m;assert(capturePart(read,base,base+0x100,m));return m;}

// Original MatrixApply/MatrixMultiply outputs, including local HVA and a
// turret offset rotated by a different body facing. See voxel-lighting-native.
static void nativeGeometry(const Library&lib,Tail original){
 const float hva[3][12]={{1,0,0,0,0,1,0,0,0,0,1,0},{0,-1,0,3,1,0,0,5,0,0,1,2},{.8f,0,.6f,7,0,1,0,-3,-.6f,0,.8f,4}};
 struct Anchor{unsigned facing,hva,body;float offset;Vec screen;};
 const Anchor anchors[]={
  {0,0,5,0,{3.525767565f,-5.783790588f,-4.014563084f}},
  {0,1,5,0,{-7.792265415f,-8.921742439f,-5.448217392f}},
  {0,1,5,7,{-0.924682617f,-8.243892908f,-4.274840713f}},
  {0,2,5,0,{10.874896049f,-6.653036594f,-7.118545055f}},
  {0,2,5,7,{17.742478848f,-5.975187063f,-5.945168376f}},
  {8,0,13,0,{6.369376659f,-0.834028363f,4.553617001f}},
  {8,1,13,0,{9.180444717f,-8.227345467f,-4.246197224f}},
  {8,1,13,7,{7.835882068f,-4.790998220f,1.702218533f}},
  {8,2,13,0,{9.491923332f,3.534880877f,10.517031670f}},
  {8,2,13,7,{8.147360682f,6.971228123f,16.465447426f}},
  {16,0,21,0,{-3.515992403f,0.591107070f,7.020567894f}},
  {16,1,21,0,{7.806338787f,0.256782234f,10.440071106f}},
  {16,1,21,7,{0.936685562f,-0.415796697f,9.275817990f}},
  {16,2,21,0,{-10.860321999f,2.851426840f,9.333954811f}},
  {16,2,21,7,{-17.729975224f,2.178847909f,8.169701695f}},
  {24,0,29,0,{-6.374776840f,-4.356466770f,-1.543824673f}},
  {24,1,29,0,{-9.168479919f,-0.424590290f,9.260596275f}},
  {24,1,29,7,{-7.823917270f,-3.860937536f,3.312180519f}},
  {24,2,29,0,{-9.508592606f,-7.337539196f,-8.303438187f}},
  {24,2,29,7,{-8.164029956f,-10.773886442f,-14.251853943f}},
 };
 auto tail=original;for(int i=0;i<8;++i){tail.bounds[i*3]=(i&1)?8:7;tail.bounds[i*3+1]=(i&2)?3:2;tail.bounds[i*3+2]=(i&4)?7:3;}
 write(lib.tail,tail);float saved[12];assert(read(base+0x200,saved,sizeof saved));
 for(const auto&a:anchors){
  bytes(base+0x200,hva[a.hva],48);auto m=capture();auto face=m.faces.front();face.turret=a.hva!=0;
  double direction=(int(a.facing)-8)*(6.28318530717958647692/32),body=(int(a.body)-8)*(6.28318530717958647692/32);
  auto actual=project(posePoint(face,face.p[0],face.turret?body:direction,direction,a.offset));
  assert(std::hypot(actual.x-a.screen.x,actual.y-a.screen.y)<.11f);assert(std::abs(actual.z-a.screen.z)<.1f);
  auto raw=Normals4[1];Vec n{hva[a.hva][0]*raw[0]+hva[a.hva][1]*raw[1]+hva[a.hva][2]*raw[2],-(hva[a.hva][4]*raw[0]+hva[a.hva][5]*raw[1]+hva[a.hva][6]*raw[2]),hva[a.hva][8]*raw[0]+hva[a.hva][9]*raw[1]+hva[a.hva][10]*raw[2]};
  float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);assert(std::abs(face.normal.x-n.x/length)<.00001f&&std::abs(face.normal.y-n.y/length)<.00001f&&std::abs(face.normal.z-n.z/length)<.00001f);
 }
 bytes(base+0x200,saved,sizeof saved);write(lib.tail,original);
}

int main(int argc,char**argv){Library lib{};lib.headers=lib.tailers=1;lib.head=base+0x300;lib.tail=base+0x400;lib.body=base+0x1000;lib.size=19;write(base,lib);uint32_t motion[4]={0,1,1,base+0x200};bytes(base+0x100,motion,sizeof motion);float matrix[12]={1,0,0,0,0,1,0,0,0,0,1,0};bytes(base+0x200,matrix,sizeof matrix);
 Tail t{};t.start=lib.body;t.end=lib.body+4;t.data=lib.body+8;t.multiplier=.083333f;t.size[0]=t.size[1]=1;t.size[2]=4;t.normal=4;for(int n=0;n<8;++n){t.bounds[n*3]=(n&1)?1.f:-1.f;t.bounds[n*3+1]=(n&2)?1.f:-1.f;t.bounds[n*3+2]=(n&4)?4.f:0.f;}write(lib.tail,t);write<int32_t>(lib.body,0);write<int32_t>(lib.body+4,10);uint8_t span[]={0,4,16,1,17,1,18,1,19,1,4};bytes(lib.body+8,span,sizeof span);
 Model m=capture();assert(m.faces.size()==18);nativeGeometry(lib,t);matrix[3]=8;bytes(base+0x200,matrix,sizeof matrix);auto moved=capture();for(size_t i=0;i<m.faces.size();++i)assert(std::abs(moved.faces[i].p[0].x-m.faces[i].p[0].x-8)<.001);matrix[3]=0;bytes(base+0x200,matrix,sizeof matrix);
 for(uint32_t broken:{0u,base-1,lib.body+100}){auto old=t.start;t.start=broken;write(lib.tail,t);Model bad;assert(!capturePart(read,base,base+0x100,bad));t.start=old;}write(lib.tail,t);
 memory[lib.body+8-base]=255;{Model bad;assert(!capturePart(read,base,base+0x100,bad));}bytes(lib.body+8,span,sizeof span);
 int reads=0;auto race=[&](uint32_t at,void*out,size_t n){bool ok=read(at,out,n);if(at==base&&++reads==2)static_cast<Library*>(out)->body+=4;return ok;};{Model bad;assert(!capturePart(race,base,base+0x100,bad));}
 std::array<uint32_t,256> pal{};for(int n=0;n<256;++n)pal[n]=0xff80d090;Image image;assert(render(m,0,pal,image));bool painted=false;for(uint32_t c:image.pixels)if(c){painted=true;assert((c>>24)==124&&((c>>16)&255)<=124&&((c>>8)&255)<=124&&(c&255)<=124);}assert(painted);
 // A constructed hull/turret/barrel scene exercises depth and direction in an
 // inspectable output without redistributing any game's copyrighted models.
 Model tank;auto block=[&](float x,float y,float z,float sx,float sy,float sz,int color){for(auto face:m.faces){for(auto&p:face.p){p.x=x+p.x*sx;p.y=y+p.y*sy;p.z=z+p.z*sz;}face.color=color;tank.faces.push_back(face);}};
 block(0,0,0,22,10,2,2);block(0,0,8,9,7,2.5,2);block(15,0,15,15,1.5,.6,0x10);pal[2]=0xff759151;pal[0x10]=0xffe6b450;assert(render(tank,.45,pal,image));FILE*f=argc>1?fopen(argv[1],"wb"):nullptr;if(f){fprintf(f,"P6\n%d %d\n255\n",image.width,image.height);for(uint32_t c:image.pixels){unsigned a=c>>24;uint8_t rgb[3]={uint8_t(((c>>16)&255)+(255-a)*20/255),uint8_t(((c>>8)&255)+(255-a)*26/255),uint8_t((c&255)+(255-a)*35/255)};fwrite(rgb,1,3,f);}fclose(f);}
 for(size_t i=m.faces.size();i<tank.faces.size();++i)tank.faces[i].turret=true;Image aligned,turned;assert(render(tank,0,pal,aligned,0,3));assert(render(tank,0,pal,turned,1.5707963267948966,3));assert(aligned.pixels!=turned.pixels);
 std::cout<<"PASS native asymmetric HVA/body/turret/barrel projection anchors; independent turret/body rotation and offset; VXL spans, HVA translation once, bounds, malformed input, depth, premultiplied alpha\n";
}
