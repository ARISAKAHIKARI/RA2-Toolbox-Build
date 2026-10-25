#include "../../external_reader/source/voxel_preview.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
using namespace external::voxel;
// Full 256-byte tables from original 753D00 -> 7586F0, with unused slots
// zeroed before each call. Provenance and code hashes: voxel-lighting-native.json.
static constexpr uint32_t NativePages[4][32]={
 {0x5097acd3u, 0xc7fe40bdu, 0x346becf0u, 0xf6d44959u, 0x1091dcf9u, 0x68085ef7u, 0x3d2feeaeu, 0x5fce8a20u, 0xdbec1da3u, 0x9ffc183du, 0x1c22017eu, 0x49e5e7e5u, 0x03adef7au, 0x69fed713u, 0x5247284cu, 0x371d9da6u, 0xc34e371fu, 0x20966b41u, 0x8db4b560u, 0x4567ac29u, 0xa6e9edbcu, 0x048f3f9fu, 0xd53c445au, 0x841be6bcu, 0x0fed15c3u, 0x897c78e1u, 0x8043fc76u, 0xd692d321u, 0x6f24fd1au, 0x9e7a8307u, 0xd2255af0u, 0xc48c0cdau},
 {0xd604a9d9u, 0x7e1fef8bu, 0x81d983bau, 0xf3ac40eeu, 0xee46d6bdu, 0x044f23d3u, 0x6772fa8eu, 0x299db576u, 0x51040e55u, 0xca808370u, 0x79362753u, 0x57768f8cu, 0x6fbd084cu, 0x24cf87e3u, 0xe05a0a03u, 0x7351cde5u, 0x3de88806u, 0xdf256dcbu, 0x9a097fb4u, 0x0e8906c2u, 0xd06e4436u, 0x039a5c78u, 0x121e33efu, 0x754d9bc4u, 0x9e25caf0u, 0x5cd47975u, 0x34dac0e5u, 0xf1e93d2au, 0x73fc3cc7u, 0x21cb4ca6u, 0xa0cbf95eu, 0xa50cb1b7u},
 {0xe21ad984u, 0x7c521398u, 0xac4007c8u, 0x9563b32eu, 0x7c6634d5u, 0x27e134ffu, 0xc285b269u, 0xbfed882fu, 0x3062d757u, 0x7313a799u, 0x5c9b4c2bu, 0x60a9edb3u, 0xe9224bacu, 0xf844401eu, 0x88ddb6adu, 0x53e496b6u, 0x20398788u, 0x577e0e1du, 0x2c9ccbf9u, 0x2e4c789au, 0x045bc60bu, 0xe3315ec5u, 0x6cdceb76u, 0x60e85666u, 0xf2e9d1d7u, 0xeb9c41b0u, 0x096b5860u, 0x714903feu, 0x35270d57u, 0x952a6f3du, 0x04078fcdu, 0x0480de9du},
 {0x1e7d7894u, 0xad7486aau, 0x34e8d00bu, 0xdf01d88au, 0x573d93e8u, 0xd81421ceu, 0xbe29f369u, 0x19eef850u, 0x21f0b901u, 0x5dcd83c5u, 0xf71bc2b5u, 0x01fdd24du, 0xf000989eu, 0x38bc4f38u, 0x0f6fc06du, 0x10c6c9f3u, 0x65dda465u, 0xb2acd462u, 0xc1b9f5f9u, 0x04342440u, 0x2b72912au, 0xe852d9a1u, 0x884f1998u, 0xd1407f3au, 0x4c14179au, 0xb29edab6u, 0x78b1b61eu, 0x4107ba54u, 0xa3aa5253u, 0x9d0539aau, 0x68d31cd0u, 0x6a4ef32cu},
};
static unsigned closest(std::array<float,3> target){
 unsigned best=0;float error=100;
 for(unsigned i=0;i<Normals4.size();++i){float n=0;for(unsigned j=0;j<3;++j)n+=(target[j]-Normals4[i][j])*(target[j]-Normals4[i][j]);if(n<error){error=n;best=i;}}
 return best;
}
static Model panels(){
 Model model;Face left,right;
 left.p[0]={-8,8,0};left.p[1]={-8,8,16};left.p[2]={8,8,16};left.p[3]={8,8,0};
 right.p[0]={8,-8,0};right.p[1]={8,8,0};right.p[2]={8,8,16};right.p[3]={8,-8,16};
 left.normalIndex=uint8_t(closest({0,-1,0}));right.normalIndex=uint8_t(closest({1,0,0}));
 for(Face* f:{&left,&right}){f->normalType=4;f->hasNormal=true;f->color=1;auto n=Normals4[f->normalIndex];f->normal={n[0],-n[1],n[2]};}
 model.faces={left,right};return model;
}
static void lowerLeftIsLit(const Image&image){
 double left=0,right=0;unsigned nl=0,nr=0;
 for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){auto p=image.pixels[size_t(y)*image.width+x];if(!p)continue;int sx=x+image.x;if(sx<-1){left+=p&255;++nl;}else if(sx>1){right+=p&255;++nr;}}
 assert(nl>25&&nr>25);assert(left/nl>1.5*right/nr);
}
static void captureVpl(){
 uint32_t count=32;std::vector<uint8_t> pages(count*256);for(size_t i=0;i<pages.size();++i)pages[i]=uint8_t(i*13+i/256);
 int reads=0,countReads=0,tableReads=0,failRead=0,racingCount=0;bool racingTable=false;
 auto read=[&](uint32_t at,void*out,size_t size){
  if(++reads==failRead)return false;
  if(at==0xB2FB58&&size==4){uint32_t value=count+(++countReads==racingCount?1:0);memcpy(out,&value,4);return true;}
  if(at==0xB41178&&size==size_t(count)*256&&size<=pages.size()){
   memcpy(out,pages.data(),size);if(++tableReads==2&&racingTable)static_cast<uint8_t*>(out)[17]^=1;return true;
  }
  return false;
 };
 Vpl live;auto capture=[&]{reads=countReads=tableReads=0;return live.capture(read);};
 assert(!live.ready()&&live.signature()==0&&capture()&&live.ready());
 assert(reads==5&&tableReads==2);for(unsigned page=0;page<32;++page)for(unsigned color=0;color<256;++color)assert(live.map(page,uint8_t(color))==pages[page*256+color]);
 uint64_t signature=live.signature();assert(signature&&capture()&&live.signature()==signature);
 std::vector<uint8_t> bytes(784+pages.size());memcpy(bytes.data()+8,&count,4);memcpy(bytes.data()+784,pages.data(),pages.size());
 Vpl file;assert(file.load(bytes)&&file.signature()==signature);
 for(int failure=1;failure<=5;++failure){failRead=failure;assert(!capture()&&live.signature()==signature&&live.map(17,23)==pages[17*256+23]);}failRead=0;
 for(int header:{2,3}){racingCount=header;assert(!capture()&&live.signature()==signature);}racingCount=0;
 racingTable=true;assert(!capture()&&live.signature()==signature);racingTable=false;
 for(uint32_t invalid:{0u,33u,256u,0xffffffffu}){count=invalid;assert(!capture()&&tableReads==0&&live.signature()==signature);}count=32;
 pages[17*256+23]^=1;assert(capture()&&live.signature()!=signature&&live.map(17,23)==pages[17*256+23]);signature=live.signature();
 count=1;assert(capture()&&live.signature()!=signature&&live.map(31,23)==pages[23]);
 // An intentionally black custom table is valid; failed file loads still
 // clear their destination, while failed live snapshots retain valid data.
 std::fill(pages.begin(),pages.end(),0);assert(capture()&&live.ready()&&live.map(0,23)==0);
 assert(!file.load({})&&!file.ready()&&!file.signature());
 std::cout<<"PASS live VPL byte mapping, source-independent signature, 32-page bounds, read failure and concurrent-change retention\n";
}
int main(int argc,char**argv){
 captureVpl();
 std::ofstream dump;if(argc==3&&std::string(argv[1])=="--dump-pages")dump.open(argv[2],std::ios::binary);
 unsigned comparisons=0;
 for(unsigned mode=1;mode<=4;++mode)for(unsigned facing=0;facing<32;++facing){
  auto lighting=nativeVoxelLight((int(facing)-8)*(6.28318530717958647692/32));uint32_t hash=2166136261u;uint8_t pages[256]{};
  for(unsigned i=0;i<256;++i){std::array<float,3> normal;bool valid=normalVector(mode,i,normal);pages[i]=uint8_t(i>=253?16:valid?nativeVoxelPage(normal,lighting):0);hash=(hash^pages[i])*16777619u;if(valid||i>=253)++comparisons;}
  assert(hash==NativePages[mode-1][facing]);if(dump)dump.write(reinterpret_cast<const char*>(pages),256);
 }
 assert(Normals4[241][0]<0);assert(Normals4[241]==Normals4[240]);
 auto source=nativeVoxelWorldLight();auto screen=project({source[0],source[1],source[2]});assert(screen.x<-.49f&&screen.y>.69f);
 // Two visible sides: native -Y becomes preview +Y, the lower-left side.
 // VPL pages are encoded as gray levels, so this asserts actual raster pixels.
 std::vector<uint8_t> bytes(784+32*256);uint32_t count=32;std::memcpy(bytes.data()+8,&count,4);
 for(unsigned page=0;page<count;++page)for(unsigned i=0;i<256;++i)bytes[784+page*256+i]=uint8_t(page+1);
 Vpl vpl;assert(vpl.load(bytes));std::array<uint32_t,256> palette{};for(unsigned i=1;i<256;++i)palette[i]=0xff000000u|((std::min(i*7u,255u))*0x010101u);
 auto model=panels();Image lit;assert(render(model,0,palette,lit,0,0,&vpl,255));lowerLeftIsLit(lit);
 auto turret=model;for(auto&f:turret.faces)f.turret=true;Image independent;assert(render(turret,1.5707963267948966,palette,independent,0,0,&vpl,255));assert(independent.x==lit.x&&independent.y==lit.y&&independent.pixels==lit.pixels);
 palette[1]=0xff505050;Image fallback;assert(render(model,0,palette,fallback,0,0,nullptr,255));lowerLeftIsLit(fallback);
 for(auto&f:model.faces){f.normalIndex=253;f.hasNormal=false;}Image special;assert(render(model,0,palette,special,0,0,&vpl,255));for(size_t i=0;i<special.pixels.size();++i)if(special.pixels[i])assert(special.indices[i]==17);
 std::cout<<"PASS "<<comparisons<<" native normal/special VPL pages; lower-left light pixels in VPL and fallback; independent turret lighting; normal 241 sign\n";
}
