#include "../../neural_renderer/source/pixel_codec.h"
#include "../../shared/source/neural_frame_protocol.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main(){
 neuralframe::Layout a,b;assert(!neuralframe::layout(0,a));assert(!neuralframe::layout(neuralframe::MaximumPixels+1,a));assert(neuralframe::layout(1,a));assert(neuralframe::layout(neuralframe::MaximumPixels,b));assert(a.color==256&&!(a.motion%64)&&!(a.output%64)&&b.bytes<512ull*1024*1024);
 uint64_t checks=0;for(unsigned r=0;r<256;++r)for(unsigned g=0;g<256;++g){unsigned blue=(r*13+g*29)&255;uint32_t source=0xff000000|(r<<16)|(g<<8)|blue,out=0;uint16_t pixel[4]{};neuralpixels::unpack(source,pixel);assert(neuralpixels::pack(pixel,source,1,out)&&out==source);++checks;}
 // Red and blue are deliberately different; treating ARGB bytes as RGBA is wrong.
 uint16_t pixel[4]{};neuralpixels::unpack(0xffff0011,pixel);assert(neuralpixels::unhalf(pixel[0])==1&&neuralpixels::unhalf(pixel[1])==0&&neuralpixels::unhalf(pixel[2])<.07f);
 pixel[0]=pixel[1]=pixel[2]=neuralpixels::half(1);uint32_t out=0;
 for(uint32_t source:{0u,0x40102030u,0xff123456u}){assert(neuralpixels::pack(pixel,source,0,out)&&out==source);if(source>>24!=255)assert(neuralpixels::pack(pixel,source,1,out)&&out==source);}
 pixel[0]=0x7e00;assert(!neuralpixels::pack(pixel,0xff102030,1,out));assert(neuralpixels::pack(pixel,0xff102030,0,out)&&out==0xff102030);
 neuralframe::Header h;h.capacityPixels=9;h.width=h.height=3;assert(neuralframe::valid(h,9));h.width=4;assert(!neuralframe::valid(h,9));h.width=3;h.flags|=0x8000;assert(!neuralframe::valid(h,9));
 std::printf("PASS: neural fixed ABI, bounded planes, %llu exact ARGB/half round trips, alpha/confidence preservation and invalid NN color rejection\n",static_cast<unsigned long long>(checks));
}
