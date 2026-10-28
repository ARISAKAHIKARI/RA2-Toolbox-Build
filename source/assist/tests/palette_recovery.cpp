#include "../../external_reader/source/palette_recovery.h"
#include "../../external_reader/source/pixel_upgrade.h"
#include <cassert>
#include <cstdio>
static uint32_t expand(uint16_t c){unsigned r=(c>>11)&31,g=(c>>5)&63,b=c&31;return 0xff000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);}
int main(){
 using namespace external;std::array<uint32_t,256> source{},live{};
 for(unsigned i=1;i<256;++i){source[i]=0xff000000u|(uint32_t(pixelupgrade::expand6(i%64))<<16)|(uint32_t(pixelupgrade::expand6((i*3)%64))<<8)|pixelupgrade::expand6((i*7)%64);live[i]=expand(paletterecovery::rgb565(source[i]));}
 auto original=live;assert(original!=source);assert(paletterecovery::promote(live,source)&&live==source);
 for(unsigned i=1;i<256;++i)assert(paletterecovery::rgb565(live[i])==paletterecovery::rgb565(original[i]));
 live=original;auto mismatch=source;mismatch[174]^=0x00800000;assert(!paletterecovery::promote(live,mismatch)&&live==original); // one bad entry rejects the complete candidate
 live[12]=expand(0xffff);auto changedLive=live;assert(!paletterecovery::promote(live,source)&&live==changedLive); // live mutation wins
 live=original;mismatch=source;mismatch[9]&=0x7fffffff;assert(!paletterecovery::promote(live,mismatch)&&live==original);
 mismatch=source;mismatch[0]=0xffffffff;assert(paletterecovery::promote(live,mismatch)&&live[0]==0); // transparent index never changes
 std::puts("PASS: source-PAL RGB565-guided precision recovery; all-entry consistency, mutation/mismatch rejection and alpha/index-zero guards");
}
