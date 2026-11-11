#pragma once
namespace previewoverlay {
struct Bounds {float x=0,y=0,w=0,h=0;explicit operator bool()const{return w>0&&h>0;}};
inline Bounds bounds(const external::voxel::Image&i,const external::Projection&p,const nativeviewhost::Transform&t,int cameraX,int cameraY){
 float x=float(i.x+p.cameraX-cameraX),y=float(i.y+p.cameraY-cameraY),right=x+i.width,bottom=y+i.height;
 if(!nativeviewhost::localPoint(t,x,y)||!nativeviewhost::localPoint(t,right,bottom))return {};
 return {x,y,right-x,bottom-y};
}
inline void draw(Graphics&g,const external::voxel::Image&i,const external::Projection&p,const nativeviewhost::Transform&t,int cameraX,int cameraY){
 auto b=bounds(i,p,t,cameraX,cameraY);if(!b)return;
 Bitmap bitmap(i.width,i.height,i.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(i.pixels.data())));auto saved=g.Save();
 if(t.active){const auto area=t.physical;g.SetClip(RectF(float(area.x),float(area.y),float(area.w),float(area.h)),CombineModeIntersect);}
 g.DrawImage(&bitmap,RectF(b.x,b.y,b.w,b.h),0.f,0.f,float(i.width),float(i.height),UnitPixel,nullptr);g.Restore(saved);
}
}
