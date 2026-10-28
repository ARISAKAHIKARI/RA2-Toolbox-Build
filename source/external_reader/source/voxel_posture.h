#pragma once
#include "game_view.h"
#include "voxel_lighting.h"
// Current native Drive/Ship posture, sampled externally. The locomotor stores
// ramp indices/timing; its shared live matrices/quaternions define the slope.
// No draw/locomotor methods are called and no cell-height guess is substituted.
namespace external { namespace voxelposture {
using Matrix=std::array<float,12>;
inline Matrix identity(){return {{1,0,0,0,0,1,0,0,0,0,1,0}};}
inline Matrix multiply(const Matrix&a,const Matrix&b){
 Matrix out{};for(int row=0;row<3;++row)for(int col=0;col<4;++col){
  double v=col==3?a[row*4+3]:0;
  for(int k=0;k<3;++k)v+=double(a[row*4+k])*b[k*4+col];
  out[row*4+col]=voxel::nativeStoredFloat(v);
 }return out;
}
inline bool validMatrix(const Matrix&m){
 for(float v:m)if(!std::isfinite(v)||std::abs(v)>4096)return false;
 // Native ramp tables are rotations. Reject uninitialized/all-zero or torn
 // tables instead of flattening a model with an invalid transform.
 for(int row=0;row<3;++row){double n=0;for(int k=0;k<3;++k)n+=double(m[row*4+k])*m[row*4+k];if(n<.95||n>1.05)return false;}
 for(int a=0;a<3;++a)for(int b=a+1;b<3;++b){double d=0;for(int k=0;k<3;++k)d+=double(m[a*4+k])*m[b*4+k];if(std::abs(d)>.025)return false;}
 return true;
}
inline bool validQuaternion(const std::array<float,4>&q){double n=0;for(float v:q){if(!std::isfinite(v))return false;n+=double(v)*v;}return n>.95&&n<1.05;}
// 4CADB0 quantizes asin's input to 1/2048 before deriving acos. Reconstruct
// that mathematical table; never embed the game's original lookup bytes.
inline double nativeAcos(double value){
 constexpr double HalfPi=1.57079632679489661923;
 int index=int((std::clamp(value,-1.,1.)+1.)*-2048.);
 double quantized=std::clamp(-double(index)/2048.-1.,-1.,1.);
 return HalfPi-voxel::nativeStoredFloat(std::asin(quantized));
}
inline float nativeSine(double angle){
 constexpr double Tau=6.28318530717958647692;int n=int(angle*2607.594482421875),index=n/2;
 index%=8192;if(index<0)index+=8192;if((n&1)&&index<8191)++index;
 return voxel::nativeStoredFloat(std::sin(index*(Tau/8192)));
}
inline bool interpolate(const std::array<float,4>&a,const std::array<float,4>&b,double phase,Matrix&out){
 if(!validQuaternion(a)||!validQuaternion(b)||!std::isfinite(phase)||phase<0||phase>1)return false;
 phase=voxel::nativeStoredFloat(phase);double dot=0;for(int i=0;i<4;++i)dot+=double(a[i])*b[i];
 // Vanilla ramps are neighboring rotations in the same quaternion hemisphere.
 // Do not guess a custom table's antipodal fallback.
 if(dot<=-1.+1e-5)return false;
 double first=1.-phase,last=phase;
 if(1.-dot>1e-5){double angle=nativeAcos(dot),sine=nativeSine(angle);if(std::abs(sine)<1e-8)return false;
  first=nativeSine((1.-phase)*angle)/sine;last=nativeSine(phase*angle)/sine;
 }
 std::array<float,4> q{};for(int i=0;i<4;++i)q[i]=voxel::nativeStoredFloat(first*a[i]+last*b[i]);
 double x=q[0],y=q[1],z=q[2],w=q[3];auto f=[](double v){return voxel::nativeStoredFloat(v);};
 out={{f(1-2*(y*y+z*z)),f(2*(x*y-w*z)),f(2*(x*z+w*y)),0,
       f(2*(x*y+w*z)),f(1-2*(x*x+z*z)),f(2*(y*z-w*x)),0,
       f(2*(x*z-w*y)),f(2*(y*z+w*x)),f(1-2*(x*x+y*y)),0}};
 return validMatrix(out);
}
struct State {
 Matrix ramp=identity();float sideways=0,forwards=0,scaleX=1,scaleY=1;
 bool active=false,available=true;
};
inline uint64_t signature(const State&s){
 if(s.available&&!s.active)return 0;
 uint64_t hash=1469598103934665603ull;auto fold=[&](uint32_t value){hash=(hash^value)*1099511628211ull;};
 fold(s.available);fold(s.active);auto number=[&](float value){uint32_t bits=0;std::memcpy(&bits,&value,4);fold(bits);};
 for(float value:s.ramp)number(value);for(float value:{s.sideways,s.forwards,s.scaleX,s.scaleY})number(value);return hash;
}
template<class Read>State capture(GameView<Read>&g,uint32_t object,uint32_t type,int kind,int frame){
 State out;if(kind!=2||object<0x10000||type<0x10000||frame<0)return out;
 uint32_t loco=g.template get<uint32_t>(object+0x674);if(loco<0x10000||loco>0xfffffff0u)return out;
 uint32_t table=g.template get<uint32_t>(loco);
 if(table!=0x7E7EB0&&table!=0x7F2D8C)return out; // verified Drive / Ship ILocomotion
 out.available=false;
 struct Sample {int current,previous,start,pause,duration,rate;};Sample state{},again{};
 std::array<float,2> rocking{},verifyRock{};
 if(loco>0xffffffd0u||g.template get<uint32_t>(loco+8)!=object||!g.bytes(loco+0x18,&state,sizeof state)||
    !g.bytes(object+0x328,rocking.data(),sizeof rocking))return out;
 if(state.current<0||state.current>20||state.previous<0||state.previous>20||state.rate<0||state.rate>32768||
    state.duration<0||state.duration>32768||state.start< -1)return out;
 for(float v:rocking)if(!std::isfinite(v)||std::abs(v)>1.570797f)return out;
 int64_t remaining=state.start==-1?state.duration:std::max<int64_t>(0,int64_t(state.duration)-(int64_t(frame)-state.start));
 double phase=state.rate?std::clamp(double(state.rate-remaining)/state.rate,0.,1.):1.;
 if(phase<1&&state.current!=state.previous){
  std::array<float,4>a{},b{},a2{},b2{};
  uint32_t oldAt=0xB43188+uint32_t(state.previous)*16,newAt=0xB43188+uint32_t(state.current)*16;
  if(!g.bytes(oldAt,a.data(),sizeof a)||!g.bytes(newAt,b.data(),sizeof b)||
     !g.bytes(oldAt,a2.data(),sizeof a2)||!g.bytes(newAt,b2.data(),sizeof b2)||a!=a2||b!=b2||!interpolate(a,b,phase,out.ramp))return out;
 }else if(state.current){
  Matrix verify{};uint32_t at=0xB45188+uint32_t(state.current)*48;
  if(!g.bytes(at,out.ramp.data(),sizeof out.ramp)||!g.bytes(at,verify.data(),sizeof verify)||out.ramp!=verify||!validMatrix(out.ramp))return out;
 }
 if(std::abs(rocking[0])>=1e-5f||std::abs(rocking[1])>=1e-5f){
  std::array<double,2>scale{};if(!g.bytes(type+0x360,scale.data(),sizeof scale))return out;
  for(double v:scale)if(!std::isfinite(v)||v<=0||v>8)return out;
  out.sideways=rocking[0];out.forwards=rocking[1];out.scaleX=voxel::nativeStoredFloat(scale[0]);out.scaleY=voxel::nativeStoredFloat(scale[1]);
 }
 if(!g.bytes(loco+0x18,&again,sizeof again)||std::memcmp(&state,&again,sizeof state)||
    !g.bytes(object+0x328,verifyRock.data(),sizeof verifyRock)||rocking!=verifyRock||
    g.template get<uint32_t>(object+0x674)!=loco||g.template get<uint32_t>(loco)!=table||g.template get<uint32_t>(loco+8)!=object)return out;
 out.available=true;out.active=out.ramp!=identity()||out.sideways!=0||out.forwards!=0;return out;
}
inline Matrix yaw(double angle,bool native){
 auto sc=native?voxel::nativeVoxelSinCos(angle):std::array<float,2>{{float(std::sin(angle)),float(std::cos(angle))}};
 return {{sc[1],-sc[0],0,0,sc[0],sc[1],0,0,0,0,1,0}};
}
// A slope is a world transform, while rocking axes follow the hull facing.
// Conjugate the local rocking matrix so the same world transform also tilts
// the independently facing turret and its hull-relative pivot.
inline Matrix world(const State&s,double previewBody,bool native=true){
 Matrix result=s.ramp;if(s.sideways==0&&s.forwards==0)return result;
 auto side=voxel::nativeVoxelSinCos(s.sideways),forward=voxel::nativeVoxelSinCos(s.forwards);
 int height=int(std::abs(side[0])*s.scaleX+std::abs(forward[0])*s.scaleY);
 int dx=int(s.scaleY-int(double(forward[1])*s.scaleY)),dy=int(s.scaleX-int(double(side[1])*s.scaleX));
 if(s.forwards<0)dx=-dx;if(s.sideways>0)dy=-dy;
 Matrix rock={{1,0,0,float(dx),0,side[1],-side[0],float(dy),0,side[0],side[1],0}};
 Matrix pitch={{forward[1],0,forward[0],0,0,1,0,0,-forward[0],0,forward[1],0}};rock=multiply(rock,pitch);
 auto facing=yaw(-previewBody,native),inverse=identity();
 double norm=double(facing[0])*facing[0]+double(facing[4])*facing[4];
 inverse[0]=float(facing[0]/norm);inverse[1]=float(facing[4]/norm);inverse[4]=float(facing[1]/norm);inverse[5]=float(facing[5]/norm);
 result=multiply(multiply(multiply(result,facing),rock),inverse);result[11]+=height;return result;
}
}}
