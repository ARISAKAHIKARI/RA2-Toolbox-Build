#pragma once
#include "label_motion.h"
#include "../../external_reader/source/passenger_bubbles.h"
namespace assist { namespace passengers {
class Motion {
 visual::Motion motion_;
 Snapshot sample_{};
 visual::LabelView views_[MaxLabels]{};
public:
 void clear(){motion_.clear();}
 const visual::LabelView* update(const std::vector<external::passengers::Bubble>& bubbles,
                               uint32_t match,int frame,uint32_t sampled,uint32_t render,
                               int cameraX,int cameraY){
  sample_.labels=int(std::min(bubbles.size(),size_t(MaxLabels)));
  sample_.match=match;sample_.frame=frame;sample_.sampledTick=sampled;
  sample_.cameraX=cameraX;sample_.cameraY=cameraY;
  for(int i=0;i<sample_.labels;++i)sample_.label[i]={bubbles[i].id,bubbles[i].x,bubbles[i].y};
  motion_.update(sample_,frame);motion_.render(render,views_);return views_;
 }
};
struct Bounds {float left,top,anchor;};
inline Bounds bounds(float x,float y,int width,int height,int viewportWidth){
 float left=std::clamp(x-width/2.f,0.f,float(std::max(0,viewportWidth-width)));
 return {left,std::max(0.f,y-26-height),std::clamp(x,left+8,left+width-8)};
}
}}
