#include "../source/render_presentation.h"
#include "../../shared/source/latest_worker.h"
#include "../../external_reader/source/feedback_cursor.h"
#include "../../external_reader/source/feedback_orders.h"
#include "../../external_reader/source/voxel_preview.h"
#include <cassert>
#include <cstdio>
#include <mutex>
#include <string>
using DWORD=uint32_t;struct POINT {int x=0,y=0;};struct CURSORINFO {unsigned cbSize=0,flags=0;};
constexpr unsigned CURSOR_SHOWING=1;
static uint32_t clockNow=1000;static uintptr_t game=101;static POINT cursorPosition{100,100};static bool systemCursor=false;
static uint32_t GetTickCount(){return clockNow;}
static bool GetCursorInfo(CURSORINFO*c){c->flags=systemCursor?CURSOR_SHOWING:0;return true;}
static bool GetCursorPos(POINT*p){*p=cursorPosition;return true;}
static bool ScreenToClient(uintptr_t,POINT*){return true;}
static bool IsWindowVisible(uintptr_t){return true;}
namespace overlaystack {enum class Layer{Cursor};struct Entry{Layer layer;uintptr_t window;};static std::mutex mutex;static std::vector<Entry>entries;}
namespace overlaycompositor {
using overlaycomposition::Context;using overlaycomposition::Surface;constexpr uint64_t RenderUpgrade=3;
struct Host{int removals=0;bool replaceGroup(const std::vector<uint64_t>&,std::vector<Surface>){++removals;return true;}};
static Host&host(){static Host h;return h;}
}
static constexpr uint32_t mouseAt=0x10000,mouseVT=0x11000,shape=0x12000,converter=0x13000,palette=0x14000;
struct Reader {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(0xB00000);bool failSelection=false,failMouse=false,failView=false;size_t reads=0,readBytes=0,pixelReads=0;
 bool open(uint32_t){return true;}void close(){}
 bool readFresh(uint32_t p,void*out,size_t n){++reads;readBytes+=n;if(p>=shape+56&&p<shape+64)++pixelReads;
  if((failSelection&&p==0xA8ECB8)||(failMouse&&p==0x887640)||(failView&&p==0xA8B230))return false;
  if(uint64_t(p)+n>bytes.size())return false;std::memcpy(out,bytes.data()+p,n);return true;
 }
 template<class T>void put(uint32_t p,const T&v){std::memcpy(bytes.data()+p,&v,sizeof v);}
}externalReader;
namespace external {using ReadSession=Reader;}
namespace renderupgrade {
struct Request {
 uint64_t generation=1;bool materialRefresh=true;int detail=1,colorPrecision=1;uint32_t pid=77,match=8,tick=1000,reveal=0;int paletteRange=1,pixelScaleMode=3;int width=640,height=600;
 external::mainview::View view{0x20000,0x21000,0x22000,50,100,200,800,600,0,0,640,600};
 overlaycompositor::Context context{9,101,77,8,800,600,100,200};
};
struct Frame {Request request;std::shared_ptr<const std::vector<uint32_t>>image;uint32_t sampledTick=1000,completedTick=1000;std::vector<external::nativefeedback::Object>objects;double yellow=.5,red=.25;};
#include "../source/render_feedback_host.h"
}
static void setup(const renderupgrade::Request&r){
 auto&m=externalReader;const auto&v=r.view;
 m.put(0xA8B230,v.scenario);m.put(0xA83D4C,v.house);m.put(0x887324,v.tactical);m.put(0xA8ED84,v.frame);m.put(0xA8E9A0,uint8_t(1));
 m.put(0xA8E378,uint8_t(1));m.put(0x8A00A4,v.screenW);m.put(0x8A00A8,v.screenH);
 m.put(v.tactical+0xB0,v.cameraX);m.put(v.tactical+0xB4,v.cameraY);
 const int rect[]{v.x,v.y,v.width,v.height};m.put(0x886FA0,rect);
 m.put(0x887640,mouseAt);m.put(mouseAt,mouseVT);m.put(mouseAt+4,shape);m.put(mouseAt+8,0);m.put(mouseAt+12,0);
 const uint32_t funcs[]{0,0x7B8A00,0,0x7B9930,0x7B9750};m.put(mouseVT,funcs);
 const uint16_t header[]{0,2,2,2};m.put(shape,header);
 for(int i=0;i<2;++i){uint32_t at=shape+8+i*24;m.put(at+4,uint16_t(2));m.put(at+6,uint16_t(2));m.put(at+20,uint32_t(56+i*4));}
 const uint8_t pix[]{1,1,1,1,2,2,2,2};m.put(shape+56,pix);
 m.put(0x87F6C8,converter);m.put(converter,uint32_t(0x7E5358));m.put(converter+4,uint32_t(2));m.put(converter+0x16C,1);m.put(converter+0x174,palette);
 const int shifts[]{11,3,0,3,5,2};m.put(0x8A0DD0,shifts);m.put(palette+2,uint16_t(0xf800));m.put(palette+4,uint16_t(0x07e0));
}
int main(){
 using namespace renderupgrade;Request request;setup(request);auto mutableFrame=std::make_shared<Frame>();mutableFrame->request=request;mutableFrame->image=std::make_shared<const std::vector<uint32_t>>(size_t(request.width)*request.height,0xff123456);std::shared_ptr<const Frame>frame=mutableFrame;
 std::vector<overlaycompositor::Surface> layers;
 assert(feedback::update(request,frame,0,0,layers)&&layers.size()==1&&layers[0].id==feedback::CursorLayer);
 assert((*layers[0].pixels)[0]==0xfff80000);size_t decoded=externalReader.pixelReads;
 for(int n=0;n<100;++n){layers.clear();++cursorPosition.x;++clockNow;
  const auto calls=externalReader.reads,bytes=externalReader.readBytes;
  assert(feedback::update(request,frame,0,0,layers)&&layers.size()==1);
  assert(layers[0].rect.x==cursorPosition.x);
  assert(externalReader.pixelReads==decoded); // moving a cached cursor never decodes pixels
  assert(externalReader.reads-calls<120&&externalReader.readBytes-bytes<4096);
 }
 for(int n=0;n<50;++n){layers.clear();externalReader.put(mouseAt+8,n%2);++clockNow;
  assert(feedback::update(request,frame,0,0,layers)&&layers.size()==1);
  assert((*layers[0].pixels)[0]==(n%2?0xff00fc00u:0xfff80000u)); // current frame on first presentation, no async wait
 }
 externalReader.put(palette+2,uint16_t(0x001f));externalReader.put(mouseAt+8,0);layers.clear();
 assert(feedback::update(request,frame,0,0,layers)&&(*layers[0].pixels)[0]==0xff0000f8u);
 // A torn selection read is handled by the complete-group lease below,
 // rather than stacking a second independently renewed selection lease.
 externalReader.failSelection=true;clockNow+=50;layers.clear();assert(!feedback::update(request,frame,0,0,layers));
 clockNow+=120;layers.clear();assert(!feedback::update(request,frame,0,0,layers));
 externalReader.failSelection=false;layers.clear();assert(feedback::update(request,frame,0,0,layers));
 systemCursor=true;layers.clear();assert(feedback::update(request,frame,0,0,layers)&&layers.empty());systemCursor=false;
 overlaystack::entries.push_back({overlaystack::Layer::Cursor,1001});layers.clear();assert(feedback::update(request,frame,0,0,layers)&&layers.empty());overlaystack::entries.clear();
 externalReader.put(mouseAt+12,-1);layers.clear();assert(feedback::update(request,frame,0,0,layers)&&layers.empty());
 externalReader.put(mouseAt+12,0);
 renderpresentation::GroupLease<Request,Frame> group;
 auto publish=[&](const Request&r){
  layers.clear();
  if(feedback::update(r,frame,0,0,layers)){
   layers.push_back({overlaycompositor::RenderUpgrade,110,r.context,{0,0,r.width,r.height},true,255,frame->image});
   group.publish(r,frame,layers,clockNow);
  }else if(!feedback::unsafeView&&group.retain(r,clockNow))layers=group.layers();
  else{group.reset();layers.clear();}
  return !layers.empty();
 };
 assert(publish(request));const uint32_t lastGood=clockNow;
 for(int n=1;n<=7;++n){clockNow=lastGood+uint32_t(n)*15;externalReader.failSelection=n%3==0;externalReader.failMouse=n%3==1;externalReader.failView=n%3==2;
  assert(publish(request));assert(layers.back().pixels==frame->image); // never a full native frame on transient feedback reads
 }
 clockNow=lastGood+120;assert(!publish(request)); // failures never renew lastGood
 externalReader.failSelection=externalReader.failMouse=externalReader.failView=false;assert(publish(request));
 externalReader.failMouse=true;auto changed=request;changed.detail=0;++clockNow;assert(!publish(changed));
 externalReader.failMouse=false;assert(publish(request));
 externalReader.put(request.view.tactical+0xB0,request.view.cameraX+1);++clockNow;assert(!publish(request)&&feedback::unsafeView);
 layers.clear();assert(!feedback::update(request,frame,0,0,layers));
 assert(overlaycompositor::host().removals==0);feedback::shutdown();assert(overlaycompositor::host().removals==1);
 std::puts("PASS: actual feedback host, 100 current-position cache hits, 50 immediate cursor frame changes, palette refresh, bounded selection/group read grace, native/external cursor and immediate viewport/config rejection");
}
