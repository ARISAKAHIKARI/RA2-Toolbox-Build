// Exact pre-optimization production object() body; only function name is changed.
namespace external { namespace lighting {
template<class Read>State objectBefore(Read read,uint32_t type,uint32_t owner,int kind,int x,int y,int z,const State&fallback){
 uint8_t terrainPalette=0,voxel=0;
 if(type>=0x10000&&type<=0xfffffdc9u)read(type+0x236,&voxel,1);
 if(kind==3&&!voxel&&type>=0x10000&&type<=0xffffe8fcu&&read(type+0x1702,&terrainPalette,1)&&terrainPalette)
  return at(read,x,y,z,fallback,true); // 705EC1: also replaces ExtraLight with cell terrain intensity.
 State out=at(read,x,y,z,fallback,false,false);
 uint32_t converter=objectConverter(read,type,owner);converterTint(read,converter,out.tint);converterMetadata(read,converter,out);
 if(kind>=0&&kind<=2){
  // Unit::Draw 73D0C3 adds ExtraUnitLight before selecting VXL or SHP.
  // Infantry::Draw 519457 and Aircraft::Draw 414925 use separate fields.
  // Rules parsing stores config*1000 as signed integers, despite public
  // float declarations. Never infer one class's light from another class.
  uint32_t rules=0,after=0,offset=kind==2?0x17D4:(kind==1?0x17D8:0x17DC);int32_t extra=0;
  if(read(0x8871E0,&rules,4)&&rules>=0x10000&&uint64_t(rules)+offset+4<=0x100000000ull&&read(rules+offset,&extra,4)&&
     read(0x8871E0,&after,4)&&after==rules){
   int64_t base=out.intensity<0?int64_t(out.ambient)*10:out.intensity;
   out.intensity=int(std::clamp<int64_t>(base+extra,0,2000)); // presentation range; negative is not fallback ambient
  }
 }
 if(kind==3&&type>=0x10000&&type<=0xffffeab6u){int16_t extra=0;if(read(type+0x1548,&extra,2))out.intensity=(out.intensity<0?out.ambient*10:out.intensity)+extra;}
 return out;
}
}}
