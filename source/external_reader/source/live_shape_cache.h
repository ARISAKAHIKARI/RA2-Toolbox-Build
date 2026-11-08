#pragma once
#include "voxel_preview.h"
#include "resource_read.h"
#include <map>
#include <tuple>
namespace external { namespace liveshape {
// GPL-3.0: Vera20K 3d42426, selection_overlay.rs / instances/units.rs.
// Plain Convert palettes belong to the shape's draw owner, not the player's
// ColorScheme. One frame-local palette sample feeds all cached SHP instances.
class Cache {
 struct Palette {uint32_t converter=0,row=0;std::array<uint32_t,256> colors{};};
 struct Entry {std::array<uint8_t,8>shape{};std::array<uint8_t,24>frame{};Palette palette;std::shared_ptr<const voxel::Image>image;};
 std::map<uint32_t,Palette> palettes_;
 std::map<std::tuple<uint32_t,uint32_t,int>,Entry> images_;
 size_t bytes_=0;
 static size_t bytes(const voxel::Image&i){return i.pixels.size()*4+i.indices.size()+sizeof(Entry);}
 template<class Read>bool palette(Read read,uint32_t slot,Palette&out){
  if(auto found=palettes_.find(slot);found!=palettes_.end()){out=found->second;return true;}
  Palette next;uint32_t vt=0,bpp=0,again=0;int shades=0;std::array<uint16_t,256> colors{};std::array<int,6>shifts{},verify{};
  if(!read(slot,&next.converter,4)||!voxel::range(next.converter,0x178)||!read(next.converter,&vt,4)||vt!=0x7E5358||
   !read(next.converter+4,&bpp,4)||bpp!=2||!read(next.converter+0x16C,&shades,4)||shades<1||shades>256||
   !read(next.converter+0x174,&next.row,4)||!voxel::range(next.row,512)||!read(next.row,colors.data(),512)||
   !read(0x8A0DD0,shifts.data(),sizeof shifts)||!read(0x8A0DD0,verify.data(),sizeof verify)||verify!=shifts||
   !read(next.converter+0x174,&again,4)||again!=next.row||!read(slot,&again,4)||again!=next.converter)return false;
  const bool rgb565=shifts==std::array<int,6>{{11,3,0,3,5,2}},rgb555=shifts==std::array<int,6>{{10,3,0,3,5,3}};if(!rgb565&&!rgb555)return false;
  for(size_t i=1;i<colors.size();++i){unsigned c=colors[i],r=(c>>(rgb565?11:10))&31,g=(c>>5)&(rgb565?63:31),b=c&31;
   next.colors[i]=0xff000000u|(((r<<3)|(r>>2))<<16)|((rgb565?((g<<2)|(g>>4)):((g<<3)|(g>>2)))<<8)|(b<<3)|(b>>2);
  }
  palettes_[slot]=next;out=next;return true;
 }
public:
 void reset(){palettes_.clear();images_.clear();bytes_=0;}
 void begin(){palettes_.clear();}
 template<class Read>std::shared_ptr<const voxel::Image> get(Read read,uint32_t shape,int frame,uint32_t paletteSlot){
  if(frame<0||frame>=8192||!voxel::range(shape,16))return {};Palette colors;if(!palette(read,paletteSlot,colors))return {};
  auto resource=resourceread::view(read);
  std::array<uint8_t,8>head{};if(!resource(shape,head.data(),head.size()))return {};
  if(voxel::u16(head.data())==0xffff){if(!resource(shape+12,&shape,4)||!voxel::range(shape,8)||!resource(shape,head.data(),head.size()))return {};}
  if(voxel::u16(head.data())||frame>=voxel::u16(head.data()+6)||!voxel::range(shape,8+size_t(frame+1)*24))return {};
  std::array<uint8_t,24>fh{};if(!resource(shape+8+frame*24,fh.data(),fh.size()))return {};
  const auto key=std::make_tuple(paletteSlot,shape,frame);auto found=images_.find(key);
  if(found!=images_.end()&&found->second.shape==head&&found->second.frame==fh&&found->second.palette.converter==colors.converter&&found->second.palette.row==colors.row&&found->second.palette.colors==colors.colors)return found->second.image;
  voxel::Image image;if(!voxel::sprite(resource,shape,colors.colors,image,frame,nullptr,255)||image.width>512||image.height>512)return {};
  std::array<uint8_t,24>after{};std::array<uint8_t,8>afterHead{};if(!resource(shape+8+frame*24,after.data(),after.size())||after!=fh||!resource(shape,afterHead.data(),afterHead.size())||afterHead!=head)return {};
  image.writeDepth=false;image.objectPalette=false;
  if(found!=images_.end()){bytes_-=bytes(*found->second.image);images_.erase(found);}
  const size_t cost=bytes(image);if(images_.size()>=256||bytes_+cost>32*1024*1024){images_.clear();bytes_=0;}
  auto result=std::make_shared<const voxel::Image>(std::move(image));bytes_+=cost;images_[key]={head,fh,colors,result};return result;
 }
};
}}
