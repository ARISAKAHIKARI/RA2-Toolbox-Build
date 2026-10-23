"""Compare production SHP pixels with EA FA2/XCC decode3 on supplied real files.
Reference and art remain outside this repository. Usage: script FA2_shp_decode.cpp *.shp
Primary source: electronicarts/CNC_TS_and_RA2_Mission_Editor, MissionEditor/XCC/shp_decode.cpp.
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys,tempfile
reference=Path(sys.argv[1]);files=[Path(p).resolve() for p in sys.argv[2:]]
assert files
source=reference.read_text();start=source.index('int decode3(');end=source.index('\nint encode3(',start)
decode=source[start:end].replace('unsigned __int16','uint16_t')
# The reference assumes x86 unaligned uint16 loads. Express that byte load
# portably for UBSan; the decoder loop is otherwise the supplied FA2 function.
decode=decode.replace('*reinterpret_cast<const uint16_t*>(r)', '(uint16_t(r[0]) | uint16_t(r[1]) << 8)')
harness=r'''
#include "source/external_reader/source/asset_decode.h"
#include <cassert>
#include <fstream>
#include <iostream>
using byte=uint8_t;
REFERENCE_DECODER
int main(int argc,char**argv){
 using namespace external;std::array<uint32_t,256> pal{};
 for(int n=0;n<256;++n)pal[n]=0xff000000u|unsigned(n)*0x010101u;
 for(int f=1;f<argc;++f){std::ifstream stream(argv[f],std::ios::binary);std::vector<byte>b((std::istreambuf_iterator<char>(stream)),{});assert(b.size()>=8);int count=voxel::u16(b.data()+6);
  for(int frame=0;frame<count;++frame){const byte*h=b.data()+8+24*frame;int w=voxel::u16(h+4),height=voxel::u16(h+6);uint32_t at=voxel::u32(h+20);voxel::Image result;
   assert(assets::shp(b,frame,pal,result));assert(result.x==int16_t(voxel::u16(h))-int(voxel::u16(b.data()+2))/2);assert(result.y==int16_t(voxel::u16(h+2))-int(voxel::u16(b.data()+4))/2);
   std::vector<byte> expected(size_t(std::max(1,w))*std::max(1,height));
   if(at&&w&&height){assert(at<b.size());if(voxel::u32(h+8)&2)assert(decode3(b.data()+at,expected.data(),w,height)==w*height);else{assert(at+size_t(w)*height<=b.size());memcpy(expected.data(),b.data()+at,size_t(w)*height);}}
   assert(expected.size()==result.pixels.size());for(size_t i=0;i<expected.size();++i)assert(result.pixels[i]==(expected[i]?voxel::premultiply(pal[expected[i]],1):0));
  }std::cout<<"PASS "<<argv[f]<<" frames="<<count<<" FA2 pixels and crop anchors\n";
 }
}
'''.replace('REFERENCE_DECODER',decode)
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'compare.cpp').write_text(harness)
 subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root),str(p/'compare.cpp'),'-o',str(p/'compare')],check=True)
 subprocess.run([str(p/'compare'),*map(str,files)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print(json.dumps({'reference_sha256':hashlib.sha256(reference.read_bytes()).hexdigest(),'samples':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files}},indent=2))
