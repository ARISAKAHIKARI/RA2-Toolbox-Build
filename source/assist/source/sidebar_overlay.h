#pragma once
#include "sidebar_presentation.h"
// Fixed sidebar decoration has no world-camera dependency. Keep its completed
// pixels while a newer world view is being rendered; input remains elsewhere.
namespace sidebaroverlay {
constexpr uint64_t Layer=sidebarpresentation::Layer;
static asyncwork::Latest<worldrender::Job,worldrender::Frame> worker(worldrender::rasterize);
static worldpresentation::Submission submission;
static void invalidate(){overlaycompositor::host().hide(Layer);if(submission.invalidate())worker.invalidate();}
static bool update(POINT origin,const RECT&client,const std::vector<construction::NativeBadge>&badges,
                   int nativeTab,uint32_t idleTabs,bool phase,const std::vector<RECT>&opaquePanels){
 auto context=overlaycompositor::host().capture(uintptr_t(game),pid);
 if(!context.token){invalidate();return false;}
 static sidebarpresentation::Identity identity;
 if(identity.changed(context.token,nativeTab,snap.width,snap.height))invalidate();
 if(badges.empty()&&!idleTabs){invalidate();return true;}
 auto frame=worker.get();
 if(frame&&frame->pid==pid&&frame->match==snap.match&&frame->clientW==client.right&&frame->clientH==client.bottom&&frame->context.token==context.token){
  auto pixels=std::shared_ptr<const std::vector<uint32_t>>(frame,&frame->pixels);
  overlaycompositor::host().publish(sidebarpresentation::surface(frame->context,{frame->relative.x,frame->relative.y,frame->w,frame->h},std::move(pixels)));
 }
 viewport::Scale view;if(!view.set(snap.width,snap.height,client.right,client.bottom)){invalidate();return true;}
 static uint64_t drawnToken=0;static int drawnWidth=0,drawnHeight=0,drawnClientW=0,drawnClientH=0;
 static uint32_t drawnTabs=0,drawnShapes=0;static bool drawnPhase=false;
 static std::vector<construction::NativeBadge> drawnBadges;static std::vector<RECT> drawnPanels;
 bool changed=drawnToken!=context.token||drawnWidth!=snap.width||drawnHeight!=snap.height||drawnClientW!=client.right||drawnClientH!=client.bottom||
  drawnBadges!=badges||drawnTabs!=idleTabs||drawnShapes!=sidebarShapes.revision||(idleTabs&&drawnPhase!=phase)||
  drawnPanels.size()!=opaquePanels.size()||(!opaquePanels.empty()&&memcmp(drawnPanels.data(),opaquePanels.data(),opaquePanels.size()*sizeof(RECT)));
 if(!submission.required(changed))return true;
 int left=snap.width,top=snap.height,right=0,bottom=0;
 auto include=[&](int x,int y,int w,int h){int x0=std::clamp(x,0,snap.width),y0=std::clamp(y,0,snap.height),x1=std::clamp(x+w,0,snap.width),y1=std::clamp(y+h,0,snap.height);if(x1>x0&&y1>y0){left=std::min(left,x0);top=std::min(top,y0);right=std::max(right,x1);bottom=std::max(bottom,y1);}};
 for(const auto&b:badges)include(b.x,b.y,b.w,b.h);
 std::array<external::sidebar::Frame,4> shapes;
 for(int i=0;i<4;++i)if(idleTabs&(1u<<i)){shapes[i]=sidebarShapes.frames[i];const auto&f=shapes[i];include(f.x,f.y,f.width,f.height);}
 if(right<=left||bottom<=top){invalidate();return true;}
 int width=view.width(right-left),height=view.height(bottom-top);if(width<1||height<1){invalidate();return true;}
 auto draw=[=](Graphics&g) mutable {
  g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(view.x,view.y);g.TranslateTransform(-float(left),-float(top));
  for(const auto&r:opaquePanels)g.SetClip(RectF(float(r.left),float(r.top),float(r.right-r.left),float(r.bottom-r.top)),CombineModeExclude);
  if(idleTabs&&phase){auto state=g.Save();g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);
   for(int i=0;i<4;++i)if(idleTabs&(1u<<i)){auto&f=shapes[i];Bitmap image(f.width,f.height,f.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(f.pixels.data()));g.DrawImage(&image,RectF(float(f.x),float(f.y),float(f.width),float(f.height)));}g.Restore(state);
  }
  for(const auto&b:badges){SolidBrush tag(Color(230,97,67,14));g.FillRectangle(&tag,b.x,b.y,b.w,b.h);
   std::wstring numbers;for(int i=0;i<construction::Capacity;++i)if(b.positions&(1<<i))numbers+=std::to_wstring(i+1);
   drawText(g,numbers,float(b.x+2),float(b.y),10.f,Color(255,255,239,158));}
 };
 POINT relative{view.left(left),view.top(top)};origin.x+=relative.x;origin.y+=relative.y;
 worldrender::Job job{pid,snap.match,GetTickCount(),width,height,client.right,client.bottom,origin,std::move(draw),context,relative};
 worker.submit(std::move(job));submission.submitted();
 drawnToken=context.token;drawnWidth=snap.width;drawnHeight=snap.height;drawnClientW=client.right;drawnClientH=client.bottom;
 drawnBadges=badges;drawnTabs=idleTabs;drawnShapes=sidebarShapes.revision;drawnPhase=phase;drawnPanels=opaquePanels;
 return true;
}
}
