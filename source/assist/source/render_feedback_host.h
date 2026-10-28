// Included inside namespace renderupgrade, after the immutable scene worker.
// All UI feedback shares the compositor HWND. No mouse messages are consumed,
// no game calls, cursor hiding, selection writes or extra game hooks are used.
namespace feedback {
namespace nf=external::nativefeedback;
constexpr uint64_t SelectionLayer=0x52460001,CursorLayer=0x52460002;
struct CursorRequest {uint32_t pid=0,match=0,tick=0;overlaycompositor::Context context;external::mainview::View view;};
struct CursorResult {CursorRequest request;nf::CursorFrame cursor;};
static external::ReadSession cursorReader;
static nf::CursorCache cursorCache;
static uint32_t cursorPID=0,cursorEpoch=0,cursorSubmitted=0;
static asyncwork::Latest<CursorRequest,CursorResult> cursorWorker([](const CursorRequest&r)->std::shared_ptr<const CursorResult>{
 if(cursorPID!=r.pid||cursorEpoch!=r.match){cursorReader.close();cursorCache.clear();cursorPID=r.pid;cursorEpoch=r.match;}
 if(!cursorReader.open(r.pid))return {};
 auto read=[](uint32_t p,void*out,size_t n){return cursorReader.readFresh(p,out,n);};
 external::mainview::View before,after;
 if(!external::mainview::sample(read,before)||!external::mainview::sameViewport(before,r.view)||before.frame<r.view.frame)return {};
 nf::CursorFrame cursor;
 if(!cursorCache.capture(read,[&](const nf::Mouse&m,const std::array<uint32_t,256>&pal,nf::CursorImage&out){
  external::voxel::Image im;std::string reason;
  if(!external::voxel::sprite(read,m.shape,pal,im,m.frame,&reason,255))return false;
  out={im.width,im.height,im.x+im.canvasWidth/2,im.y+im.canvasHeight/2,std::move(im.pixels)};return true;
 },cursor))return {};
 if(!external::mainview::sample(read,after)||!external::mainview::sameViewport(before,after)||after.frame<before.frame)return {};
 auto out=std::make_shared<CursorResult>();out->request=r;out->cursor=std::move(cursor);return out;
});
static std::vector<nf::Primitive> oldPrimitives;
static std::shared_ptr<const std::vector<uint32_t>> selectionPixels;
static nf::Rect selectionBounds;
static bool live=false;
static void hide(){
 overlaycompositor::host().replaceGroup({overlaycompositor::RenderUpgrade,SelectionLayer,CursorLayer},{});
 oldPrimitives.clear();selectionPixels.reset();selectionBounds={};
 if(live){cursorWorker.invalidate();live=false;}
}
static void shutdown(){hide();cursorWorker.shutdown();}
static bool externalCursor(){
 std::lock_guard<std::mutex>lock(overlaystack::mutex);
 for(const auto&e:overlaystack::entries)if(e.layer==overlaystack::Layer::Cursor&&e.window&&IsWindowVisible(e.window))return true;
 return false;
}
static bool update(const Request&r,const std::shared_ptr<const Frame>&scene,int left,int top,std::vector<overlaycompositor::Surface>&layers){
 if(!scene){hide();return false;}live=true;
 auto read=[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(read,before)||!external::mainview::sameViewport(before,r.view)||before.frame<r.view.frame){hide();return false;}
 nf::Band band;std::vector<nf::Selected> selected;
 const bool bandOK=nf::band(read,r.view.tactical,band);
 const bool selectionOK=nf::selected(read,scene->objects,selected);
 if(!bandOK||!selectionOK){hide();return false;}
 const double sx=double(r.width)/r.view.width,sy=double(r.height)/r.view.height;
 auto primitives=nf::selectionPrimitives(selected,band,{0,0,r.width,r.height},sx,sy,scene->yellow,scene->red);
 external::mainview::View after;
 if(!external::mainview::sample(read,after)||!external::mainview::sameViewport(before,after)||after.frame<before.frame){hide();return false;}
 if(primitives.empty()){selectionPixels.reset();oldPrimitives.clear();}
 else{
  if(!selectionPixels||primitives!=oldPrimitives){
   nf::Canvas canvas;if(!nf::paint(primitives,canvas,selectionBounds)){selectionPixels.reset();oldPrimitives.clear();}
   else{selectionPixels=std::make_shared<const std::vector<uint32_t>>(std::move(canvas.pixels));oldPrimitives=std::move(primitives);}
  }
  if(selectionPixels)layers.push_back({SelectionLayer,200,r.context,{left+selectionBounds.x,top+selectionBounds.y,selectionBounds.w,selectionBounds.h},true,255,selectionPixels});
 }
 // Mirror only the covered portion of the original software cursor.
 CURSORINFO ci{};ci.cbSize=sizeof ci;POINT pos{};
 if(!GetCursorInfo(&ci)||!GetCursorPos(&pos)||!ScreenToClient(game,&pos)){layers.clear();return false;}
 bool sysCursor=(ci.flags&CURSOR_SHOWING)!=0;
 bool external=externalCursor();
 if(sysCursor||external||pos.x<left||pos.y<top||pos.x>=left+r.width||pos.y>=top+r.height)return true;
 uint32_t now=GetTickCount();auto result=cursorWorker.get();bool covered=false;
 nf::Mouse current;const bool mouseOK=nf::mouse(read,current);
 if(mouseOK&&current.refCount<0){return true;}
 if(result&&result->cursor.image&&result->request.pid==r.pid&&result->request.match==r.match&&
    result->request.context.token==r.context.token&&DWORD(now-result->request.tick)<600&&
    external::mainview::sameViewport(result->request.view,r.view)){
  if(mouseOK&&current.same(result->cursor.mouse)&&nf::cursorVisible(true,external,sysCursor,current)){
   const auto&im=*result->cursor.image;
   auto dst=nf::cursorRect(pos.x-left,pos.y-top,im.x,im.y,im.width,im.height,current.hotX,current.hotY,sx,sy);
   auto clip=nf::intersect(dst,{0,0,r.width,r.height});
   if(clip.valid()&&uint64_t(clip.w)*clip.h<=1024*1024){
    auto pixels=std::make_shared<std::vector<uint32_t>>(size_t(clip.w)*clip.h);
    for(int y=0;y<clip.h;++y)for(int x=0;x<clip.w;++x){
     const int ix=std::clamp(int(int64_t(clip.x+x-dst.x)*im.width/dst.w),0,im.width-1);
     const int iy=std::clamp(int(int64_t(clip.y+y-dst.y)*im.height/dst.h),0,im.height-1);
     (*pixels)[size_t(y)*clip.w+x]=im.pixels[size_t(iy)*im.width+ix];
    }
    layers.push_back({CursorLayer,300,r.context,{left+clip.x,top+clip.y,clip.w,clip.h},false,255,std::move(pixels)});covered=true;
   }
  }
 }
 if(!cursorSubmitted||DWORD(now-cursorSubmitted)>=8){cursorSubmitted=now;cursorWorker.submit({r.pid,r.match,now,r.context,r.view});}
 // Keep the original visible until a verified replacement is ready.
 if(!covered)layers.clear();
 return covered;
}
}
