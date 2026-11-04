#include "../../external_reader/source/native_base_shade.h"
#include <vector>
#include <cassert>
#include <cstdio>
int main(){
 using namespace external;std::vector<uint32_t>input,expected;std::vector<uint8_t>alpha;
 for(unsigned a=0;a<256;++a)for(unsigned value=0;value<256;++value){uint32_t c=(value<<24)|(value<<16)|((255-value)<<8)|((value*17)&255);input.push_back(c);alpha.push_back(uint8_t(a));
  uint32_t out=c&0xff000000u;for(unsigned shift:{0u,8u,16u})out|=std::min(255u,((c>>shift)&255u)*a/127u)<<shift;expected.push_back(out);}
 for(unsigned offset=0;offset<4;++offset)for(unsigned tail=0;tail<4;++tail)for(bool simd:{false,true}){
  auto actual=input;const size_t count=input.size()-offset-tail;nativesurface::shadeBase(actual.data()+offset,alpha.data()+offset,count,simd);
  for(size_t n=0;n<actual.size();++n)assert(actual[n]==(n>=offset&&n<offset+count?expected[n]:input[n]));
 }
 // Neutral and full-dark groups exercise the SIMD whole-group shortcuts.
 std::vector<uint32_t>pixels(33,0x80402010u);std::vector<uint8_t>a(33,127);
 nativesurface::shadeBase(pixels.data(),a.data(),pixels.size(),true);for(auto p:pixels)assert(p==0x80402010u);
 std::fill(a.begin(),a.end(),0);nativesurface::shadeBase(pixels.data(),a.data(),pixels.size(),true);for(auto p:pixels)assert(p==0x80000000u);
 std::puts("PASS: all 65536 channel/A pairs with independent scalar expectations, alpha preservation, offsets/tails and scalar/SSE2 paths");
}
