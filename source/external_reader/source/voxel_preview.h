#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "voxel_lighting.h"
namespace external { namespace voxel {
// Read-only in-memory VXL/HVA capture. Never calls the game renderer. Layout
// evidence: YRpp + native 755DB0/7563A8, 5BD5C0/5BD730 and 705D70.
constexpr size_t MaxBytes=8*1024*1024,MaxVoxels=100000,MaxFaces=150000;
struct Vec {float x=0,y=0,z=0;};
struct Face {Vec p[4],normal{};uint8_t color=0,normalIndex=255;bool turret=false,hasNormal=false;};
struct Model {std::vector<Face> faces;};
struct Image {int canvasWidth=0,canvasHeight=0;int width=0,height=0,x=0,y=0;std::vector<uint32_t> pixels;std::vector<float> depth;std::vector<uint8_t> indices;bool objectPalette=false,flat=false,writeDepth=true,shadow=false;float depthBias=0;std::vector<Image> attachments;};
struct Library {uint32_t failed=0,headers=0,tailers=0,size=0,head=0,tail=0,body=0;};
struct Tail {uint32_t start,end,data;float multiplier,matrix[12],bounds[24];uint8_t size[3],normal;};
static_assert(sizeof(Tail)==164,"VXL memory tailer stride");
inline uint32_t u32(const uint8_t*p){uint32_t v;std::memcpy(&v,p,4);return v;}
inline uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline bool range(uint32_t at,size_t n){return at>=0x10000&&n<=MaxBytes&&uint64_t(at)+n<=0x100000000ull;}
inline bool finite(float v){return std::isfinite(v)&&std::abs(v)<=16384;}
inline Vec transform(const float*m,Vec p){return {m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};}
template<class Read>bool capturePart(Read read,uint32_t vxl,uint32_t hva,Model&out,float offset=0,float scaleX=1,float scaleY=1){
 Library lib{},after{};if(!range(vxl,sizeof lib)||!read(vxl,&lib,sizeof lib)||uint8_t(lib.failed)||!lib.headers||lib.headers>32||lib.tailers!=lib.headers||!lib.size||lib.size>MaxBytes||!range(lib.body,lib.size)||!range(lib.tail,lib.tailers*sizeof(Tail)))return false;
 struct Motion{uint32_t failed,layers,frames,matrices;}motion{},motionAfter{};
 if(!range(hva,sizeof motion)||!read(hva,&motion,sizeof motion)||uint8_t(motion.failed)||motion.layers!=lib.headers||!motion.frames||motion.frames>4096||!range(motion.matrices,motion.layers*48))return false;
 std::vector<uint8_t> data(lib.size);std::vector<Tail> tails(lib.tailers);std::vector<std::array<float,12>> matrices(motion.layers);
 if(!read(lib.body,data.data(),data.size())||!read(lib.tail,tails.data(),tails.size()*sizeof(Tail))||!read(motion.matrices,matrices.data(),matrices.size()*48))return false;
 Model pending;size_t total=0;
 for(size_t limb=0;limb<tails.size();++limb){auto&t=tails[limb];auto&m=matrices[limb];int sx=t.size[0],sy=t.size[1],sz=t.size[2];if(!sx||!sy||!sz||size_t(sx)*sy*sz>4*1024*1024)return false;
  for(float f:m)if(!finite(f))return false;
  Vec lo{16384,16384,16384},hi{-16384,-16384,-16384};for(int i=0;i<8;++i){float x=t.bounds[i*3],y=t.bounds[i*3+1],z=t.bounds[i*3+2];if(!finite(x)||!finite(y)||!finite(z))return false;lo.x=std::min(lo.x,x);lo.y=std::min(lo.y,y);lo.z=std::min(lo.z,z);hi.x=std::max(hi.x,x);hi.y=std::max(hi.y,y);hi.z=std::max(hi.z,z);}
  Vec step{(hi.x-lo.x)/sx,(hi.y-lo.y)/sy,(hi.z-lo.z)/sz};if(step.x<=0||step.y<=0||step.z<=0||step.x>64||step.y>64||step.z>64)return false;
  // The loader converts all three span offsets to absolute pointers. HVA
  // translation has already been multiplied by HVAMultiplier; do not repeat it.
  auto within=[&](uint32_t at,size_t n){return at>=lib.body&&uint64_t(at-lib.body)+n<=data.size();};
  if(!within(t.start,size_t(sx)*sy*4)||!within(t.end,size_t(sx)*sy*4)||!within(t.data,1))return false;
  std::vector<uint8_t> grid(size_t(sx)*sy*sz,0),shading(grid.size(),255);auto index=[&](int x,int y,int z){return (size_t(z)*sy+y)*sx+x;};
  for(int column=0;column<sx*sy;++column){int32_t start=int32_t(u32(data.data()+t.start-lib.body+column*4)),end=int32_t(u32(data.data()+t.end-lib.body+column*4));if(start==-1&&end==-1)continue;
   if(start<0||end<start||uint64_t(t.data-lib.body)+uint32_t(end)>=data.size())return false;
   size_t pos=size_t(t.data-lib.body)+start,limit=size_t(t.data-lib.body)+end+1;int z=0,groups=0;
   while(z<sz){if(++groups>sz+1||pos+2>limit)return false;int skip=data[pos++],count=data[pos++];if((!skip&&!count)||z+skip+count>sz||pos+size_t(count)*2+1>limit)return false;z+=skip;
    for(int i=0;i<count;++i){uint8_t color=data[pos++],normal=data[pos++];if(color){if(++total>MaxVoxels)return false;grid[index(column%sx,column/sx,z)]=color;shading[index(column%sx,column/sx,z)]=normal;}++z;}
    if(data[pos++]!=count)return false;
   }
  }
  const int normals[6][3]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
  const int corners[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
  for(int z=0;z<sz;++z)for(int y=0;y<sy;++y)for(int x=0;x<sx;++x){uint8_t color=grid[index(x,y,z)];if(!color)continue;
   for(int f=0;f<6;++f){int nx=x+normals[f][0],ny=y+normals[f][1],nz=z+normals[f][2];if(nx>=0&&nx<sx&&ny>=0&&ny<sy&&nz>=0&&nz<sz&&grid[index(nx,ny,nz)])continue;
    if(pending.faces.size()+out.faces.size()>=MaxFaces)return false;Face face;face.color=color;face.normalIndex=shading[index(x,y,z)];std::array<float,3> normal{};
    face.hasNormal=normalVector(t.normal,face.normalIndex,normal);
    if(face.hasNormal){face.normal={(m[0]*normal[0]+m[1]*normal[1]+m[2]*normal[2])/scaleX,(m[4]*normal[0]+m[5]*normal[1]+m[6]*normal[2])/scaleY,m[8]*normal[0]+m[9]*normal[1]+m[10]*normal[2]};float n=std::sqrt(face.normal.x*face.normal.x+face.normal.y*face.normal.y+face.normal.z*face.normal.z);if(n>1e-6f){face.normal.x/=n;face.normal.y/=n;face.normal.z/=n;}else face.hasNormal=false;}
    for(int c=0;c<4;++c){int v=corners[f][c];Vec p{lo.x+(x+(v&1))*step.x,lo.y+(y+((v>>1)&1))*step.y,lo.z+(z+((v>>2)&1))*step.z};p=transform(m.data(),p);p.x=(p.x+offset)*scaleX;p.y*=scaleY;face.p[c]=p;}pending.faces.push_back(face);
   }
  }
 }
 if(!read(vxl,&after,sizeof after)||std::memcmp(&lib,&after,sizeof lib)||!read(hva,&motionAfter,sizeof motionAfter)||std::memcmp(&motion,&motionAfter,sizeof motion))return false;
 out.faces.insert(out.faces.end(),pending.faces.begin(),pending.faces.end());return !pending.faces.empty();
}
template<class Read>bool palette(Read read,uint32_t type,uint32_t owner,std::array<uint32_t,256>&out){
 uint32_t vector=0,items=0,scheme=0;int index=-1,count=0;
 if(!read(owner+0x16054,&index,4)||index<0||index>=512||!read(type+0xDF0,&vector,4))return false;if(!vector)vector=0xB054D0;
 uint8_t colors[768];if(!read(vector+4,&items,4)||!read(vector+16,&count,4)||count<=index||count>512||!range(items,count*4)||!read(items+index*4,&scheme,4)||!range(scheme,772)||!read(scheme+4,colors,sizeof colors))return false;
 if(std::all_of(colors,colors+sizeof colors,[](uint8_t c){return c==0;}))return false;
 for(int i=0;i<256;++i)out[i]=0xFF000000u|(uint32_t(colors[i*3])<<16)|(uint32_t(colors[i*3+1])<<8)|colors[i*3+2];out[0]=0;return true;
}
inline Vec rotate(Vec p,double angle){double c=std::cos(angle),s=std::sin(angle);return {float(p.x*c-p.y*s),float(p.x*s+p.y*c),p.z};}
struct Screen {float x,y,z;};
inline Screen project(Vec p){return {float((p.x-p.y)*0.70710678118),float((p.x+p.y)*0.35355339059-p.z*0.86602540378),float((p.x+p.y)*0.61237243569+p.z*.5)};}
inline uint32_t premultiply(uint32_t c,float light,unsigned alpha=124){unsigned r=unsigned(std::clamp(((c>>16)&255)*light,0.f,255.f)),g=unsigned(std::clamp(((c>>8)&255)*light,0.f,255.f)),b=unsigned(std::clamp((c&255)*light,0.f,255.f));return (alpha<<24)|((r*alpha/255)<<16)|((g*alpha/255)<<8)|(b*alpha/255);}
inline bool render(const Model&model,double angle,const std::array<uint32_t,256>&pal,Image&out,double turretAngle=std::numeric_limits<double>::quiet_NaN(),float turretOffset=0,const Vpl*vpl=nullptr,unsigned alpha=124){
 out={};if(model.faces.empty()||model.faces.size()>MaxFaces||!std::isfinite(angle))return false;
 auto worldPoint=[&](const Face&f,Vec p){if(!f.turret||!std::isfinite(turretAngle))return rotate(p,angle);p=rotate(p,turretAngle);auto pivot=rotate({turretOffset,0,0},angle);p.x+=pivot.x;p.y+=pivot.y;return p;};
 float minX=0,minY=0,maxX=0,maxY=0;for(const auto&f:model.faces)for(auto p:f.p){auto s=project(worldPoint(f,p));if(!finite(s.x)||!finite(s.y)||!finite(s.z))return false;minX=std::min(minX,s.x);minY=std::min(minY,s.y);maxX=std::max(maxX,s.x);maxY=std::max(maxY,s.y);}
 int left=int(std::floor(minX))-1,top=int(std::floor(minY))-1,w=int(std::ceil(maxX))-left+2,h=int(std::ceil(maxY))-top+2;if(w<1||h<1||w>512||h>512)return false;
 out.width=w;out.height=h;out.x=left;out.y=top;out.pixels.assign(size_t(w)*h,0);out.depth.assign(size_t(w)*h,-std::numeric_limits<float>::infinity());out.indices.assign(size_t(w)*h,0);out.objectPalette=true;
 auto edge=[](Screen a,Screen b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};
 for(const auto&f:model.faces){Screen s[4];Vec world[4];for(int c=0;c<4;++c){world[c]=worldPoint(f,f.p[c]);s[c]=project(world[c]);s[c].x-=left;s[c].y-=top;}
  Vec a{world[1].x-world[0].x,world[1].y-world[0].y,world[1].z-world[0].z},b{world[2].x-world[0].x,world[2].y-world[0].y,world[2].z-world[0].z};Vec normal{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};float len=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
  uint8_t index=f.color;float light=1;
  if(vpl&&vpl->ready()&&(f.hasNormal||f.normalIndex>=253)){
   unsigned page=16;
   if(f.normalIndex<253){
    double heading=(f.turret&&std::isfinite(turretAngle)?turretAngle:angle)+1.5707963267948966;
    Vec source{-float(std::cos(heading)),-float(std::sin(heading)),0};
    float diffuse=std::max(0.f,f.normal.x*source.x+f.normal.y*source.y);
    float halfDot=(f.normal.x*source.x+f.normal.y*source.y+f.normal.z)*.70710678118f;
    float specular=std::max(0.f,halfDot/(3.f-2.f*halfDot));
    page=unsigned(std::clamp((diffuse+specular)*16.f,0.f,255.f));
   }
   index=vpl->map(page,index);
  }else{
   Vec n=f.hasNormal?rotate(f.normal,f.turret&&std::isfinite(turretAngle)?turretAngle:angle):normal;
   float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
   float diffuse=length>1e-6f?std::max(0.f,(n.z-n.x)*.70710678f/length):0;
   light=.8f+1.3f*diffuse;
  }
  uint32_t color=premultiply(pal[index],light,alpha);
  for(int t=0;t<2;++t){Screen v[3]={s[0],s[t+1],s[t+2]};float area=edge(v[0],v[1],v[2].x,v[2].y);if(std::abs(area)<1e-6f)continue;
   int x0=std::max(0,int(std::floor(std::min({v[0].x,v[1].x,v[2].x})))),x1=std::min(w-1,int(std::ceil(std::max({v[0].x,v[1].x,v[2].x}))));int y0=std::max(0,int(std::floor(std::min({v[0].y,v[1].y,v[2].y})))),y1=std::min(h-1,int(std::ceil(std::max({v[0].y,v[1].y,v[2].y}))));
   for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){float a=edge(v[1],v[2],x+.5f,y+.5f)/area,b=edge(v[2],v[0],x+.5f,y+.5f)/area,c=1-a-b;if(a<-.0001f||b<-.0001f||c<-.0001f)continue;float z=a*v[0].z+b*v[1].z+c*v[2].z;size_t at=size_t(y)*w+x;if(z>out.depth[at]){out.depth[at]=z;out.pixels[at]=color;out.indices[at]=index;}}
  }
 }
 for(auto&z:out.depth)if(std::isfinite(z))z*=.57735026919f;
 return true;
}
// FA2/XCC's Cshp_ts_file::is_compressed + decode3 semantics. RA2 flags bit 1
// means length-prefixed RLE rows (including flag 2 opaque rows). In particular,
// real odd-width sprites pad their final transparent run past the crop width;
// FA2 clips that run. It is not an invalid frame or a fixed-stride bitmap.
template<class Read>bool sprite(Read read,uint32_t pointer,const std::array<uint32_t,256>&pal,Image&out,int frameIndex=0,std::string*reason=nullptr,unsigned alpha=124){
 auto fail=[&](const std::string&s){if(reason)*reason=s;return false;};if(reason)reason->clear();
 uint8_t head[8];if(!range(pointer,32)||!read(pointer,head,8))return fail("header-read");if(u16(head)==65535){uint32_t decoded=0;if(!read(pointer+12,&decoded,4)||!range(decoded,32)||!read(decoded,head,8))return fail("reference-unloaded");pointer=decoded;}
 int w=u16(head+2),h=u16(head+4),frames=u16(head+6);if(u16(head)||w<1||h<1||w>2048||h>2048||frames<1||frames>8192||frameIndex<0||frameIndex>=frames||!range(pointer,8+size_t(frames)*24))return fail("header/frame-range frames="+std::to_string(frames));uint8_t frame[24];if(!read(pointer+8+frameIndex*24,frame,24))return fail("frame-read");int x=int16_t(u16(frame)),y=int16_t(u16(frame+2)),width=u16(frame+4),height=u16(frame+6);bool compressed=(u32(frame+8)&2)!=0;uint32_t offset=u32(frame+20);
 // The canvas is the origin reference, not a clipping box. Cropped frames
 // may extend beyond it (weapons/death animations). Keep their real anchor.
 if(width>2048||height>2048||std::abs(x)>4096||std::abs(y)>4096)return fail("crop-bounds");
 Image next;next.canvasWidth=w;next.canvasHeight=h;next.width=std::max(1,width);next.height=std::max(1,height);next.x=x-w/2;next.y=y-h/2;next.pixels.resize(size_t(next.width)*next.height);next.indices.resize(next.pixels.size());
 if(!offset||!width||!height){out=std::move(next);return true;}
 if(offset<uint32_t(8+frames*24)||offset>MaxBytes||uint64_t(pointer)+offset>0xffffffffull||!range(pointer+offset,1))return fail("pixel-offset");
 uint32_t at=pointer+offset;
 for(int row=0;row<height;++row){uint8_t bytes[8192];unsigned n=width;if(compressed){uint16_t length;if(!read(at,&length,2)||length<2||length>sizeof bytes)return fail("row-length row="+std::to_string(row));n=length;}if(uint64_t(at)-pointer+n>MaxBytes||!range(at,n)||!read(at,bytes,n))return fail("row-read row="+std::to_string(row));at+=n;unsigned p=compressed?2:0;int col=0;
  while(p<n){uint8_t color=bytes[p++];if(compressed&&!color){if(p>=n)return fail("rle-truncated row="+std::to_string(row));col+=std::min(int(bytes[p++]),width-col);continue;}if(col>=width)return fail("row-literal-overflow row="+std::to_string(row));if(color){size_t pixel=size_t(row)*next.width+col;next.pixels[pixel]=premultiply(pal[color],1,alpha);next.indices[pixel]=color;}++col;}
  // Unstored right-hand pixels are transparent, including empty RLE rows.
 }out=std::move(next);return true;
}
// Shared read-only model capture for formation and local observation views.
template<class Read>bool actorImage(Read read,uint32_t p,uint32_t type,uint32_t id,double angle,Image&out){
 auto word=[&](uint32_t a){uint32_t v=0;read(a,&v,4);return v;};
 if(word(p+0x10)!=id)return false;uint32_t owner=word(p+0x21C);std::array<uint32_t,256> colors{};if(!palette(read,type,owner,colors))return false;
 uint32_t parts[6]{};if(!read(type+0xB0,parts,sizeof parts))return false;
 if(!parts[0])return sprite(read,word(type+0xA4),colors,out);
 double sx=1,sy=1;read(type+0x360,&sx,8);read(type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
 Model model;if(!capturePart(read,parts[0],parts[1],model,0,float(sx),float(sy)))return false;
 int offset=int32_t(word(type+0x720));if(offset<-4096||offset>4096)return false;
 for(int n=2;n<6;n+=2)if(parts[n]&&!capturePart(read,parts[n],parts[n+1],model,float(offset)*12/256,float(sx),float(sy)))return false;
 uint32_t after[6]{};if(!read(type+0xB0,after,sizeof after)||memcmp(parts,after,sizeof parts)||word(p+0x10)!=id||word(p+0x21C)!=owner)return false;
 return render(model,angle,colors,out);
}
}}
