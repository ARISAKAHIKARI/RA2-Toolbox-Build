#include "../../external_reader/source/asset_decode.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace external;
static void u16(std::vector<uint8_t>&b,size_t p,uint16_t v){b[p]=v;b[p+1]=v>>8;}
static void u32(std::vector<uint8_t>&b,size_t p,uint32_t v){memcpy(b.data()+p,&v,4);}
static std::vector<uint8_t> shape(int format,const std::vector<uint8_t>&pixels,int x=0,int width=4,int height=3){
 std::vector<uint8_t> b(32);u16(b,2,4);u16(b,4,3);u16(b,6,1);u16(b,8,x);u16(b,12,width);u16(b,14,height);u32(b,16,format);u32(b,28,32);b.insert(b.end(),pixels.begin(),pixels.end());return b;
}
int main(int argc,char**argv){
 std::array<uint32_t,256> p{};for(unsigned i=0;i<256;++i)p[i]=0xff000000|i*0x010101;
 voxel::Image image;
 const std::vector<uint8_t> raw={1,2,3,4,5,6,7,8,9,10,11,12};
 for(int format:{0,1}){assert(assets::shp(shape(format,raw),0,p,image));for(int i=0;i<12;++i)assert(image.pixels[i]==voxel::premultiply(p[i+1],1));}
 // FA2/RA2 bit 1 selects per-row length prefixes and zero-run decoding.
 const std::vector<uint8_t> prefixed={6,0,1,2,3,4,6,0,5,6,7,8,6,0,9,10,11,12};
 for(int format:{2,3,6}){assert(assets::shp(shape(format,prefixed),0,p,image));for(int i=0;i<12;++i)assert(image.pixels[i]==voxel::premultiply(p[i+1],1));}
 // Real Westwood/FA2 images pad trailing transparent runs to an even width.
 // Clip just that transparent run, retaining every literal and the next row.
 for(int format:{2,3}){assert(assets::shp(shape(format,{5,0,1,0,8,5,0,2,0,8,5,0,3,0,8},0,3),0,p,image));for(int y=0;y<3;++y){assert(image.pixels[y*3]==voxel::premultiply(p[y+1],1));assert(!image.pixels[y*3+1]&&!image.pixels[y*3+2]);}}
 assert(assets::shp(shape(3,{5,0,1,0,2,2,0,6,0,5,6,7,8}),0,p,image));assert(image.pixels[0]&&image.pixels[3]==0&&image.pixels[4]==0&&image.pixels[11]);
 assert(!assets::shp(shape(3,{3,0,0}),0,p,image)); // truncated zero-run
 assert(!assets::shp(shape(2,{7,0,1,2,3,4,5}),0,p,image)); // literal overflow
 assert(!assets::shp(shape(2,{6,0,1,2}),0,p,image));
 // Canvas dimensions locate the origin; they do not clip cropped frames.
 assert(assets::shp(shape(0,{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},-1,5),0,p,image));assert(image.width==5&&image.x==-3);
 auto empty=shape(3,{});u32(empty,28,0);assert(assets::shp(empty,0,p,image));for(auto c:image.pixels)assert(!c);
 assert(!assets::shp(empty,1,p,image));
 // Optional public or user-provided real files, never copied into the repo.
 for(int i=1;i<argc;++i){std::ifstream f(argv[i],std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});assert(b.size()>=8);int count=voxel::u16(b.data()+6);for(int n=0;n<count;++n)if(!assets::shp(b,n,p,image)){std::cerr<<argv[i]<<" frame "<<n<<" failed\n";return 1;}std::cout<<"PASS real SHP "<<count<<" frames\n";}
 std::cout<<"PASS SHP raw/FA2-prefixed-RLE, transparent/empty/cropped frames and malformed bounds\n";
}
