#pragma once
#include "voxel_preview.h"
#include <cstddef>
// GPU input preparation retains the CPU native-light helpers. It deliberately
// does not read game memory or change VXL/HVA capture. Vulkan raster kernels
// perform the nested per-triangle/per-pixel work in one pending scene batch.
namespace external { namespace gpuvoxel {
using namespace voxel;
struct alignas(16) Triangle {
 std::array<float,4> vertex[3]{};
 std::array<uint32_t,4> material{}; // PARGB, source palette index, earliest order, reserved
};
static_assert(sizeof(Triangle)==64&&offsetof(Triangle,material)==48,"GPU std430 triangle layout");
struct Prepared {
 int rasterScale=1,width=0,height=0,x=0,y=0,depthBottom=0;
 uint64_t candidatePixels=0,maximumTrianglePixels=0;
 std::vector<Triangle> triangles;
};
struct Params {
 uint32_t triangleOffset=0,triangleCount=0,depthOffset=0,winnerOffset=0;
 uint32_t colorOffset=0,indexOffset=0,width=0,height=0;
};
static_assert(sizeof(Params)==32,"GPU voxel push constants");
inline bool prepare(const Model&model,double angle,const std::array<uint32_t,256>&pal,Prepared&out,double turretAngle=std::numeric_limits<double>::quiet_NaN(),float turretOffset=0,const Vpl*vpl=nullptr,unsigned alpha=124,double pitch=0,bool projectile=false,int rasterScale=1,const voxelposture::State*posture=nullptr){
 out={};if(rasterScale<1||rasterScale>resolutionsampling::MaxRasterScale||model.faces.empty()||model.faces.size()>MaxFaces||!std::isfinite(angle)||(posture&&!posture->available))return false;
 const bool tilted=posture&&posture->active&&!projectile;
 const auto geometryWorld=tilted?voxelposture::world(*posture,angle,false):voxelposture::identity();
 const auto lightWorld=tilted?voxelposture::world(*posture,angle,true):voxelposture::identity();
 const auto bodyLight=tilted?postureLight(angle,lightWorld):nativeVoxelLight(angle,pitch),turretLight=std::isfinite(turretAngle)?(tilted?postureLight(turretAngle,lightWorld):nativeVoxelLight(turretAngle,pitch)):bodyLight;
 const auto worldLight=nativeVoxelWorldLight();
 std::array<std::array<uint8_t,256>,5> bodyPages,turretPages;std::array<bool,5> bodyPagesReady{},turretPagesReady{};
 auto pageFor=[&](const Face&face)->unsigned{
  if(face.normalIndex>=253||face.normalType<1||face.normalType>4)return 16;
  auto&pages=(face.turret?turretPages:bodyPages)[face.normalType];auto&ready=(face.turret?turretPagesReady:bodyPagesReady)[face.normalType];
  if(!ready){pages.fill(16);std::array<float,3> raw{};for(unsigned i=0;i<253&&normalVector(face.normalType,i,raw);++i)pages[i]=uint8_t(projectile?nativeProjectilePage(raw,face.turret?turretLight:bodyLight):nativeVoxelPage(raw,face.turret?turretLight:bodyLight));ready=true;}
  return pages[face.normalIndex];
 };
 auto pitched=[&](Vec p){if(pitch==0)return p;double c=std::cos(pitch),s=std::sin(pitch);return Vec{float(p.x*c+p.z*s),p.y,float(-p.x*s+p.z*c)};};
 auto worldPoint=[&](const Face&f,Vec p){auto point=posePoint(f,pitched(p),angle,turretAngle,turretOffset);return tilted?posturePoint(geometryWorld,point):point;};
 float minX=0,minY=0,maxX=0,maxY=0;for(const auto&f:model.faces)for(auto p:f.p){auto s=project(worldPoint(f,p));if(!finite(s.x)||!finite(s.y)||!finite(s.z))return false;minX=std::min(minX,s.x);minY=std::min(minY,s.y);maxX=std::max(maxX,s.x);maxY=std::max(maxY,s.y);}
 int left=int(std::floor(minX))-1,top=int(std::floor(minY))-1,w=int(std::ceil(maxX))-left+2,h=int(std::ceil(maxY))-top+2;if(w<1||h<1||w>512||h>512)return false;
 // Rasterize the original VXL faces at the presentation's required sample
 // density. These are new geometry samples; world depth stays in native units.
 if(uint64_t(w)*rasterScale>resolutionsampling::MaxDimension||uint64_t(h)*rasterScale>resolutionsampling::MaxDimension||uint64_t(w)*h*rasterScale*rasterScale>resolutionsampling::MaxPixels)return false;
 left*=rasterScale;top*=rasterScale;w*=rasterScale;h*=rasterScale;
 out.rasterScale=rasterScale;out.width=w;out.height=h;out.x=left;out.y=top;out.depthBottom=(int(std::ceil(maxY))-1)*rasterScale;out.triangles.reserve(model.faces.size()*2);
 auto edge=[](Screen a,Screen b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};
 for(const auto&f:model.faces){Screen s[4];Vec world[4];for(int c=0;c<4;++c){world[c]=worldPoint(f,f.p[c]);s[c]=project(world[c]);s[c].x=s[c].x*rasterScale-left;s[c].y=s[c].y*rasterScale-top;}
  Vec a{world[1].x-world[0].x,world[1].y-world[0].y,world[1].z-world[0].z},b{world[2].x-world[0].x,world[2].y-world[0].y,world[2].z-world[0].z};Vec normal{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};float len=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
  uint8_t index=f.color;float light=1;
  if(vpl&&vpl->ready()&&(f.hasNormal||f.normalIndex>=253)){
   // Precalculate once per normal mode/facing, as 753D00 does. HVA changes
   // geometry but not these raw normal-table indices; transformed normals are
   // reserved for the fallback path.
   index=vpl->map(pageFor(f),index);
  }else{
   Vec n=f.hasNormal?rotate(pitched(f.normal),f.turret&&std::isfinite(turretAngle)?turretAngle:angle):normal;
   if(tilted&&f.hasNormal)n=posturePoint(geometryWorld,n,true);
   float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
   float diffuse=length>1e-6f?std::max(0.f,(n.x*worldLight[0]+n.y*worldLight[1]+n.z*worldLight[2])/length):0;
   light=.8f+1.3f*diffuse;
  }
  uint32_t color=premultiply(pal[index],light,alpha);
  for(int t=0;t<2;++t){Screen v[3]={s[0],s[t+1],s[t+2]};float area=edge(v[0],v[1],v[2].x,v[2].y);if(std::abs(area)<1e-6f)continue;
   int x0=std::max(0,int(std::floor(std::min({v[0].x,v[1].x,v[2].x})))),x1=std::min(w-1,int(std::ceil(std::max({v[0].x,v[1].x,v[2].x}))));int y0=std::max(0,int(std::floor(std::min({v[0].y,v[1].y,v[2].y})))),y1=std::min(h-1,int(std::ceil(std::max({v[0].y,v[1].y,v[2].y}))));
   if(x1<x0||y1<y0)continue;
   const uint64_t areaPixels=uint64_t(x1-x0+1)*(y1-y0+1);out.candidatePixels+=areaPixels;out.maximumTrianglePixels=std::max(out.maximumTrianglePixels,areaPixels);
   Triangle triangle{};for(int n=0;n<3;++n){triangle.vertex[n][0]=v[n].x;triangle.vertex[n][1]=v[n].y;triangle.vertex[n][2]=v[n].z;}
   triangle.material={{color,index,uint32_t(out.triangles.size()),0}};out.triangles.push_back(triangle);
  }
 }
 return true;
}
// Independent upload-layout oracle for the atomic kernels. Production CPU
// fallback remains voxel::render, so failed GPU work does not require a readback.
inline bool rasterReference(const Prepared&input,Image&out){
 out={};if(input.width<1||input.height<1||input.width>8192||input.height>8192||uint64_t(input.width)*input.height>resolutionsampling::MaxPixels||input.triangles.size()>MaxFaces*2)return false;
 out.rasterScale=input.rasterScale;out.width=input.width;out.height=input.height;out.x=input.x;out.y=input.y;out.depthBottom=input.depthBottom;out.objectPalette=true;
 out.pixels.assign(size_t(out.width)*out.height,0);out.indices.assign(out.pixels.size(),0);out.depth.assign(out.pixels.size(),-std::numeric_limits<float>::infinity());
 auto edge=[](const std::array<float,4>&a,const std::array<float,4>&b,float x,float y){return (x-a[0])*(b[1]-a[1])-(y-a[1])*(b[0]-a[0]);};
 for(const auto&t:input.triangles){const auto&v=t.vertex;float area=edge(v[0],v[1],v[2][0],v[2][1]);if(std::abs(area)<1e-6f)continue;
  int x0=std::max(0,int(std::floor(std::min({v[0][0],v[1][0],v[2][0]})))),x1=std::min(out.width-1,int(std::ceil(std::max({v[0][0],v[1][0],v[2][0]}))));
  int y0=std::max(0,int(std::floor(std::min({v[0][1],v[1][1],v[2][1]})))),y1=std::min(out.height-1,int(std::ceil(std::max({v[0][1],v[1][1],v[2][1]}))));
  for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){float a=edge(v[1],v[2],x+.5f,y+.5f)/area,b=edge(v[2],v[0],x+.5f,y+.5f)/area,c=1-a-b;
   if(a<-.0001f||b<-.0001f||c<-.0001f)continue;float z=a*v[0][2]+b*v[1][2]+c*v[2][2];size_t at=size_t(y)*out.width+x;
   if(z>out.depth[at]){out.depth[at]=z;out.pixels[at]=t.material[0];out.indices[at]=uint8_t(t.material[1]);}
  }
 }
 for(auto&z:out.depth)if(std::isfinite(z))z*=.57735026919f;
 return true;
}
inline uint32_t orderedDepth(float z){if(z==0)z=0;uint32_t bits=0;std::memcpy(&bits,&z,4);return (bits&0x80000000u)?~bits:(bits^0x80000000u);}
inline float decodedDepth(uint32_t key){uint32_t bits=(key&0x80000000u)?(key^0x80000000u):~key;float z=0;std::memcpy(&z,&bits,4);return z;}
inline bool image(const Model&model,double angle,const std::array<uint32_t,256>&pal,Image&out,double turretAngle=std::numeric_limits<double>::quiet_NaN(),float turretOffset=0,const Vpl*vpl=nullptr,unsigned alpha=124,double pitch=0,bool projectile=false,int rasterScale=1,const voxelposture::State*posture=nullptr){
 auto prepared=std::make_shared<Prepared>();
 if(!prepare(model,angle,pal,*prepared,turretAngle,turretOffset,vpl,alpha,pitch,projectile,rasterScale,posture))return false;
 out={};out.rasterScale=prepared->rasterScale;out.width=prepared->width;out.height=prepared->height;out.x=prepared->x;out.y=prepared->y;out.depthBottom=prepared->depthBottom;out.objectPalette=true;out.gpuRaster=std::move(prepared);return true;
}
// Same-frame CPU fallback. Keep caller metadata (including standing-Z,
// projectile no-Z, shadow offsets and attachments), rather than substituting
// the model's internal Z buffer for the scene's standing-shape convention.
inline bool materialize(const Image&input,Image&out){
 if(!input.gpuRaster){out=input;return true;}
 if(input.width!=input.gpuRaster->width||input.height!=input.gpuRaster->height||input.rasterScale!=input.gpuRaster->rasterScale)return false;
 Image raster;if(!rasterReference(*input.gpuRaster,raster))return false;
 out=input;out.pixels=std::move(raster.pixels);out.indices=std::move(raster.indices);out.gpuRaster.reset();return true;
}
}}
