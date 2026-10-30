#include "../../external_reader/source/native_image_bank.h"
#include <cassert>
#include <thread>
#include <iostream>
int main(){
 using namespace external::nativeimages;Bank bank(12000);Key one;one.values[0]=7;one.values[1]=9;one.palette[3]=0xffaabbcc;
 auto image=std::make_shared<external::voxel::Image>();image->width=image->height=20;image->pixels.assign(400,0xffaabbcc);image->depth.assign(400,1);
 assert(!bank.get(one));bank.put(one,image);auto held=bank.get(one);assert(held==image);assert(bank.get(one)==held);
 // A later viewport owns the identical immutable native result, not a copy
 // of a minimap or a second rasterization. Profile/world/color keys separate.
 for(int field:{0,1,2,10,15,20}){auto changed=one;++changed.values[field];assert(!bank.get(changed));}
 auto color=one;++color.palette[3];assert(!bank.get(color));
 std::thread a([&]{for(int i=0;i<5000;++i)assert(bank.get(one)==held);});std::thread b([&]{for(int i=0;i<5000;++i)assert(bank.get(one)==held);});a.join();b.join();
 for(int i=0;i<10;++i){auto key=one;key.values[2]=i+1;bank.put(key,image);assert(bank.stats().resident<=12000);}assert(held->pixels[0]==0xffaabbcc); // eviction never invalidates a live reader
 auto incomplete=std::make_shared<external::voxel::Image>(*image);incomplete->incomplete=true;Key missing;bank.put(missing,incomplete);assert(!bank.get(missing));
 std::cout<<"PASS native pre-scale image identity, 10000 concurrent shared reads, world/profile/palette isolation, bounded eviction and live ownership\n";
}
