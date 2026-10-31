#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "native_packed_color.h"
#include "native_line_cubic.h"
namespace external { namespace nativefx {
inline int32_t chop(double value){if(!std::isfinite(value)||value>=9223372036854775808.||value< -9223372036854775808.)return 0;return int32_t(uint64_t(int64_t(value)));}
struct Math {
 std::array<float,10241>trig{};std::array<float,4097>atan{},asin{};std::array<uint32_t,16384>sqrt{};
 unsigned octant(int fromX,int fromY,int toX,int toY)const{float x=float(double(toX)-fromX),y=float(double(fromY)-toY);float half;uint32_t bits=0x3fc90fdb;std::memcpy(&half,&bits,4);double angle=0;
  if(x==0)angle=y==0?0:y>0?half:-half;else{bits=0x3cc7fe84;float step;std::memcpy(&step,&bits,4);uint32_t index=uint32_t(std::abs(chop((double(y)/x)/step)));angle=index<atan.size()?atan[index]:half;if(x<0)angle=3.141592653589793238462643383279502884-angle;if(y<0)angle=-angle;}
  uint64_t scaleBits=0xc0c45f07af68ecefull;double scale;std::memcpy(&scale,&scaleBits,8);uint16_t facing=uint16_t(chop((angle-1.570796326794896619231321691639751442)*scale));return (((facing>>12)+1)>>1)&7;
 }
 double sine(double radians)const{int raw=chop(radians*2607.594482421875),index=(raw/2)%8192;if(index<0)index+=8192;if((raw&1)&&index<8191)++index;return trig[unsigned(index)];}
 double cosine(double radians)const{int raw=chop(radians*2607.594482421875),index=(raw/2)%8192;if(index<0)index+=10240;else index+=2048;if((raw&1)&&index<10239)++index;return trig[unsigned(index)];}
 double arcSine(double ratio)const{int index=-chop((ratio+1.)*-2048.);return index>=0&&index<int(asin.size())?asin[unsigned(index)]:0.;}
 double rootWide(double squared)const{if(!(squared>0)||!std::isfinite(squared))return 0;float f=float(squared);if(double(f)>squared)f=std::nextafter(f,0.f);uint32_t bits;std::memcpy(&bits,&f,4);int exponent=int((bits>>23)&255)-127;if(exponent< -126||exponent>126)return 0;uint32_t mantissa=(bits&0x7fffffu)|((exponent&1)?0x800000u:0u),result=sqrt[mantissa>>10]+(uint32_t((exponent>=0?exponent/2:(exponent-1)/2)+127)<<23);std::memcpy(&f,&result,4);return f;}
 double root(int32_t squared)const{
  uint32_t value=squared<0?0u-uint32_t(squared):uint32_t(squared);if(!value)return 0;
  int exponent=31;while(!(value&(1u<<exponent)))--exponent;
  uint32_t bits=(uint32_t(exponent+127)<<23)|(exponent>23?(value>>(exponent-23))&0x7fffffu:(value<<(23-exponent))&0x7fffffu);
  uint32_t mantissa=(bits&0x7fffffu)|((exponent&1)?0x800000u:0u),result=sqrt[mantissa>>10]+(uint32_t((exponent>=0?exponent/2:(exponent-1)/2)+127)<<23);float out;std::memcpy(&out,&result,4);return out;
 }
};
template<class Read>std::shared_ptr<const Math> math(Read read){static thread_local std::weak_ptr<const Math>cache;if(auto current=cache.lock())return current;
 auto out=std::make_shared<Math>();if(!read(0x859094,out->asin.data(),sizeof out->asin)||!read(0x8610B4,out->atan.data(),sizeof out->atan)||!read(0x84F084,out->trig.data(),sizeof out->trig)||!read(0x8650BC,out->sqrt.data(),sizeof out->sqrt))return {};
 if(out->atan[0]!=0||out->atan[1]<.024f||out->atan[4096]>1.571f||out->trig[0]!=0||out->trig[2048]<.999f||out->sqrt[0]!=0)return {};for(float f:out->trig)if(!std::isfinite(f)||std::abs(f)>1.001f)return {};cache=out;return out;
}
struct Spans {int top=0;std::vector<std::array<int,2>>rows;};
template<class Emit>void edge(std::array<int,2>a,std::array<int,2>b,bool skip,Emit emit){int dx=b[0]-a[0],dy=b[1]-a[1];if(dy<=0)return;int width=std::abs(dx),step=dx>0?1:-1,x=a[0];
 if(width==0){for(int n=0;n<dy-int(skip);++n)emit(x);return;}if(width==dy){if(skip)x+=step;for(int n=0;n<dy-int(skip);++n){emit(x);x+=step;}return;}
 int advance=width<dy?0:width/dy*step,remainder=width%dy,error=dx<0?1-dy:0;auto next=[&]{error+=remainder;x+=advance;if(error>0){x+=step;error-=dy;}};if(skip)next();for(int n=0;n<dy-int(skip);++n){emit(x);next();}}
inline Spans polygon(const std::vector<std::array<int,2>>&p){Spans out;if(p.size()<3||p.size()>6)return out;size_t low=0,high=0;for(size_t n=1;n<p.size();++n){if(p[n][1]<p[low][1])low=n;else if(p[n][1]>p[high][1])high=n;}
 int minY=p[low][1],maxY=p[high][1];if(minY==maxY||maxY-minY>16384)return out;auto next=[&](size_t n){return (n+1)%p.size();};auto previous=[&](size_t n){return (n+p.size()-1)%p.size();};
 size_t after=low,before=low;while(p[after][1]==minY)after=next(after);size_t right=previous(after);while(p[before][1]==minY)before=previous(before);size_t left=next(before);
 bool include=p[left][0]!=p[right][0];int direction=-1;bool swap=include?p[right][0]<p[left][0]:int64_t(p[previous(left)][1]-p[left][1])*(p[next(right)][0]-p[left][0])-int64_t(p[next(right)][1]-p[left][1])*(p[previous(left)][0]-p[left][0])<0;
 if(swap){std::swap(left,right);direction=1;}int count=maxY-minY-1+int(include);if(count<=0)return out;out.top=minY-int(include)+1;out.rows.resize(size_t(count));
 for(int side=0;side<2;++side){size_t index=side?right:left;int step=side?-direction:direction;size_t row=0;bool skip=!include;
  for(size_t guard=0;guard<p.size();++guard){size_t following=step>0?next(index):previous(index);auto a=p[index],b=p[following];if(side){--a[0];--b[0];}bool overflow=false;edge(a,b,skip,[&](int x){if(row<out.rows.size())out.rows[row++][side]=x;else overflow=true;});if(overflow)return {};index=following;skip=false;if(index==high)break;}
  if(row!=out.rows.size())return {};
 }return out;
}
// All tables were initialized by75F020. Read their committed immutable data;
// never regenerate them with host sin/sqrt or advance a Wave's state.
template<class Read>std::shared_ptr<const std::vector<uint8_t>> waveTables(Read read){static thread_local std::weak_ptr<const std::vector<uint8_t>>cache;if(auto current=cache.lock())return current;
 uint8_t ready=0;if(!read(0xB725CC,&ready,1)||!ready)return {};auto out=std::make_shared<std::vector<uint8_t>>(180000+2004+52);
 if(!read(0xB4669C,out->data(),180000)||!read(0xB45E68,out->data()+180000,2004)||!read(0xB46648,out->data()+182004,52))return {};uint16_t first[2]{};std::memcpy(first,out->data(),4);if(first[0]||first[1]!=1)return {};cache=out;return out;
}
inline bool waveLookup(const std::vector<uint8_t>&tables,int dx,int dy,int phase,bool magnetic,int&multiple,int&strength){int64_t index=int64_t(std::abs(dy))*300+std::abs(dx);if(tables.size()!=182056||index<0||index>=90000)return false;uint16_t distance=0;std::memcpy(&distance,tables.data()+index*2,2);int sineIndex=int(distance)+phase;if(sineIndex<0||sineIndex>=(magnetic?1002:500))return false;int16_t sine=0;std::memcpy(&sine,tables.data()+180000+(sineIndex+(magnetic?0:502))*2,2);int magnitude=std::abs(int(sine));if(magnetic)magnitude=std::min(magnitude,9);if(magnitude>12)return false;multiple=magnitude<=1?0:magnitude<=6?1:magnitude<=9?2:3;std::memcpy(&strength,tables.data()+182004+magnitude*4,4);return true;}
inline bool clip(std::array<int,2>&a,std::array<int,2>&b,int width,int height){if(width<=0||height<=0)return false;std::array<double,2>p{{double(a[0]),double(a[1])}},q{{double(b[0]),double(b[1])}};double xy=(q[0]-p[0])/(q[1]-p[1]),yx=(q[1]-p[1])/(q[0]-p[0]);auto code=[&](auto v){return (v[0]<0?1:v[0]>=width?2:0)|(v[1]<0?8:v[1]>=height?4:0);};
 for(int guard=0;guard<16;++guard){int pc=code(p),qc=code(q);if(!(pc|qc)){a={{chop(p[0]),chop(p[1])}};b={{chop(q[0]),chop(q[1])}};return true;}if(pc&qc)return false;int c=pc?pc:qc;std::array<double,2>v;
  if(c&8)v={{-p[1]*xy+p[0],0}};else if(c&4)v={{(height-1-p[1])*xy+p[0],double(height-1)}};else if(c&2)v={{double(width-1),(width-1-p[0])*yx+p[1]}};else v={{0,-p[0]*yx+p[1]}};if(c==pc)p=v;else q=v;
 }return false;
}
struct LinePixel {int x=0,y=0,gateX=0;uint16_t z=0;};
inline bool clipped(std::array<int,2>&a,std::array<int,2>&b,int&za,int&zb,int width,int height,const Math*math){
 if(a[0]>b[0]){std::swap(a,b);std::swap(za,zb);}auto originalA=a,originalB=b;if(!clip(a,b,width,height))return false;
 auto distance=[&](auto p,auto q){int64_t dx=int64_t(p[0])-q[0],dy=int64_t(p[1])-q[1];uint64_t squared=uint64_t(dx*dx+dy*dy);return math?int(math->root(int32_t(uint32_t(squared)))):int(std::sqrt(double(squared)));};
 if(a!=originalA||b!=originalB){int full=distance(originalA,originalB),originalZa=za,originalZb=zb;if(full){if(b!=originalB){int delta=chop(double(distance(originalA,b))/full*(int64_t(originalZa)-originalZb));zb=originalZa<originalZb?originalZa+std::abs(delta):originalZa-std::abs(delta);}if(a!=originalA){int delta=chop(double(distance(a,originalB))/full*(int64_t(originalZa)-originalZb));za=originalZb>originalZa?originalZb-std::abs(delta):originalZb+std::abs(delta);}}}
 return true;
}
template<class Emit>bool line(std::array<int,2>a,std::array<int,2>b,int za,int zb,int width,int height,const Math*math,Emit emit){
 if(!clipped(a,b,za,zb,width,height,math))return true;
 int dx=b[0]-a[0],dy=std::abs(b[1]-a[1]),dz=std::abs(zb-za),ys=b[1]<a[1]?-1:1,zs=zb<za?-1:1,dominant=dz>dx&&dz>dy?2:dx>dy?0:1;int lengths[3]={dx,dy,dz},count=lengths[dominant];if(count>65536)return false;int errors[3]={-count,-count,-count};auto point=a;int adjustment=za;
 for(int n=0;n<count;++n){for(int axis=0;axis<3;++axis)if(axis!=dominant)errors[axis]+=2*lengths[axis];emit(LinePixel{point[0],point[1],a[0],uint16_t(32768-point[1]+adjustment)});
  for(int axis=0;axis<3;++axis)if(axis==dominant||errors[axis]>0){if(axis==0)++point[0];else if(axis==1)point[1]+=ys;else adjustment+=zs;if(axis!=dominant)errors[axis]-=2*count;}
 }return true;
}
struct AAPair {std::array<int,2>primary{},neighbor{};uint16_t z=0;std::array<int,2>source{},destination{};bool swap=false;};
template<class Emit>bool antialiased(std::array<int,2>a,std::array<int,2>b,int za,int zb,int width,int height,float intensity,const Math*math,Emit emit){
 if(width<3||height<3||!clipped(a,b,za,zb,width-2,height-2,math))return true;++a[0];++a[1];++b[0];++b[1];
 int dx=b[0]-a[0],dy=std::abs(b[1]-a[1]),dz=std::abs(zb-za),ys=b[1]<a[1]?-1:1,zs=zb<za?-1:1;bool horizontal=dx>dy,steepZ=dz>dx&&dz>dy;int major=horizontal?dx:dy,minor=horizontal?dy:dx;if(!major)return true;if(major>65536)return false;
 int step=int(int64_t(minor)*256/major),coverage[2]={256,0},error=2*dz-major,adjust=za;auto p=a;
 for(int n=0;n<major;++n){AAPair pair;pair.primary=p;pair.neighbor=horizontal?std::array<int,2>{{p[0],p[1]+ys}}:std::array<int,2>{{p[0]+1,p[1]}};pair.z=uint16_t(32768-p[1]-1+adjust);pair.swap=horizontal;
  for(unsigned k=0;k<2;++k){int index=std::clamp(chop(double(coverage[k])*intensity),0,256);pair.source[k]=Cubic[index];pair.destination[k]=Cubic[256-index];}emit(pair);
  coverage[0]-=step;coverage[1]+=step;if(coverage[1]>256){coverage[0]+=256;coverage[1]-=256;if(horizontal)p[1]+=ys;else ++p[0];}
  if(horizontal)++p[0];else p[1]+=ys;
  if(steepZ)adjust+=zs;else{if(error>0){adjust+=zs;error-=2*major;}error+=2*dz;}
 }return true;
}
inline uint32_t packWide(const std::array<unsigned,3>&c,bool f){return ((c[0]>>3)<<(f?10:11))|((c[1]>>(f?3:2))<<5)|(c[2]>>3);}
inline std::array<unsigned,3> unpackWord(uint32_t word,bool f){return {{((word>>(f?10:11))&31u)<<3,((word>>5)&(f?31u:63u))<<(f?3:2),(word&31u)<<3}};}
inline uint32_t aaColor(uint32_t old,uint32_t rgb,unsigned alpha,int source,int destination,bool format){auto dst=unpackWord(nativepacked::word(old,format),format);std::array<unsigned,3>out{};for(unsigned c=0;c<3;++c){unsigned src=(rgb>>(16-c*8))&255u;out[c]=std::min(255u,((dst[c]*unsigned(destination)+src*unsigned(source))*alpha)>>15);}return nativepacked::color(packWide(out,format)&65535u,0xff000000u,format);}
inline uint32_t lineColor(uint32_t destination,uint32_t rgb,unsigned alpha,unsigned mode,unsigned strength,bool format){
 if(!alpha&&mode!=3)return destination;auto dst=unpackWord(nativepacked::word(destination,format),format);std::array<unsigned,3>src{{(rgb>>16)&255u,(rgb>>8)&255u,rgb&255u}},out{};
 if(mode==3){for(unsigned n=0;n<3;++n){int32_t delta=int32_t(uint32_t(dst[n])*strength)>>8;out[n]=uint32_t(std::min(int32_t(dst[n])+delta,255));}}
 else if(mode==2){uint32_t word=nativepacked::word(rgb,format);if(alpha==127)return nativepacked::color(word,0xff000000u,format);out=unpackWord(word,format);for(auto&v:out)v=v*alpha>>7;}
 else if(mode==1){unsigned count=0,excess=0;for(unsigned n=0;n<3;++n){out[n]=dst[n]+(src[n]*alpha>>7);if(out[n]>=255){++count;excess+=out[n]-255;}}if(count==1)excess>>=1;if(count&&count<3)for(auto&v:out)if(v<255)v+=excess;for(auto&v:out)v=std::min(v,255u);}
 else{for(unsigned n=0;n<3;++n)out[n]=(((dst[n]*(256-strength))>>8)+((src[n]*strength)>>8))*alpha>>7;}
 return nativepacked::color(packWide(out,format)&65535u,0xff000000u,format);
}
inline uint32_t waveColor(uint32_t source,unsigned kind,int strength,bool format){auto c=unpackWord(nativepacked::word(source,format),format);
 if(kind==0){c[1]=std::min(255u,c[1]+((unsigned(strength)*c[1])>>8));c[2]=std::min(255u,c[2]+((unsigned(strength)*c[2])>>8));}
 else if(kind==3){c[0]=std::min(255u,c[0]+((unsigned(strength/2)*c[0])>>8));c[2]=std::min(255u,c[2]+((unsigned(strength)*c[2]*4)>>8));}
 else{c[0]=std::min(255u,c[0]+unsigned(std::max(0,strength*64)>>8));c[2]=std::min(255u,c[2]+unsigned(std::max(0,strength*96)>>8));}
 return nativepacked::color(packWide(c,format)&65535u,0xff000000u,format);
}
}}
