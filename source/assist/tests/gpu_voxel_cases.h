#pragma once
#include "../../external_reader/source/gpu_voxel.h"
#include <cassert>
#include <random>
namespace gpuvoxeltest {
using namespace external;
inline voxel::Model model(){
 voxel::Model out;
 const int corners[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
 for(int n=0;n<14;++n){float x=float(n%4)*4-8,y=float(n/4)*3-6,z=float(n%3)*2;
  for(int side=0;side<6;++side){voxel::Face f;f.color=uint8_t(1+n*13+side);f.normalIndex=uint8_t((n*37+side*11)%256);f.normalType=uint8_t(n%4+1);f.turret=n>=9;
   std::array<float,3> normal{};f.hasNormal=voxel::normalVector(f.normalType,f.normalIndex,normal);
   f.normal={normal[0],-normal[1],normal[2]};
   for(int c=0;c<4;++c){int v=corners[side][c];f.p[c]={x+((v&1)?3.25f:0.f),y+((v&2)?2.75f:0.f),z+((v&4)?2.5f:0.f)};}
   out.faces.push_back(f);
  }
 }
 // Exactly coincident faces with different colors test the earliest-triangle
 // tie rule. Opposite/zero depths also test the ordered IEEE key conversion.
 auto duplicate=out.faces.front();duplicate.color=250;out.faces.push_back(duplicate);
 return out;
}
inline voxel::Vpl vpl(){
 std::vector<uint8_t> bytes(784+32*256);uint32_t count=32;std::memcpy(bytes.data()+8,&count,4);
 for(unsigned page=0;page<count;++page)for(unsigned index=0;index<256;++index)bytes[784+page*256+index]=uint8_t(1+(index+page*7)%255);
 voxel::Vpl out;assert(out.load(bytes));return out;
}
struct Case {gpuvoxel::Prepared prepared;voxel::Image expected;};
inline std::vector<Case> cases(unsigned count=96){
 auto geometry=model();auto pages=vpl();std::array<uint32_t,256> palette{};
 for(unsigned i=1;i<256;++i)palette[i]=0xff000000u|(((i*31)&255)<<16)|(((i*67)&255)<<8)|((i*113)&255);
 std::vector<Case> out;
 for(unsigned n=0;n<count;++n){
  const double angle=(n%32)*6.28318530717958647692/32,turret=(n*7%32)*6.28318530717958647692/32;
  const bool projectile=n%5==4;const double pitch=projectile?(int(n%7)-3)*.11:0;
  auto state=voxelposture::State{};if(n%3==1){state.active=true;float c=std::cos(.2f),s=std::sin(.2f);state.ramp={{1,0,0,0,0,c,-s,0,0,s,c,0}};state.sideways=.03f;state.forwards=-.07f;state.scaleX=1.2f;state.scaleY=.8f;}
  const voxel::Vpl* light=n%4?&pages:nullptr;
  const unsigned alpha=n%7?255:124;const int scale=int(n%4)+1;
  Case result;
  assert(gpuvoxel::prepare(geometry,angle,palette,result.prepared,turret,3.5f,light,alpha,pitch,projectile,scale,&state));
  assert(voxel::render(geometry,angle,palette,result.expected,turret,3.5f,light,alpha,pitch,projectile,scale,&state));
  out.push_back(std::move(result));
 }
 return out;
}
}
