#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gpu_voxel_cases.h"
#include <cstdio>
using namespace external;
// Original per-face implementation is intentionally independent of the new
// limb inverse reuse. Exercise singular matrices and interleaved owners too.
static bool animatedReference(const voxel::Model&model,int body,int turret,voxel::Model&out){
 using namespace voxel;if(body<0||turret<0)return false;out=model;
 for(auto&face:out.faces){if(!face.motion||face.motion->changes.empty())continue;const auto&m=face.motion->changes[unsigned(face.turret?turret:body)%face.motion->changes.size()];for(auto&p:face.p)p=transform(m.data(),p);if(face.hasNormal){voxelposture::Matrix inv;if(!inverseMotion(m,inv)){face.hasNormal=false;continue;}auto n=face.normal;face.normal={inv[0]*n.x+inv[4]*n.y+inv[8]*n.z,inv[1]*n.x+inv[5]*n.y+inv[9]*n.z,inv[2]*n.x+inv[6]*n.y+inv[10]*n.z};float length=std::sqrt(face.normal.x*face.normal.x+face.normal.y*face.normal.y+face.normal.z*face.normal.z);if(length>1e-6f){face.normal.x/=length;face.normal.y/=length;face.normal.z/=length;}else return false;}}return true;
}
static void motion(){
 auto a=std::make_shared<voxel::Motion>(),b=std::make_shared<voxel::Motion>();a->changes.resize(3,voxelposture::identity());a->changes[1]={{1.2f,.3f,-.2f,3.f,.1f,.7f,.04f,-5.f,.08f,.2f,2.f,7.f}};a->changes[2][8]=a->changes[2][9]=a->changes[2][10]=0;
 b->changes.resize(2,voxelposture::identity());b->changes[1]={{.7f,.8f,-.3f,9.f,.13f,.9f,.2f,1.f,.1f,.2f,1.3f,7.f}};
 auto model=gpuvoxeltest::model();for(size_t i=0;i<model.faces.size();++i){auto&f=model.faces[i];f.motion=i%3?a:b;f.hasNormal=i%7!=0;f.normal={.1f,.3f,.6f};f.turret=i%2;}
 for(int body=-1;body<13;++body)for(int turret=0;turret<11;++turret){voxel::Model actual,expected;assert(voxel::animated(model,body,turret,actual)==animatedReference(model,body,turret,expected));assert(actual.faces.size()==expected.faces.size());for(size_t i=0;i<actual.faces.size();++i){auto&x=actual.faces[i];auto&y=expected.faces[i];assert(!std::memcmp(x.p,y.p,sizeof x.p)&&!std::memcmp(&x.normal,&y.normal,sizeof x.normal)&&x.hasNormal==y.hasNormal&&x.motion==y.motion);}}
 model.faces[0].normal={};model.faces[0].hasNormal=true;voxel::Model x,y;assert(!voxel::animated(model,0,0,x)&&!animatedReference(model,0,0,y));
}
static void pages(){
 // Every valid/invalid normal and each body/turret/barrel namespace, including
 // possible page 255. Cache readiness is separate from the page value.
 for(int pose=0;pose<25;++pose)for(bool projectile:{false,true}){voxel::NormalPages pages;voxel::Face f;auto light=voxel::nativeVoxelLight(pose*.139,(pose-12)*.02);
  for(int repeat=0;repeat<2;++repeat)for(unsigned mode=0;mode<6;++mode)for(unsigned part=0;part<3;++part)for(unsigned index=0;index<256;++index){f.normalType=uint8_t(mode);f.normalIndex=uint8_t(index);f.part=uint8_t(part);f.turret=part!=0;std::array<float,3> normal{};unsigned expected=16;if(index<253&&voxel::normalVector(mode,index,normal))expected=uint8_t(projectile?voxel::nativeProjectilePage(normal,light):voxel::nativeVoxelPage(normal,light));assert(pages.page(f,light,projectile)==expected);}
 }
}
static void triangles(){
 std::mt19937 random(192330);std::uniform_real_distribution<float> xy(-12,79),z(-17,53);size_t tested=0;
 for(unsigned batch=0;batch<64;++batch){gpuvoxel::Prepared packet;packet.width=71;packet.height=53;
  for(unsigned n=0;n<192;++n){gpuvoxel::Triangle t;for(auto&v:t.vertex)v={{xy(random),xy(random),z(random),0}};
   if(n%4==0){float x=float(n%71)+.49999997f,y=float(n%53)+.5f;const float width=float(n%9)+.000001f;t.vertex[0]={{x,y,z(random),0}};t.vertex[1]={{x+width,y,4.f,0}};t.vertex[2]={{x+width*.5f,y+float(n%7)+.0000001f,4.f,0}};}
   if(n%11==0&&n)t=packet.triangles.back(); // equal Z, later material must lose
   if(n%3==0)std::swap(t.vertex[0],t.vertex[1]);
   t.material={{0xff000000u|random()%0xffffff,uint8_t(n+1),n,0}};packet.triangles.push_back(t);
  }
  voxel::Image actual,expected;actual.width=packet.width;actual.height=packet.height;actual.pixels.assign(71*53,0);actual.indices.assign(71*53,0);actual.depth.assign(71*53,-std::numeric_limits<float>::infinity());
  for(auto&t:packet.triangles){voxel::Screen v[3];for(int n=0;n<3;++n)v[n]={t.vertex[n][0],t.vertex[n][1],t.vertex[n][2]};auto edge=[](voxel::Screen a,voxel::Screen b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};float area=edge(v[0],v[1],v[2].x,v[2].y);if(std::abs(area)<1e-6f)continue;
   int x0=std::max(0,int(std::floor(std::min({v[0].x,v[1].x,v[2].x})))),x1=std::min(actual.width-1,int(std::ceil(std::max({v[0].x,v[1].x,v[2].x}))));int y0=std::max(0,int(std::floor(std::min({v[0].y,v[1].y,v[2].y})))),y1=std::min(actual.height-1,int(std::ceil(std::max({v[0].y,v[1].y,v[2].y}))));voxel::rasterTriangle(v,area,x0,x1,y0,y1,t.material[0],uint8_t(t.material[1]),actual);++tested;
  }
  for(auto&value:actual.depth)if(std::isfinite(value))value*=.57735026919f;
  assert(gpuvoxel::rasterReference(packet,expected));assert(actual.pixels==expected.pixels&&actual.indices==expected.indices&&actual.depth==expected.depth);
 }
 std::printf("PASS %zu ordered triangles: SSE2/scalar exact RGBA, index, float-Z; narrow/tail/clipped/both winding/degenerate/depth-tie; 154 HVA combinations and 460800 lazy normal pages\n",tested);
}
int main(){motion();pages();triangles();}
