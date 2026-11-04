#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_raster.h"
#include RA2_REFERENCE_RASTER_HEADER
#include <cassert>
#include <cstdio>
#include <chrono>
using namespace external;
static size_t checks=0;
template<class T>static bool bytesEqual(const std::vector<T>&a,const std::vector<T>&b){return a.size()==b.size()&&(!a.size()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);}
static void equal(const camera::Surface&a,const camera_reference::Surface&b){
 assert(a.width()==b.width()&&a.height()==b.height()&&a.failed()==b.failed());
 assert(bytesEqual(a.color,b.color)&&bytesEqual(a.depth,b.depth)&&bytesEqual(a.nativeZ,b.nativeZ));
 for(size_t n=0;n<a.color.size();++n)assert(a.alphaAt(n)==b.alphaAt(n));++checks;
}
template<class S>static void initialize(S&s,unsigned flags,int w=79,int h=61){
 assert(s.reset(w,h,0x92334961));s.nativeView(9,1,1,w,h+6,3);s.outputRowOrigin(2);s.writeRows(2,h-2);
 auto alpha=std::make_shared<std::vector<uint8_t>>(s.color.size());
 for(size_t n=0;n<alpha->size();++n){(*alpha)[n]=uint8_t(n*17+flags);s.nativeZ[n]=uint16_t(32640+n%180);s.depth[n]=float(int(n%73)-19);}s.alpha(alpha);
 s.nativeRead([](int x,int y){return 0xff000000u|((unsigned(x)*17039+unsigned(y)*719)&0xffffff);});
}
static void parity(){
 const float scales[]={.25f,.5f,.9375f,1.f,1.0001f,1.25f,2.f,3.f};
 for(unsigned flags=0;flags<1024;++flags){
  camera::Surface a;camera_reference::Surface b;initialize(a,flags);initialize(b,flags);
  const auto precision=(flags&512)?colorprecision::Mode::SmoothRGB:colorprecision::Mode::NativeSteps;a.precision(precision);b.precision(precision);
  voxel::Image image;image.width=37;image.height=29;image.x=-5;image.y=-3;image.rasterScale=(flags&512)?2:1;
  image.flat=flags&1;image.skipZ=flags&2;image.writeDepth=!(flags&4);image.objectPalette=flags&8;
  image.nativeDepth.enabled=!(flags&512);image.nativeDepth.gradient=nativesurface::Gradient(flags%4);
  image.nativeDepth.tileEqual=flags&16;image.nativeDepth.voxel=flags&32;image.nativeDepth.shape=flags&64;
  image.nativeDepth.clipTop=(flags&1)?3:0;image.nativeDepth.clipBottom=(flags&2)?21:999;image.nativeDepth.adjust=int(flags%17)-8;
  image.depthBias=.75f;image.depthBottom=(flags&64)?39:std::numeric_limits<int>::min();
  const size_t count=size_t(image.width)*image.height;image.pixels.resize(count);image.indices.resize(count);
  for(size_t n=0;n<count;++n){unsigned opacity=n%11<4?0:n%7==0?91:255;image.pixels[n]=(opacity<<24)|((uint32_t(n)*0x170b03)&0x3f3f3f);image.indices[n]=uint8_t(n*19);}
  if(flags&128){image.depth.resize(count);for(size_t n=0;n<count;++n)image.depth[n]=n%13?float(int(n%23)-11):std::numeric_limits<float>::quiet_NaN();}
  if(flags&256){image.nativeShape.resize(count);image.nativeShapeCoverage.resize(count);for(size_t n=0;n<count;++n){image.nativeShape[n]=int(n%39)-19;image.nativeShapeCoverage[n]=n%5!=0;}}
  lighting::State light;light.intensity=int(flags*13%2300)-100;light.tint={{1327,701,1999}};light.rgb555=flags&4;light.scalar=flags&32;light.paletteRows=(flags&64)?1:53;
  if(flags&8){auto table=std::make_shared<std::vector<uint32_t>>(light.paletteRows*256);for(size_t n=0;n<table->size();++n)(*table)[n]=uint16_t(n*137+101);light.table=table;}
  for(unsigned mode=0;mode<12;++mode){auto draw=image;camera::Material mat;mat.nativeZAdjust=-7;mat.clipBottom=(flags&16)?19.25f:1e7f;mat.bridgeSplit=flags&64;mat.shpFlat=flags&32;
   if(mode==2)draw.shadow=true;
   if(mode==3){draw.nativeBlend=2;draw.blendSource=std::make_shared<const std::vector<uint32_t>>(draw.pixels);}
   if(mode==4)mat.translucency=2;
   if(mode==5)mat.mixStrength=127;
   if(mode==6){mat.displace=true;mat.cloakWords=-3;draw.nativeDepth.voxel=true;}
   if(mode==7){auto part=image;part.x+=4;part.y+=3;draw.attachments.push_back(part);part.shadow=true;part.x-=5;draw.attachments.push_back(std::move(part));}
   if(mode==8)draw.nativeShapeCoverage.resize(count-1); // Invalid coverage remains rejected.
   if(mode==9)draw.nativeDepth.enabled=true;
   float scale=mode<2?1.f:scales[(flags+mode)%8];bool smooth=mode>=10;
   float ax=mode==0?20.f:20.25f+float(flags%3)/256,ay=mode==0?17.f:17.75f;
   if(mode==11){ax=-7.125f;ay=-5.375f;}
   // Unlike the old digest fixture, pass the material we actually vary.
   a.blit(draw,ax,ay,scale,scale*.875f+(mode<2?.125f:0),31.5f,light,1.25f,3.f,smooth,mat);
   b.blit(draw,ax,ay,scale,scale*.875f+(mode<2?.125f:0),31.5f,light,1.25f,3.f,smooth,mat);equal(a,b);
  }
 }
}
static void boundaries(){
 // Exercise signed native candidates outside uint16, bridge/clip row walks,
 // tiny/subpixel source coverage, worker row origins and asymmetric scale.
 for(int origin:{-65539,-32769,-1,0,32767,65539})for(int raster:{1,2,4})for(bool smooth:{false,true}){
  camera::Surface a;camera_reference::Surface b;initialize(a,17,83,69);initialize(b,17,83,69);
  a.nativeView(origin,.9375f,1.25f,512,65540);b.nativeView(origin,.9375f,1.25f,512,65540);
  voxel::Image image;image.width=43;image.height=37;image.x=-17;image.y=-13;image.rasterScale=raster;image.nativeDepth.enabled=true;image.nativeDepth.voxel=true;
  image.pixels.assign(43*37,0xffbbaa99);image.indices.assign(43*37,244);image.nativeShape.resize(43*37);
  for(size_t n=0;n<image.nativeShape.size();++n)image.nativeShape[n]=int(n%13)-6;
  lighting::State light;camera::Material mat;mat.bridgeSplit=true;mat.clipBottom=18.125f;
  for(int gradient=0;gradient<4;++gradient)for(float scale:{.03125f,.49999f,1.00001f,7.999f}){
   image.nativeDepth.gradient=nativesurface::Gradient(gradient);image.nativeDepth.tileEqual=gradient&1;image.nativeDepth.adjust=origin;
   a.blit(image,31.49999f,29.50001f,scale,scale*.75f,19.75f,light,3.25f,0,smooth,mat);
   b.blit(image,31.49999f,29.50001f,scale,scale*.75f,19.75f,light,3.25f,0,smooth,mat);equal(a,b);
  }
 }
}
struct Shp {
 std::vector<uint8_t> bytes;std::array<uint32_t,256> palette{};
 Shp(){const int w=64,h=56,frames=8;bytes.resize(8+frames*24);put16(2,w);put16(4,h);put16(6,frames);
  for(unsigned n=0;n<256;++n)palette[n]=0xff000000u|((n*37%256)<<16)|((n*73%256)<<8)|(n*109%256);
  for(int f=0;f<frames;++f){const size_t at=8+f*24;put16(at+4,w);put16(at+6,h);put32(at+8,2);put32(at+20,uint32_t(bytes.size()));
   for(int y=0;y<h;++y){const size_t start=bytes.size();bytes.resize(start+2);for(int x=0;x<w;){const bool opaque=((x-32)*(x-32)+(y-28)*(y-28)<700)&&((x+y+f)%13>2);if(opaque){bytes.push_back(uint8_t(1+(x*3+y*7+f*17)%254));++x;}else{int n=1;while(x+n<w&&n<255&&(!(((x+n-32)*(x+n-32)+(y-28)*(y-28)<700)&&((x+n+y+f)%13>2))))++n;bytes.push_back(0);bytes.push_back(uint8_t(n));x+=n;}}put16(start,unsigned(bytes.size()-start));}
  }
 }
 void put16(size_t at,unsigned v){bytes[at]=uint8_t(v);bytes[at+1]=uint8_t(v>>8);}void put32(size_t at,unsigned v){put16(at,v);put16(at+2,v>>16);}
 bool operator()(uint32_t at,void*out,size_t n)const{if(at<0x10000||uint64_t(at-0x10000)+n>bytes.size())return false;std::memcpy(out,bytes.data()+at-0x10000,n);return true;}
 voxel::Image decode(int frame)const{voxel::Image out;auto read=[this](uint32_t at,void*out,size_t n){return (*this)(at,out,n);};assert(voxel::sprite(read,0x10000,palette,out,frame,nullptr,255));out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Vertical;out.objectPalette=true;return out;}
};
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
template<class S>static double animate(S&s,const std::array<voxel::Image,8>&images,float scale,bool smooth,int sequence){
 double elapsed=0;
 for(int frame=0;frame<12;++frame){assert(s.reset(1280,720,0xff304830));auto alpha=std::make_shared<std::vector<uint8_t>>(s.color.size());for(size_t n=0;n<alpha->size();++n)(*alpha)[n]=uint8_t(90+(n+frame)%90);s.alpha(alpha);
  lighting::State light;light.intensity=700+(frame+sequence)%5*173;light.tint={{1100,830,970}};
  const double start=now();
  for(int unit=0;unit<160;++unit){const auto&image=images[(unit+frame+sequence)%8];float x=35+(unit%20)*62+((frame*3+unit)%7)*.25f,y=45+(unit/20)*82+((frame+unit)%9)*.25f;s.blit(image,x,y,scale,scale,float(unit),light,1,0,smooth);}
  elapsed+=now()-start;
 }
 return elapsed/12;
}
static double median(std::vector<double>v){std::sort(v.begin(),v.end());return v[v.size()/2];}
int main(){xbrzadapter::gpuPresentation(false);parity();boundaries();Shp shp;std::array<voxel::Image,8>images;for(int i=0;i<8;++i)images[i]=shp.decode(i);
 double start=now();size_t decoded=0;for(int n=0;n<256;++n)decoded+=shp.decode(n%8).pixels.size();double decodeMs=(now()-start)/256;
 std::printf("parity ordered_draws=%zu planes=RGBA,floatZ_bits,nativeZ,A decode_pixels=%zu decode_mean_ms=%.6f\n",checks,decoded,decodeMs);
 for(auto config:std::array<std::pair<float,bool>,4>{{{1.f,false},{.75f,false},{1.5f,false},{1.5f,true}}}){
  camera::Surface a;camera_reference::Surface b;animate(a,images,config.first,config.second,0);animate(b,images,config.first,config.second,0);equal(a,b);
  std::vector<double> current,prior;for(int trial=0;trial<5;++trial){if(trial%2){prior.push_back(animate(b,images,config.first,config.second,trial));current.push_back(animate(a,images,config.first,config.second,trial));}else{current.push_back(animate(a,images,config.first,config.second,trial));prior.push_back(animate(b,images,config.first,config.second,trial));}equal(a,b);}
  std::printf("dynamic_shp scale=%.2f smooth=%u units=160 frames_per_trial=12 trials=5 before_ms=%.6f after_ms=%.6f ratio=%.4f\n",config.first,unsigned(config.second),median(prior),median(current),median(current)/median(prior));
 }
 std::printf("PASS all complete planes equal; every timed frame submits all 160 changing SHP poses/lights; reset/decode excluded from blit timings.\n");
}
