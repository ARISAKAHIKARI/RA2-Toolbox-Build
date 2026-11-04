#pragma once
#include "voxel_posture.h"
#include "projection.h"
// Drive/Ship use Cell::RampType for shadows, independently of their animated
// Draw_Matrix. Shadow_Point uses the exact floor under the object's XY.
namespace external { namespace voxelshadow {
struct State {voxelposture::State posture;int drop=0;bool known=false;};
inline bool floorHeight(int level,int height,int x,int y,unsigned ramp,double scale,const std::array<double,5>&terms,int&out){
 if(height<=0||height>4096||ramp>20)return false;
 out=int(double(level)*height+.5);
 if(ramp){
  if(!std::isfinite(scale)||scale<=0||scale>16)return false;
  for(double v:terms)if(!std::isfinite(v)||std::abs(v)>1048576)return false;
  if(terms[3]<0||terms[3]>8192)return false;
  double extra=(y&255)*terms[1]*scale+(x&255)*terms[0]*scale+terms[2]+terms[4];
  out=int(out+std::clamp(extra,0.,terms[3]));
 }return true;
}
template<class Read>State capture(GameView<Read>&g,uint32_t object,int kind){
 State out;if(kind!=2||object<0x10000)return out;
 uint32_t loco=g.template get<uint32_t>(object+0x674),table=g.template get<uint32_t>(loco);
 if(table!=0x7E7EB0&&table!=0x7F2D8C)return out;
 out.known=true;out.posture.available=false;
 std::array<int,3> point{},again{};uint32_t cells=0,cell=0;int count=0,height=0;uint8_t bridge=0;
 if(g.template get<uint32_t>(loco+8)!=object||!g.bytes(object+0x9C,point.data(),sizeof point)||
    !g.bytes(object+0x8C,&bridge,1)||!g.bytes(0x87F924,&cells,4)||!g.bytes(0x87F928,&count,4)||
    !g.bytes(0x89E7C0,&height,4)||point[0]<0||point[1]<0||point[0]>=512*256||point[1]>=512*256||std::abs(int64_t(point[2]))>1048576)return out;
 unsigned index=unsigned(point[1]/256)*512+unsigned(point[0]/256);
 if(cells<0x10000||count<=0||count>512*512||index>=unsigned(count)||!g.bytes(cells+index*4,&cell,4)||cell<0x10000)return out;
 int16_t xy[2]{};int8_t level=0;uint8_t ramp=0;
 if(!g.bytes(cell+0x24,xy,4)||xy[0]!=point[0]/256||xy[1]!=point[1]/256||
    !g.bytes(cell+0x11B,&level,1)||!g.bytes(cell+0x11C,&ramp,1)||ramp>20)return out;
 voxelposture::Matrix matrix{},verify{};uint32_t at=0xB45188+unsigned(ramp)*48;
 if(!g.bytes(at,matrix.data(),sizeof matrix)||!voxelposture::validMatrix(matrix))return out;
 std::array<double,5> terms{},verifyTerms{};double scale=1;uint32_t coefficients=0;
 if(ramp){uint8_t initialized=0;coefficients=0x81C900+(unsigned(ramp)-1)*40;
  if(!g.bytes(0x89E770,&initialized,1)||(initialized&7)!=7||!g.bytes(0x89E740,&scale,8)||!g.bytes(coefficients,terms.data(),sizeof terms))return out;
 }
 int floor=0,bridgeHeight=0;double zScale=0;
 if(!floorHeight(level,height,point[0],point[1],ramp,scale,terms,floor)||
    !g.bytes(0xB0CD48,&zScale,8)||!std::isfinite(zScale)||zScale<=0||zScale>1||
    (bridge&&(!g.bytes(0xAC13BC,&bridgeHeight,4)||bridgeHeight<=0||bridgeHeight>8192)))return out;
 int drop=screenHeight(point[2]-floor-bridgeHeight,zScale);if(std::abs(drop)>8192)return out;
 uint8_t verifyRamp=0;int8_t verifyLevel=0;
 if(!g.bytes(object+0x9C,again.data(),sizeof again)||point!=again||!g.bytes(at,verify.data(),sizeof verify)||matrix!=verify||
    !g.bytes(cell+0x11B,&verifyLevel,1)||level!=verifyLevel||!g.bytes(cell+0x11C,&verifyRamp,1)||ramp!=verifyRamp||
    (ramp&&(!g.bytes(coefficients,verifyTerms.data(),sizeof verifyTerms)||terms!=verifyTerms))||
    g.template get<uint32_t>(object+0x674)!=loco||g.template get<uint32_t>(loco)!=table||g.template get<uint32_t>(loco+8)!=object)return out;
 out.posture.available=true;out.posture.ramp=matrix;out.posture.active=matrix!=voxelposture::identity();out.drop=drop;return out;
}
inline voxelposture::State projection(const State&s){
 auto out=s.posture;
 // Native 753F90 transforms the footprint by Shadow_Matrix, then clears world
 // Z before the view projection. A ramp must never lift the shadow off ground.
 if(out.active)for(int n=8;n<12;++n)out.ramp[n]=0;
 return out;
}
}}
