#include "../../external_reader/source/pixel_upgrade.h"
#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <limits>
#include <vector>
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
 assert(scaled.size()==36);
 assert(canvas.presentRegion(6,6,unchanged,-1,-1,3,3,2));
 assert(unchanged[0]==0); // no invented pixel outside the native backing
 assert(!canvas.presentRegion(8192,8192,scaled,0,0,4096,4096,2));
 // Fractional 1.5x interpolation: a hard vertical silhouette must not
 // blur across the edge, while uniform shading remains constant.
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
 assert(restored!=original); // original box filter softens a hard edge
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
  // Low-contrast shading interpolates into RGB values absent from the source
  // palette. The expected center sample comes from the actual physical ratio.
  const int probe=target==6?1:target==12?3:4;
  const uint32_t expected=target==6?0xff242424u:0xff252525u;
  assert(adaptive[size_t(target/2)*target+probe]==expected);
  assert(adaptive!=nearest);
  // A source alpha cutout remains a hard geometric boundary at every ratio.
  assert(fractional.presentRegion(target,target,adaptive,-1,0,4,4,3));
  assert(fractional.presentRegion(target,target,nearest,-1,0,4,4,1));
  for(size_t at=0;at<adaptive.size();++at){assert((adaptive[at]>>24)==(nearest[at]>>24));if(!(adaptive[at]>>24))assert(adaptive[at]==0);}
 }
 // Independently scaled axes still use adaptive sampling; a native Y axis
 // must not send a 1.5x X edge through the softening box-filter fallback.
 for(unsigned mode:{2u,3u}){assert(fractional.presentRegion(6,4,restored,0,0,4,4,mode));assert(restored[2*6+1]==H);}
 // Even a small physical upscale must honor the selected sampling algorithm.
 external::camera::NativeCanvas slight;assert(slight.reset(1001,1));slight.underlayRect(0,0,1,1,0x101010);slight.underlayRect(1,0,1001,1,0xf0f0f0);
 assert(slight.presentRegion(1001,1,restored,.5,0,1000,1,3));assert(restored[0]==L);
 // Exact 2x with an odd target is a fractional source crop, not a legal
 // Scale2x block grid. It uses adaptive sampling instead of failing.
 assert(gradient.presentRegion(5,5,restored,0,0,2.5,2.5,2));
 assert(gradient.presentRegion(5,5,original,0,0,2.5,2.5,3));assert(restored==original&&restored.size()==25);
 // At the maximum output axis, prefetch only centers actually sampled.
 // A half-pixel crop spans 8193 cells but needs exactly 8192 source centers.
 external::camera::NativeCanvas maximum;assert(maximum.reset(8192,2));maximum.underlayRect(0,0,8192,2,0x223344);
 assert(maximum.presentRegion(8192,1,restored,.5,0,8192,.5,1));assert(restored.front()==0xff223344&&restored.back()==0);
 // reject invalid aspect/sampling envelopes.
 assert(!sampleAdaptive([](int,int){return 0xff223344u;},0,0,4,4,3,6,restored));
 assert(!sampleAdaptive([](int,int){return 0xff223344u;},0,0,4,4,17,16,restored));
 assert(!sampleNearest([](int,int){return 0xff223344u;},0,0,std::numeric_limits<double>::infinity(),4,6,6,restored));
 std::puts("PASS: atomic DAC/RGB888 palette; actual 1.5x/3x/4x nearest placement and adaptive RGB shading; native-axis, odd 2x, maximum-axis and alpha boundary guards");
}
