#include "../../external_reader/source/pixel_upgrade.h"
#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
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
 // The original native-pixel canvas preserves 1:1 exact colors when scaled
 // mode is requested but the output is not actually 2x.
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
 std::puts("PASS: atomic DAC/RGB888 palette, 2x nearest and edge blend, transparent borders, native 1:1 fallback");
}
