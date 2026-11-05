#pragma once
#ifndef OBJECT_CANVAS_HEADER
#define OBJECT_CANVAS_HEADER "../../external_reader/source/object_canvas.h"
#endif
#include OBJECT_CANVAS_HEADER
#include "../../external_reader/source/asset_decode.h"
#include "../../external_reader/source/gpu_effect_plan.h"
#include "gpu_voxel_cases.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <stdexcept>
namespace mixedstress {
using namespace external;
inline void word(std::vector<uint8_t>&v,size_t at,uint32_t x){assert(at+4<=v.size());std::memcpy(v.data()+at,&x,4);}
inline void half(std::vector<uint8_t>&v,size_t at,uint16_t x){assert(at+2<=v.size());std::memcpy(v.data()+at,&x,2);}
inline void real(std::vector<uint8_t>&v,size_t at,float x){assert(at+4<=v.size());std::memcpy(v.data()+at,&x,4);}
inline uint32_t random(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline std::array<uint32_t,256> palette(){std::array<uint32_t,256>p{};for(unsigned n=1;n<256;++n)p[n]=0xff000000u|((32+n*31%200)<<16)|((40+n*67%190)<<8)|(24+n*113%215);return p;}
inline std::vector<uint8_t> shpBytes(unsigned variant){
 const unsigned w=31,h=43,frames=8;std::vector<uint8_t>v(8+frames*24);half(v,2,w);half(v,4,h);half(v,6,frames);
 for(unsigned f=0;f<frames;++f){size_t at=8+f*24;half(v,at+4,w);half(v,at+6,h);word(v,at+8,2);word(v,at+20,uint32_t(v.size()));
  for(unsigned y=0;y<h;++y){std::vector<uint8_t>row(2);for(unsigned x=0;x<w;){auto occupied=[&](unsigned px){double dx=(double(px)-15)/(y<14?7.:11.),dy=(double(y)-19)/21.;bool body=dx*dx+dy*dy<1.;if(y>29)body&=std::abs(int(px)-15-(int(f%4)-2))>2;return body;};
    if(!occupied(x)){unsigned start=x;while(x<w&&!occupied(x))++x;row.push_back(0);row.push_back(uint8_t(x-start));}
    else{row.push_back(uint8_t(1+(x*3+y*5+variant*29+f*7)%230));++x;}}
   half(row,0,uint16_t(row.size()));v.insert(v.end(),row.begin(),row.end());}}
 return v;
}
inline std::vector<uint8_t> tmpBytes(unsigned variant){std::vector<uint8_t>v(20+52+1800);word(v,0,1);word(v,4,1);word(v,8,60);word(v,12,30);word(v,16,20);word(v,20+12,952);word(v,20+36,2);
 for(unsigned n=0;n<900;++n){v[72+n]=uint8_t(20+(n/30+variant*11)%170);v[972+n]=uint8_t((n/75+variant)%4);}return v;}
// Construct a valid VXL span stream plus four HVA matrices, then use the real
// file decoder. The benchmark does not ship or impersonate game asset files.
inline voxel::Model model(unsigned part){
 const unsigned sx=part==0?18:part==1?10:14,sy=part==0?12:part==1?8:3,sz=part==0?8:part==1?6:3,columns=sx*sy;
 std::vector<uint8_t>body(columns*8);for(unsigned c=0;c<columns;++c){word(body,c*4,uint32_t(body.size()-columns*8));body.push_back(0);body.push_back(uint8_t(sz));
  for(unsigned z=0;z<sz;++z){bool occupied=part!=0||z<4||((c%sx)>3&&(c%sx)<14&&(c/sx)>2&&(c/sx)<9);body.push_back(occupied?uint8_t(40+(c*3+z*11+part*37)%170):0);body.push_back(uint8_t((c*7+z*13)%240));}body.push_back(uint8_t(sz));word(body,columns*4+c*4,uint32_t(body.size()-columns*8-1));}
 std::vector<uint8_t>v(802+28+body.size()+92);std::memcpy(v.data(),"Voxel Animation",15);word(v,20,1);word(v,24,1);word(v,28,uint32_t(body.size()));std::memcpy(v.data()+802,"mixedpart",9);std::memcpy(v.data()+830,body.data(),body.size());size_t tail=830+body.size();word(v,tail,0);word(v,tail+4,columns*4);word(v,tail+8,columns*8);real(v,tail+12,1);
 auto identity=voxelposture::identity();std::memcpy(v.data()+tail+16,identity.data(),48);const float lo[3]={part==2?0.f:-float(sx),-float(sy),part==0?0.f:part==1?10.f:15.f};const float hi[3]={lo[0]+sx*2.f,lo[1]+sy*2.f,lo[2]+sz*2.f};for(int n=0;n<3;++n){real(v,tail+64+n*4,lo[n]);real(v,tail+76+n*4,hi[n]);}v[tail+88]=uint8_t(sx);v[tail+89]=uint8_t(sy);v[tail+90]=uint8_t(sz);v[tail+91]=4;
 std::vector<uint8_t>h(24+16+4*48);word(h,16,4);word(h,20,1);std::memcpy(h.data()+24,"mixedpart",9);for(int f=0;f<4;++f){auto m=identity;m[11]=float(f==1?1:f==3?-1:0);std::memcpy(h.data()+40+f*48,m.data(),48);}
 voxel::Model out;if(!assets::vxl(v,h,out))throw std::runtime_error("generated VXL/HVA decode failed");for(auto&face:out.faces){face.part=uint8_t(part);face.turret=part!=0;}return out;
}
struct Workload {
 int width=0,height=0;size_t tileCount=0,shpCount=0,vxlCount=0,animationCount=0,particleCount=0,beamCount=0,modelFaces=0,preparedTriangles=0;unsigned seed=0x5a17;
 camera::NativeCanvas base;std::vector<camera::ObjectCanvas::Draw>draws;
 std::vector<std::shared_ptr<const voxel::Image>>shapes,voxels,tiles;
 double assetSetupMs=0;size_t decodedShpFrames=0,decodedTmpFrames=0,decodedVxlParts=0,terrainLightUpdates=0;unsigned lightPhase=0;
 explicit Workload(bool high){auto start=std::chrono::steady_clock::now();unsigned side=high?128:64;width=int(side)*60;height=int(side)*30;tileCount=size_t(side)*side;shpCount=high?1024:256;vxlCount=high?512:128;animationCount=high?256:64;particleCount=high?768:192;beamCount=high?96:24;auto pal=palette();
  auto terrainPal=pal;for(unsigned n=1;n<256;++n)terrainPal[n]=0xff000000u|((28+n%35)<<16)|((70+n%65)<<8)|(22+n%29);
  for(unsigned n=0;n<16;++n){voxel::Image out;if(!assets::tmp(tmpBytes(n),0,terrainPal,out))throw std::runtime_error("generated TMP decode failed");tiles.push_back(std::make_shared<const voxel::Image>(std::move(out)));++decodedTmpFrames;}
  for(unsigned variant=0;variant<8;++variant){auto bytes=shpBytes(variant);for(int f=0;f<8;++f){voxel::Image out;if(!assets::shp(bytes,f,pal,out))throw std::runtime_error("generated SHP RLE decode failed");out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Vertical;auto shadow=out;shadow.shadow=true;shadow.flat=true;shadow.writeDepth=false;shadow.y+=14;out.attachments.push_back(std::move(shadow));shapes.push_back(std::make_shared<const voxel::Image>(std::move(out)));++decodedShpFrames;}}
  auto geometry=model(0);for(unsigned part=1;part<3;++part){auto extra=model(part);geometry.faces.insert(geometry.faces.end(),extra.faces.begin(),extra.faces.end());}decodedVxlParts=3;modelFaces=geometry.faces.size();auto vpl=gpuvoxeltest::vpl();
  for(unsigned n=0;n<32;++n){voxel::Model posed;if(!voxel::animated(geometry,int(n%4),int((n/2)%4),posed))throw std::runtime_error("HVA animation failed");voxel::Image out;double angle=n*6.283185307179586/32,turret=(n*7%32)*6.283185307179586/32;
   if(!gpuvoxel::image(posed,angle,pal,out,turret,1.5f,&vpl,255))throw std::runtime_error("VXL GPU preparation failed");out.nativeDepth.enabled=true;out.nativeDepth.voxel=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;preparedTriangles+=out.gpuRaster->triangles.size();auto shadow=out;shadow.shadow=true;shadow.flat=true;shadow.writeDepth=false;shadow.nativeDepth.voxel=false;shadow.y+=9;out.attachments.push_back(std::move(shadow));voxels.push_back(std::make_shared<const voxel::Image>(std::move(out)));}
  if(!base.reset(width,height))throw std::runtime_error("canvas allocation failed");terrainLight(0);
  draws.resize(shpCount+vxlCount+animationCount);update(0);assetSetupMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 }
 void terrainLight(unsigned phase){unsigned side=unsigned(width/60);size_t id=0;for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x){float px=float((int(x)-int(y)+int(side))*30),py=float((x+y)*15+15);lighting::State light;light.intensity=850+int((x+y)%7)*25+int(phase)*45;base.terrainSource(id++,tiles[(x*3+y*7)%tiles.size()],px,py,py,light,int((x/16+y/16)%2));}lightPhase=phase;++terrainLightUpdates;}
 void update(unsigned frame){unsigned phase=frame/60%4;if(phase!=lightPhase)terrainLight(phase);for(size_t n=0;n<draws.size();++n){auto&d=draws[n];uint32_t r=random(uint32_t(n)+seed);unsigned side=unsigned(width/60),cx=(r%side),cy=(r>>9)%side;double px=(int(cx)-int(cy)+int(side))*30.,py=(cx+cy)*15.+15;
   d.x=float(px+int((frame+unsigned(n)*3)%31)-15);d.y=float(py+int((frame*2+unsigned(n))%17)-8);d.base=float(py);d.lift=1;d.drop=0;d.object=uint32_t(n+1);d.id=uint32_t(n+1);d.light={};d.material={};d.material.nativeZ=true;d.material.nativeZAdjust=-96;
   if(n<shpCount)d.image=shapes[((n%8)*8+(frame/3+n)%8)%shapes.size()];else if(n<shpCount+vxlCount){d.image=voxels[(frame/4+n*3)%voxels.size()];if(n%41==0){d.material.displace=true;d.material.cloakWords=-width-3;}d.light.intensity=900+int(n%5)*25;}
   else{d.image=shapes[(frame+n*5)%shapes.size()];d.material.translucency=2;d.x+=24;d.y-=12;}
  }std::stable_sort(draws.begin(),draws.end(),[](const auto&a,const auto&b){return a.base<b.base;});
 }
 worldfx::View view(int w,int h)const{worldfx::View v;v.width=width;v.height=height;v.pixelsW=w;v.pixelsH=h;v.zscale=.5;return v;}
 template<class Sink>void paintEffects(Sink&sink,int w,int h,unsigned frame)const{
  for(size_t n=0;n<particleCount;++n){auto r=random(uint32_t(n)+seed+73);int x=int((r+frame*(3+n%5))%unsigned(w)),y=int(((r>>12)+frame*(1+n%3))%unsigned(h));sink.pixel(x,y,0x80c0ffu^uint32_t(n*7919),1,false);}
  for(size_t n=0;n<beamCount;++n){auto r=random(uint32_t(n)+seed+907);double x=double((r+frame*7)%unsigned(w)),y=double(((r>>14)+frame*3)%unsigned(h));sink.line({{x,y}},{{std::clamp(x+double(int(n%3)-1)*50,0.,double(w-1)),std::clamp(y-20-double(n%7)*3,0.,double(h-1))}},0x60a0ffu^uint32_t(n*467),.8,true);}
 }
 std::shared_ptr<const gpueffects::Plan>effects(int w,int h,unsigned frame)const{gpueffects::Builder builder(view(w,h));paintEffects(builder,w,h,frame);return builder.finish();}
};
}
