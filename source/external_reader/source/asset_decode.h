#pragma once
#include "terrain_preview.h"
#include "game_archive.h"
namespace external { namespace assets {
struct Memory {
 static constexpr uint32_t Base=0x10000;std::vector<uint8_t> bytes;
 bool operator()(uint32_t p,void*out,size_t n)const{if(p<Base||uint64_t(p-Base)+n>bytes.size())return false;memcpy(out,bytes.data()+p-Base,n);return true;}
 uint32_t append(const void*p,size_t n){if(bytes.size()+n>voxel::MaxBytes)return 0;uint32_t at=Base+bytes.size();bytes.resize(bytes.size()+n);if(n)memcpy(bytes.data()+at-Base,p,n);return at;}
};
inline bool tmp(std::vector<uint8_t> bytes,unsigned sub,const std::array<uint32_t,256>&pal,voxel::Image&out,std::string*reason=nullptr){
 if(reason)*reason="directory bytes="+std::to_string(bytes.size());
 if(bytes.size()<20)return false;uint32_t cols=word(bytes.data()),rows=word(bytes.data()+4);if(!cols||!rows||cols>64||rows>64||16+cols*rows*4>bytes.size())return false;
 for(unsigned i=0;i<cols*rows;++i){uint32_t p=word(bytes.data()+16+i*4);if(p){if(p>=bytes.size()||p<16+cols*rows*4)return false;p+=Memory::Base;memcpy(bytes.data()+16+i*4,&p,4);}}
 Memory memory{std::move(bytes)};return terrain::tile(memory,Memory::Base,sub,pal,out,reason);
}
inline bool shp(std::vector<uint8_t> bytes,int frame,const std::array<uint32_t,256>&pal,voxel::Image&out){Memory m{std::move(bytes)};return voxel::sprite(m,Memory::Base,pal,out,frame);}
inline bool pal(const std::vector<uint8_t>&bytes,std::array<uint32_t,256>&out){if(bytes.size()!=768)return false;for(int i=0;i<256;++i){if(bytes[i*3]>63||bytes[i*3+1]>63||bytes[i*3+2]>63)return false;out[i]=0xff000000u|(uint32_t(bytes[i*3])*4<<16)|(uint32_t(bytes[i*3+1])*4<<8)|uint32_t(bytes[i*3+2])*4;}out[0]=0;return true;}
inline bool vxl(const std::vector<uint8_t>&v,const std::vector<uint8_t>&h,voxel::Model&out,float offset=0,float sx=1,float sy=1){
 if(v.size()<802||h.size()<24||memcmp(v.data(),"Voxel Animation",15))return false;unsigned count=word(v.data()+20),bodySize=word(v.data()+28),frames=word(h.data()+16),layers=word(h.data()+20);
 if(!count||count>32||word(v.data()+24)!=count||!frames||frames>4096||layers!=count||uint64_t(802)+count*28+bodySize+count*92>v.size()||uint64_t(24)+layers*16+uint64_t(layers)*frames*48>h.size())return false;
 Memory m;uint32_t body=m.append(v.data()+802+count*28,bodySize);if(!body)return false;std::vector<voxel::Tail> tails(count);std::vector<std::array<float,12>> matrices(count);
 for(unsigned i=0;i<count;++i){const auto*p=v.data()+802+count*28+bodySize+i*92;auto&t=tails[i];t.start=body+word(p);t.end=body+word(p+4);t.data=body+word(p+8);memcpy(&t.multiplier,p+12,4);memcpy(t.matrix,p+16,48);float b[6];memcpy(b,p+64,24);memcpy(t.size,p+88,3);t.normal=p[91];
  if(!voxel::finite(t.multiplier)||t.multiplier<=0||t.multiplier>64)return false;
  for(int j=0;j<8;++j)for(int axis=0;axis<3;++axis)t.bounds[j*3+axis]=b[axis+((j&(1<<axis))?3:0)];
  int layer=-1;for(unsigned k=0;k<layers;++k)if(!memcmp(v.data()+802+i*28,h.data()+24+k*16,16)){layer=int(k);break;}if(layer<0)return false;
  memcpy(matrices[i].data(),h.data()+24+layers*16+layer*48,48);for(int axis=0;axis<3;++axis)matrices[i][axis*4+3]*=t.multiplier;
 }
 uint32_t tail=m.append(tails.data(),tails.size()*sizeof(voxel::Tail)),matrix=m.append(matrices.data(),matrices.size()*48);if(!tail||!matrix)return false;
 voxel::Library lib{0,count,count,bodySize,0,tail,body};uint32_t library=m.append(&lib,sizeof lib);uint32_t motion[4]={0,layers,frames,matrix};uint32_t hva=m.append(motion,sizeof motion);return library&&hva&&voxel::capturePart(m,library,hva,out,offset,sx,sy);
}
template<class Read>std::string text(Read read,uint32_t p,unsigned n){if(p<0x10000||n>127)return {};char s[128]{};if(!read(p,s,n))return {};s[n]=0;return safe(s)?name(s):std::string{};}
template<class Read>std::string shapeName(Read read,uint32_t p){uint16_t tag=0;uint32_t filename=0;if(!read(p,&tag,2)||tag!=65535||!read(p+8,&filename,4)||filename<0x10000)return {};std::string s;for(int i=0;i<127;++i){char c=0;if(!read(filename+i,&c,1))return {};if(!c)return safe(s)?name(s):std::string{};s.push_back(c);}return {};}
}}
