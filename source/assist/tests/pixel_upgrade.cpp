#include "../../external_reader/source/pixel_upgrade.h"
#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <limits>
#include <vector>
// Oracle uses the unchanged upstream scaler on one fully assembled backing,
// with edge extension matching an opaque framebuffer. It does not reimplement
// xBRZ's edge rules or depend on the adapter's block/crop sampling code.
static std::vector<uint32_t> upstreamRegion(const external::camera::NativeCanvas&canvas,int width,int height,
                                           double x,double y,double spanX,double spanY){
 const int factor=int(std::ceil(std::max(width/spanX,height/spanY))),halo=3;
 int sw=canvas.width()+2*halo,sh=canvas.height()+2*halo;
 std::vector<uint32_t> source(size_t(sw)*sh),scaled(size_t(sw)*sh*factor*factor),out(size_t(width)*height);
 for(int yy=0;yy<sh;++yy)for(int xx=0;xx<sw;++xx)
  source[size_t(yy)*sw+xx]=canvas.pixel(std::clamp(xx-halo,0,canvas.width()-1),std::clamp(yy-halo,0,canvas.height()-1));
 xbrz::scale(factor,source.data(),scaled.data(),sw,sh,xbrz::ColorFormat::argbUnbuffered);
 const int tw=sw*factor,th=sh*factor;
 for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
  double nx=x+(xx+.5)*spanX/width,ny=y+(yy+.5)*spanY/height;
  if(nx<0||ny<0||nx>=canvas.width()||ny>=canvas.height())continue;
  double px=(nx+halo)*factor-.5,py=(ny+halo)*factor-.5;
  int ix=int(std::floor(px)),iy=int(std::floor(py));double fx=px-ix,fy=py-iy;
  auto at=[&](int a,int b){return scaled[size_t(std::clamp(b,0,th-1))*tw+std::clamp(a,0,tw-1)];};
  uint32_t a=at(ix,iy),b=at(ix+1,iy),c=at(ix,iy+1),d=at(ix+1,iy+1),value=0;
  for(int shift:{0,8,16,24}){double top=((a>>shift)&255)*(1-fx)+((b>>shift)&255)*fx;
   double bottom=((c>>shift)&255)*(1-fx)+((d>>shift)&255)*fx;
   value|=uint32_t(std::floor(top*(1-fy)+bottom*fy+.5))<<shift;
  }out[size_t(yy)*width+xx]=value;
 }return out;
}
static void matchesUpstream(const std::vector<uint32_t>&actual,const external::camera::NativeCanvas&canvas,
                            int width,int height,double x,double y,double spanX,double spanY){
 auto expected=upstreamRegion(canvas,width,height,x,y,spanX,spanY);assert(actual.size()==expected.size());
 for(size_t n=0;n<actual.size();++n)for(int shift:{0,8,16,24})
  assert(std::abs(int((actual[n]>>shift)&255)-int((expected[n]>>shift)&255))<=1);
}
int main(){
 using namespace external::pixelupgrade;
 std::array<uint32_t,256> pal{};pal.fill(0x12345678);
 std::vector<uint8_t> raw(768,0);
 raw[3]=63;raw[4]=31;raw[5]=1;
 assert(decodePalette(raw.data(),raw.size(),pal,PaletteRange::OriginalDAC));
 assert(pal[0]==0 && pal[1]==0xfffc7c04u);
 assert(decodePalette(raw.data(),raw.size(),pal,PaletteRange::FullRGB));
 assert(pal[0]==0&&pal[1]==(0xff000000u|(255u<<16)|(125u<<8)|4u));
 auto prior=pal;raw[10]=64;
 assert(!decodePalette(raw.data(),raw.size(),pal,PaletteRange::FullRGB));
 assert(pal==prior);raw[10]=0;
 assert(!decodePalette(raw.data(),raw.size()-1,pal,PaletteRange::FullRGB));
 // Shape edge: black center, white left and bottom; 2x mixes a new RGB shade.
 const uint32_t W=0xffffffffu, K=0xff000000u;
 std::vector<uint32_t> indexed={W,K,K, W,K,K, W,W,W},out;
 assert(scale2x(indexed,3,3,out,TwoX::Nearest));
 assert(out.size()==36&&out[3*6+2]==K);
 assert(scale2x(indexed,3,3,out,TwoX::EdgeBlend));
 assert(out[3*6+2]==0xff808080u); // lower-left pixel of the center 2x block
 assert(out[2*6+2]==K); // upper-left pixel stays dark
 // Transparent neighbors must never generate a fringe color.
 auto b=block(K,0,0,W,W,TwoX::EdgeBlend);
 for(auto p:b)assert((p>>24)==255u);
 b=block(0,W,W,K,K,TwoX::EdgeBlend);
 for(auto p:b)assert(!p);
 // The native-pixel canvas preserves 1:1 exact colors in every scale mode.
 external::camera::NativeCanvas canvas;
 assert(canvas.reset(3,3));
 canvas.underlayRect(0,0,3,3,0);
 canvas.underlayRect(0,0,1,3,0xffffff); // underlay only fills unpainted cells
 std::vector<uint32_t> normal,unchanged,scaled;
 assert(canvas.presentRegion(3,3,normal,0,0,3,3));
 assert(canvas.presentRegion(3,3,unchanged,0,0,3,3,2));
 assert(normal==unchanged);
 assert(canvas.presentRegion(6,6,scaled,0,0,3,3,2));
 assert(scaled.size()==36);matchesUpstream(scaled,canvas,6,6,0,0,3,3);
 assert(canvas.presentRegion(6,6,unchanged,-1,-1,3,3,2));
 assert(unchanged[0]==0);matchesUpstream(unchanged,canvas,6,6,-1,-1,3,3); // no invented pixel outside the native backing
 assert(!canvas.presentRegion(8192,8192,scaled,0,0,4096,4096,2));
 // Legacy custom helper tests remain as isolated unit coverage. Production
 // modes 2/3 below are checked against upstream xBRZ rather than these rules.
 const uint32_t L=0xff101010u,H=0xfff0f0f0u;
 assert(adaptivePixel(L,H,L,H,.4,.7)==L);
 assert(adaptivePixel(L,H,L,H,.6,.2)==H);
 assert(adaptivePixel(L,L,L,L,.35,.65)==L);
 assert(adaptivePixel(0,H,0,H,.2,.6)==0);
 assert(adaptivePixel(0,H,0,H,.8,.6)==H);
 // A real physical 3840/2560=1.5x case at small but identical ratios.
 external::camera::NativeCanvas fractional;
 assert(fractional.reset(4,4));
 fractional.underlayRect(0,0,1,4,0x101010);
 fractional.underlayRect(1,0,4,4,0xf0f0f0); // source edge is at 1: output x=1.5, not pixel-aligned
 std::vector<uint32_t> restored,original;
 assert(fractional.presentRegion(6,6,restored,0,0,4,4,3));
 assert(fractional.presentRegion(6,6,original,0,0,4,4,0));
 assert(restored.size()==36&&restored[2*6+0]==L&&restored[2*6+5]==H);
 matchesUpstream(restored,fractional,6,6,0,0,4,4); // simple patterns may match the box/nearest result
 for(uint32_t p:restored)assert((p>>24)==255);
 // Upscale decision must leave exact native 1:1 untouched.
 assert(fractional.presentRegion(4,4,original,0,0,4,4,0));
 assert(fractional.presentRegion(4,4,restored,0,0,4,4,3));
 assert(restored==original);
 // Any transparent outside-backing region stays transparent.
 assert(fractional.presentRegion(6,6,restored,-4,-4,4,4,3));
 for(auto p:restored)assert(p==0);
 // Nearest uses the actual pixel centers at fractional and integer ratios.
 // Each source column is distinguishable, so a hidden fixed-2x intermediate
 // or box-filter fallback changes this expected 1.5x/3x/4x placement.
 external::camera::NativeCanvas gradient;assert(gradient.reset(4,4));
 const uint32_t shades[]={0xff202020u,0xff282828u,0xff303030u,0xff383838u};
 for(int x=0;x<4;++x)gradient.underlayRect(x,0,x+1,4,shades[x]);
 const std::vector<int> columns15={0,1,1,2,3,3};
 for(int target:{6,12,16}){
  const double ratio=target/4.;std::vector<uint32_t> nearest,adaptive,edge;
  assert(gradient.presentRegion(target,target,nearest,0,0,4,4,1));
  for(int y=0;y<target;++y)for(int x=0;x<target;++x){int source=target==6?columns15[x]:x/int(ratio);assert(nearest[size_t(y)*target+x]==shades[source]);}
  assert(gradient.presentRegion(target,target,adaptive,0,0,4,4,3));
  assert(gradient.presentRegion(target,target,edge,0,0,4,4,2));assert(edge==adaptive);
  // A mature pixel-art scaler may preserve a step unchanged. Validate its
  // identity against upstream rather than requiring invented intermediate RGB.
  matchesUpstream(adaptive,gradient,target,target,0,0,4,4);
  // A source alpha cutout remains a hard geometric boundary at every ratio.
  assert(fractional.presentRegion(target,target,adaptive,-1,0,4,4,3));
  assert(fractional.presentRegion(target,target,nearest,-1,0,4,4,1));
  for(size_t at=0;at<adaptive.size();++at){assert((adaptive[at]>>24)==(nearest[at]>>24));if(!(adaptive[at]>>24))assert(adaptive[at]==0);}
 }
 // Independently scaled axes still select the mature scaler even when one
 // axis remains native. Exact physical crop placement is part of the oracle.
 for(unsigned mode:{2u,3u}){assert(fractional.presentRegion(6,4,restored,0,0,4,4,mode));matchesUpstream(restored,fractional,6,4,0,0,4,4);}
 // Even a small physical upscale must honor the selected sampling algorithm.
 external::camera::NativeCanvas slight;assert(slight.reset(1001,1));slight.underlayRect(0,0,1,1,0x101010);slight.underlayRect(1,0,1001,1,0xf0f0f0);
 assert(slight.presentRegion(1001,1,restored,.5,0,1000,1,3));matchesUpstream(restored,slight,1001,1,.5,0,1000,1);
 // Exact 2x with an odd target is a fractional source crop. Both saved
 // reconstruction mode indices use xBRZ and preserve that crop alignment.
 assert(gradient.presentRegion(5,5,restored,0,0,2.5,2.5,2));
 assert(gradient.presentRegion(5,5,original,0,0,2.5,2.5,3));assert(restored==original&&restored.size()==25);matchesUpstream(restored,gradient,5,5,0,0,2.5,2.5);
 // At the maximum output axis, prefetch only centers actually sampled.
 // A half-pixel crop spans 8193 cells but needs exactly 8192 source centers.
 external::camera::NativeCanvas maximum;assert(maximum.reset(8192,2));maximum.underlayRect(0,0,8192,2,0x223344);
 assert(maximum.presentRegion(8192,1,restored,.5,0,8192,.5,1));assert(restored.front()==0xff223344&&restored.back()==0);
 // A terrain cache can be shared by a COW fork while each painted revision
 // owns its colors. Mutating either copy must detach both page and xBRZ cache.
 external::camera::NativeCanvas revision;assert(revision.reset(8,8,{},0xff102030));
 revision.underlayRect(0,0,8,8,0x102030);std::vector<uint32_t> before,after,forkBefore,forkAfter;
 assert(revision.presentRegion(16,16,before,0,0,8,8,3));auto frozen=revision.fork();
 external::voxel::Image patch;patch.width=patch.height=3;patch.pixels.assign(9,0xffc03020);patch.depth.assign(9,0);
 external::lighting::State daylight;revision.blit(patch,2,2,1,1,10,daylight);
 assert(revision.presentRegion(16,16,after,0,0,8,8,3));assert(after!=before);matchesUpstream(after,revision,16,16,0,0,8,8);
 assert(frozen.presentRegion(16,16,forkBefore,0,0,8,8,3));assert(forkBefore==before);
 patch.pixels.assign(9,0xff2050d0);frozen.blit(patch,5,5,1,1,20,daylight);
 assert(frozen.presentRegion(16,16,forkAfter,0,0,8,8,3));assert(forkAfter!=forkBefore);matchesUpstream(forkAfter,frozen,16,16,0,0,8,8);
 assert(revision.presentRegion(16,16,restored,0,0,8,8,3));assert(restored==after);
 // Reset invalidates the old revision. Precision changes preserve existing
 // pixels while later paint uses the selected quantization and a fresh cache.
 assert(revision.reset(8,8,{},0xff403020));assert(revision.presentRegion(16,16,restored,0,0,8,8,3));
 assert(restored!=after);matchesUpstream(restored,revision,16,16,0,0,8,8);
 revision.precision(external::colorprecision::Mode::SmoothRGB);
 assert(revision.presentRegion(16,16,after,0,0,8,8,3));assert(after==restored);
 patch.pixels.assign(9,0xff020202);daylight.intensity=333;revision.blit(patch,1,1,1,1,30,daylight);
 assert(revision.pixel(1,1)==0xff010101);assert(revision.presentRegion(16,16,restored,0,0,8,8,3));matchesUpstream(restored,revision,16,16,0,0,8,8);
 // reject invalid aspect/sampling envelopes.
 assert(!sampleAdaptive([](int,int){return 0xff223344u;},0,0,4,4,3,6,restored));
 assert(!sampleAdaptive([](int,int){return 0xff223344u;},0,0,4,4,17,16,restored));
 assert(!sampleNearest([](int,int){return 0xff223344u;},0,0,std::numeric_limits<double>::infinity(),4,6,6,restored));
 std::puts("PASS: atomic DAC/RGB888 palette and legacy helper units; production xBRZ upstream identity, 1.5x/3x/4x nearest placement, crop/native-axis/odd/maximum-axis/alpha guards and terrain COW/reset/precision invalidation");
}
