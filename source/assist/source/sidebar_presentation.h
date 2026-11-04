#pragma once
#include "../../shared/source/overlay_composition.h"
namespace sidebarpresentation {
constexpr uint64_t Layer=25;
class Identity {
 uint64_t token_=0;int tab_=-1,width_=0,height_=0;
public:
 bool changed(uint64_t token,int tab,int width,int height){
  const bool changed=token!=token_||tab!=tab_||width!=width_||height!=height_;
  token_=token;tab_=tab;width_=width;height_=height;return changed;
 }
};
inline overlaycomposition::Surface surface(const overlaycomposition::Context&context,const overlaycomposition::Rect&rect,
                                          std::shared_ptr<const std::vector<uint32_t>>pixels){
 return {Layer,10,context,rect,false,255,std::move(pixels)};
}
}
