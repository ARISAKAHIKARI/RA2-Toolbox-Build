// Included inside namespace renderupgrade, after the immutable scene worker.
// All UI feedback shares the compositor HWND. No mouse messages are consumed,
// no game calls, cursor hiding, selection writes or extra game hooks are used.
namespace feedback {
namespace nf=external::nativefeedback;
constexpr uint64_t SelectionLayer=0x52460001,CursorLayer=0x52460002;
// Cursor art is already resident in game memory. Capture the current small
// frame synchronously; waiting for an older async cursor withdrew the entire
// scene on every cursor animation/action change. The cache avoids pixel decode
// when only the mouse position changes. No scene-observation delay is added.
static nf::CursorCache cursorCache;
static nf::FoundationStencil foundationStencil;
static std::shared_ptr<const std::vector<overlaycomposition::Rect>> nativeCutouts;
static uint32_t cursorPID=0,cursorEpoch=0;
static overlaycompositor::Context selectionContext{};
static std::vector<nf::Primitive> oldPrimitives;
static std::vector<nf::Stroke> oldStrokes;
static std::shared_ptr<const std::vector<uint32_t>> selectionPixels;
static nf::Rect selectionBounds;
static bool live=false,unsafeView=false;
static void hide(){
 overlaycompositor::host().replaceGroup({overlaycompositor::RenderUpgrade,SelectionLayer,CursorLayer},{});
 oldPrimitives.clear();oldStrokes.clear();selectionPixels.reset();selectionBounds={};nativeCutouts.reset();
 cursorCache.clear();foundationStencil.clear();live=false;
}
static void shutdown(){hide();}
static bool externalCursor(){
 std::lock_guard<std::mutex>lock(overlaystack::mutex);
 for(const auto&e:overlaystack::entries)if(e.layer==overlaystack::Layer::Cursor&&e.window&&IsWindowVisible(e.window))return true;
 return false;
}
static bool update(const Request&r,const std::shared_ptr<const Frame>&scene,int left,int top,std::vector<overlaycompositor::Surface>&layers){
 unsafeView=false;if(!scene)return false;live=true;
 if(cursorPID!=r.pid||cursorEpoch!=r.match){cursorCache.clear();foundationStencil.clear();nativeCutouts.reset();cursorPID=r.pid;cursorEpoch=r.match;}
 if(selectionContext.token!=r.context.token||selectionContext.cameraX!=r.context.cameraX||selectionContext.cameraY!=r.context.cameraY){
  selectionPixels.reset();oldPrimitives.clear();oldStrokes.clear();nativeCutouts.reset();
 }
 selectionContext=r.context;
 auto read=[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(read,before))return false;
 if(!external::mainview::sameViewport(before,r.view)||before.frame<r.view.frame){unsafeView=true;return false;}
 nf::Band band;std::vector<nf::Selected> selected;
 const bool bandOK=nf::band(read,r.view.tactical,band);
 const bool selectionOK=nf::selected(read,scene->objects,selected);
 std::vector<nf::Stroke> strokes;std::vector<nf::Rect> cutouts;
 const bool ordersOK=nf::actionLines(read,r.view,r.width,r.height,strokes);
 const bool placementOK=foundationStencil.capture(read,r.view,r.width,r.height,cutouts);
 const double sx=double(r.width)/r.view.width,sy=double(r.height)/r.view.height;
 external::mainview::View after;
 if(!external::mainview::sample(read,after))return false;
 if(!external::mainview::sameViewport(before,after)||after.frame<before.frame){unsafeView=true;return false;}
 if(!bandOK||!selectionOK||!ordersOK||!placementOK)return false;
 if(cutouts.empty())nativeCutouts.reset();
 else{
  bool same=nativeCutouts&&nativeCutouts->size()==cutouts.size();
  if(same)for(size_t n=0;n<cutouts.size();++n){const auto&a=(*nativeCutouts)[n];const auto&b=cutouts[n];if(a.x!=b.x||a.y!=b.y||a.w!=b.w||a.h!=b.h){same=false;break;}}
  if(!same){auto spans=std::make_shared<std::vector<overlaycomposition::Rect>>();spans->reserve(cutouts.size());for(const auto&r:cutouts)spans->push_back({r.x,r.y,r.w,r.h});nativeCutouts=std::move(spans);}
 }
 {
  auto primitives=nf::selectionPrimitives(selected,band,{0,0,r.width,r.height},sx,sy,scene->yellow,scene->red);
  if(primitives.empty()&&strokes.empty()){selectionPixels.reset();oldPrimitives.clear();oldStrokes.clear();}
  else if(!selectionPixels||primitives!=oldPrimitives||strokes!=oldStrokes){
   nf::Canvas canvas;if(!nf::paintInteractions(primitives,strokes,canvas,selectionBounds))return false;
   selectionPixels=std::make_shared<const std::vector<uint32_t>>(std::move(canvas.pixels));oldPrimitives=std::move(primitives);oldStrokes=std::move(strokes);
  }
 }
 if(selectionPixels)layers.push_back({SelectionLayer,200,r.context,{left+selectionBounds.x,top+selectionBounds.y,selectionBounds.w,selectionBounds.h},true,255,selectionPixels});
 // Mirror only the covered portion of the original software cursor.
 CURSORINFO ci{};ci.cbSize=sizeof ci;POINT pos{};
 if(!GetCursorInfo(&ci)||!GetCursorPos(&pos)||!ScreenToClient(game,&pos)){layers.clear();return false;}
 bool sysCursor=(ci.flags&CURSOR_SHOWING)!=0;
 bool external=externalCursor();
 if(sysCursor||external||pos.x<left||pos.y<top||pos.x>=left+r.width||pos.y>=top+r.height)return true;
 nf::Mouse current;if(!nf::mouse(read,current))return false;
 if(current.refCount<0)return true;
 size_t cursorBytes=0,cursorCalls=0;
 auto cursorRead=[&](uint32_t p,void*out,size_t n){
  if(++cursorCalls>2048||n>1024*1024-cursorBytes)return false;
  cursorBytes+=n;return read(p,out,n);
 };
 nf::CursorFrame cursor;
 if(!cursorCache.capture(cursorRead,[&](const nf::Mouse&m,const std::array<uint32_t,256>&pal,nf::CursorImage&out){
  external::voxel::Image im;std::string reason;
  if(!external::voxel::sprite(cursorRead,m.shape,pal,im,m.frame,&reason,255))return false;
  out={im.width,im.height,im.x+im.canvasWidth/2,im.y+im.canvasHeight/2,std::move(im.pixels)};return true;
 },cursor)||!cursor.image||!current.same(cursor.mouse))return false;
 const auto&im=*cursor.image;
 auto dst=nf::cursorRect(pos.x-left,pos.y-top,im.x,im.y,im.width,im.height,current.hotX,current.hotY,sx,sy);
 auto clip=nf::intersect(dst,{0,0,r.width,r.height});
 // A valid cursor crop can lie entirely beyond the viewport boundary.
 bool covered=!clip.valid();
 if(clip.valid()&&uint64_t(clip.w)*clip.h<=1024*1024){
  auto pixels=std::make_shared<std::vector<uint32_t>>(size_t(clip.w)*clip.h);
  for(int y=0;y<clip.h;++y)for(int x=0;x<clip.w;++x){
   const int ix=std::clamp(int(int64_t(clip.x+x-dst.x)*im.width/dst.w),0,im.width-1);
   const int iy=std::clamp(int(int64_t(clip.y+y-dst.y)*im.height/dst.h),0,im.height-1);
   (*pixels)[size_t(y)*clip.w+x]=im.pixels[size_t(iy)*im.width+ix];
  }
  layers.push_back({CursorLayer,300,r.context,{left+clip.x,top+clip.y,clip.w,clip.h},false,255,std::move(pixels)});covered=true;
 }
 // Keep the original visible until a verified replacement is ready.
 if(!covered)layers.clear();
 return covered;
}
}
