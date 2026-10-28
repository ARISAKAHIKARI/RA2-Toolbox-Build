#pragma once
#include "feedback_selection.h"
#include <memory>
namespace external { namespace nativefeedback {
struct CursorImage {int width=0,height=0,x=0,y=0;std::vector<uint32_t>pixels;};
struct CursorFrame {Mouse mouse;std::shared_ptr<const CursorImage> image;};
class CursorCache {
 Mouse key{};std::array<uint32_t,256> colors{};std::shared_ptr<const CursorImage> image;
public:
 void clear(){key={};colors={};image.reset();}
 template<class Read,class Decode>bool capture(Read read,Decode decode,CursorFrame&out){
  out={};Mouse first,last;std::array<uint32_t,256>pal{},verify{};
  if(!mouse(read,first)||!mousePalette(read,first.converter,pal)){clear();return false;}
  std::shared_ptr<const CursorImage> result=image;
  if(!result||!key.same(first)||colors!=pal){
   CursorImage candidate;
   if(!decode(first,pal,candidate)||candidate.width<1||candidate.height<1||candidate.width>512||candidate.height>512||
      std::abs(int64_t(candidate.x))>4096||std::abs(int64_t(candidate.y))>4096||
      candidate.pixels.size()!=size_t(candidate.width)*candidate.height){clear();return false;}
   if(candidate.x!=int16_t(u16(first.frameHeader.data()))||candidate.y!=int16_t(u16(first.frameHeader.data()+2))||
      candidate.width!=std::max(1,int(u16(first.frameHeader.data()+4)))||
      candidate.height!=std::max(1,int(u16(first.frameHeader.data()+6)))){clear();return false;}
   for(uint32_t p:candidate.pixels){unsigned a=p>>24;if((p&255)>a||((p>>8)&255)>a||((p>>16)&255)>a){clear();return false;}}
   result=std::make_shared<CursorImage>(std::move(candidate));
  }
  if(!mouse(read,last)||!last.same(first)||!mousePalette(read,last.converter,verify)||pal!=verify){clear();return false;}
  key=last;colors=pal;image=result;out={last,std::move(result)};return true;
 }
};
}}
