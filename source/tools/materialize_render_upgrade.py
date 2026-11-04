#!/usr/bin/env python3
"""Apply reviewed integration edits once; CI commits ordinary resulting sources.
This helper exists because the connector accepts text files, not a local git
working tree. No runtime code generation; no game files are accessed.
"""
from pathlib import Path
import hashlib
ROOT=Path(__file__).resolve().parents[2]
expected = {'source/assist/source/extra_settings_io.h': '086d0772105316509fb0730b8ab53bd3c6a26f7156dc56ef3f95c6245f80906b', 'source/assist/source/main.cpp': 'c34daad65b0a5a44878ed3b4f32737204de881db6ec8aceb8156abbce3fb9eb4', 'source/assist/source/tactical_controls.h': '021179edf21251511f46435bd42e149c4ac52cae7e3611db988cf81ebb24ab84', 'source/external_reader/source/native_canvas.h': 'ce277d23d94a1c3298c1cd2b9d02abdac20787bd38f1797d1263fba10ffd3460', 'source/external_reader/source/radar_scene.h': '53518501ed6a68444aafdc83894ed362b101632bc7d85763cd63482e1b5c99d0', 'source/external_reader/source/radar_shroud_mask.h': '0eb9cb3389f7a0a94b680852d63436911a894418218abe8f792c85790fd4623e', 'source/shared/source/overlay_stack.h': 'eaf0c9682ffab0411327aa95988f291faa3576b5314f7776b7ae52d452746abf', 'source/unit_monitor/source/main.cpp': '9fa43a6b7771f8bf70060cc2550ea266dd3b9c43c41d02f0d91d43128eecf7e9'}
finished = {'source/assist/source/extra_settings_io.h': 'c93f72fb9cfe9c987b70be46ce26e2543508b0e2228437958f3b4a41e1acbbeb', 'source/assist/source/main.cpp': 'a932ac321de2a2d9518a3172ba4430924f6518796b25175d77cd2be84cb99161', 'source/assist/source/tactical_controls.h': 'f31e2d463884e3db74c3acb8a29fad95c8fac4b69beaa8d54d06ca13198aafe1', 'source/external_reader/source/native_canvas.h': 'a25326ff56492511735cbd0e3c43679fb6a1c503ac06694bc30aff8573ef0765', 'source/external_reader/source/radar_scene.h': 'a3975e522b5745ccc66f84b8fad47e68dd3376532d206914d8f6dd6ad6ceb2de', 'source/external_reader/source/radar_shroud_mask.h': 'e98d12fcb1afdc9caaea59da7a617c9b39763b5f7da276105c33159d941a9302', 'source/shared/source/overlay_stack.h': 'ecc4ccaf7e54b19336fed14412ff09f5137f13a6b1be057dc20522b6090c74ed', 'source/unit_monitor/source/main.cpp': '653a7cec39ea997de9a92ddda216d48b3d18f5f7c369b296f2d42a04475081c4'}
def digest(p):return hashlib.sha256((ROOT/p).read_bytes().replace(b'\r\n',b'\n')).hexdigest()
if all(digest(p)==h for p,h in finished.items()):
    print('Integration already materialized');raise SystemExit(0)
for p,h in expected.items():
    if digest(p)!=h:raise SystemExit('Unexpected base; refusing to overwrite: '+p)

from pathlib import Path
root=ROOT
def mod(path,old,new):
 p=root/path;s=p.read_text();assert s.count(old)==1,(path,s.count(old),old[:80]);p.write_text(s.replace(old,new))
# Native crop at native pixel coordinates; leaves uncovered pixels transparent.
p=root/'source/external_reader/source/native_canvas.h';s=p.read_text();where=' bool present(int width,int height,std::vector<uint32_t>&out,double spanX=-1,double spanY=-1)const{'
assert s.count(where)==1
s=s.replace(where,''' // Crop a native-pixel scene BEFORE presentation scaling. This is deliberately
 // independent of the radar's whole-map downscale. Outside backing falls back
 // to the underlying game through transparent pixels instead of stretching.
 bool presentRegion(int width,int height,std::vector<uint32_t>&out,
                    double x,double y,double spanX,double spanY)const{
  if(failed_||width<1||height<1||width>8192||height>8192||
     uint64_t(width)*height>16*1024*1024||!std::isfinite(x)||!std::isfinite(y)||
     !std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||
     std::abs(x)>1e7||std::abs(y)>1e7||spanX>32768||spanY>32768)return false;
  out.assign(size_t(width)*height,0);
  const double sx=spanX/width,sy=spanY/height;
  if(sx==1&&sy==1&&std::floor(x)==x&&std::floor(y)==y){
   // Contiguous page runs, not per-pixel map lookups, in the native case.
   for(int dy=0;dy<height;++dy){int yy=int(y)+dy;if(yy<0||yy>=height_)continue;
    int lo=std::max(0,-int(x)),hi=std::min(width,width_-int(x));
    for(int dx=lo;dx<hi;){int xx=int(x)+dx,end=std::min(hi,dx+PageSize-xx%PageSize);
     auto page=pages_.find(key(xx/PageSize,yy/PageSize));auto*dest=out.data()+size_t(dy)*width+dx;
     if(page==pages_.end())std::fill(dest,dest+end-dx,background_);
     else std::copy_n(page->second->surface.color.data()+size_t(yy%PageSize)*PageSize+xx%PageSize,end-dx,dest);
     dx=end;
    }
   }return true;
  }
  Filter filter(*this);
  for(int dy=0;dy<height;++dy)for(int dx=0;dx<width;++dx){
   double l=x+dx*sx,t=y+dy*sy,r=l+sx,b=t+sy;
   if(r<=0||b<=0||l>=width_||t>=height_)continue;
   out[size_t(dy)*width+dx]=filter.pixel(l,t,r,b);
  }return true;
 }
'''+where);p.write_text(s)
# source crop mask coordinates and viewport budget; old calls unchanged.
mod(Path('source/external_reader/source/radar_shroud_mask.h'),'bool fog,Load load){','bool fog,Load load,double originX=0,double originY=0){')
mod(Path('source/external_reader/source/radar_shroud_mask.h'),'w>2048||h>2048||','w>8192||h>8192||uint64_t(w)*h>16*1024*1024||!std::isfinite(originX)||!std::isfinite(originY)||')
mod(Path('source/external_reader/source/radar_shroud_mask.h'),'const double left=centerX+image.x,top=centerY+image.y;','const double left=centerX+image.x-originX,top=centerY+image.y-originY;')
mod(Path('source/external_reader/source/radar_shroud_mask.h'),'uint32_t p=pixels[n],v=0xff000000u;','uint32_t p=pixels[n];if(!(p>>24))continue;uint32_t v=p&0xff000000u;')
# Preserve minimap rendering default; optional main-camera crop.
p=root/'source/external_reader/source/radar_scene.h';s=p.read_text();s=s.replace('template<class Assets>class SceneRenderer {','struct SceneRegion {double x=0,y=0,width=0,height=0;bool active=false;};\ntemplate<class Assets>class SceneRenderer {',1)
s=s.replace('uint64_t nativeCells_=0,basePresents_=0;','uint64_t nativeCells_=0,basePresents_=0;SceneRegion region_{};',1)
s=s.replace('Read read,Assets&assets){','Read read,Assets&assets,SceneRegion region={}){',1)
s=s.replace('width>2048||height>2048)return {};','width>(region.active?8192:2048)||height>(region.active?8192:2048)||uint64_t(width)*height>16*1024*1024)return {};',1)
needle='  if(width!=width_||height!=height_){width_=width;height_=height;baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}'
assert needle in s;s=s.replace(needle,'''  if(region.active&&(!std::isfinite(region.x)||!std::isfinite(region.y)||!std::isfinite(region.width)||!std::isfinite(region.height)||region.width<=0||region.height<=0||std::abs(region.x)>1e7||std::abs(region.y)>1e7||region.width>32768||region.height>32768))return {};
  if(width!=width_||height!=height_||region.x!=region_.x||region.y!=region_.y||region.width!=region_.width||region.height!=region_.height||region.active!=region_.active){width_=width;height_=height;region_=region;baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}''')
s=s.replace('if(!background_.present(width,height,base->pixels,spanX_,spanY_))return {};','if(!(region.active?background_.presentRegion(width,height,base->pixels,region.x,region.y,region.width,region.height):background_.present(width,height,base->pixels,spanX_,spanY_)))return {};',1)
s=s.replace('mask_.build(*complete_,width,height,spanX_,spanY_,fog,','mask_.build(*complete_,width,height,region.active?region.width:spanX_,region.active?region.height:spanY_,fog,',1)
s=s.replace('return assets.shroud(isFog,index,out);}))','return assets.shroud(isFog,index,out);},region.active?region.x:0,region.active?region.y:0))',1)
s=s.replace('auto markers=frame.markers;for(auto&m:markers)', '''auto markers=frame.markers;
  if(region.active)markers.erase(std::remove_if(markers.begin(),markers.end(),[&](const Marker&m){
   double x=(double(m.x)-m.y)*30/256.+geo.offsetX*30,y=(double(m.x)+m.y)*15/256.-geo.offsetY*15-screenHeight(m.z,projectionZScale(read));
   return x<region.x-512||x>region.x+region.width+512||y<region.y-512||y>region.y+region.height+512;
  }),markers.end());
  for(auto&m:markers)''',1)
s=s.replace('if(!view.presentDirty(width,height,out->pixels,spanX_,spanY_))','if(!(region.active?view.presentRegion(width,height,out->pixels,region.x,region.y,region.width,region.height):view.presentDirty(width,height,out->pixels,spanX_,spanY_)))',1)
s=s.replace('geo.offsetX*30)*width/spanX_)','geo.offsetX*30-(region.active?region.x:0))*width/(region.active?region.width:spanX_))')
s=s.replace('geo.offsetY*15)*height/spanY_)','geo.offsetY*15-(region.active?region.y:0))*height/(region.active?region.height:spanY_))')
p.write_text(s)

from pathlib import Path
r=ROOT
p=r/'source/assist/source/main.cpp';s=p.read_text()
s=s.replace('#include "../../shared/source/overlay_stack.h"','#include "../../shared/source/overlay_stack.h"\n#include "../../shared/source/overlay_compositor.h"',1)
s=s.replace('namespace minimap {static void hide();', 'namespace renderupgrade {static void stop();static void shutdown();}\nnamespace minimap {static void hide();',1)
s=s.replace('static bool minimapEnabled=false,','static bool renderUpgradeEnabled=false,unifiedOverlayEnabled=false;static int renderUpgradeLayer=0;\nstatic bool minimapEnabled=false,',1)
# All logical hides withdraw only the world layer, not the scene or statistics.
s=s.replace('ShowWindow(overlay,SW_HIDE);','hideWorldOverlay();')
needle='static uint32_t lastReceiptWrite=0,lastReceiptMatch=0;'
assert needle in s;s=s.replace(needle,'''static void hideWorldOverlay(){overlaycompositor::host().hide(overlaycompositor::World);if(overlay)ShowWindow(overlay,SW_HIDE);}
'''+needle,1)
s=s.replace('static void detach(){','static void detach(){renderupgrade::stop();',1)
s=s.replace('#include "minimap_overlay.h"','#include "minimap_overlay.h"\n#include "render_upgrade_host.h"',1)
s=s.replace('struct Job {uint32_t pid,match,tick;int w,h,clientW,clientH;POINT origin;std::function<void(Graphics&)> draw;};','struct Job {uint32_t pid,match,tick;int w,h,clientW,clientH;POINT origin;std::function<void(Graphics&)> draw;overlaycompositor::Context context;POINT relative{};};',1)
s=s.replace('struct Frame {uint32_t pid,match,tick;int w,h,clientW,clientH;POINT origin;std::vector<uint32_t> pixels;};','struct Frame {uint32_t pid,match,tick;int w,h,clientW,clientH;POINT origin;std::vector<uint32_t> pixels;overlaycompositor::Context context;POINT relative{};};',1)
s=s.replace('out->origin=j.origin;out->pixels.resize','out->origin=j.origin;out->context=j.context;out->relative=j.relative;out->pixels.resize',1)
s=s.replace('static void submit(Job input){active=true;worker.submit(std::move(input));}', '''static void submit(Job input){
 active=true;input.context=overlaycompositor::host().capture(uintptr_t(game),pid);
 if(input.context.token&&(input.context.cameraX!=snap.cameraX||input.context.cameraY!=snap.cameraY))input.context.token=0;
 POINT origin{};ClientToScreen(game,&origin);input.relative={input.origin.x-origin.x,input.origin.y-origin.y};
 worker.submit(std::move(input));
}''',1)
s=s.replace('static void invalidate(){if(!active)return;','static void invalidate(){overlaycompositor::host().hide(overlaycompositor::World);if(!active)return;',1)
a=s.index('static void present(){auto frame=worker.get();',s.index('namespace worldrender'))
b=s.index('\n}\n}\nstatic void paint()',a)
s=s[:a]+'''static void present(){auto frame=worker.get();
 auto context=overlaycompositor::host().capture(uintptr_t(game),pid);
 if(!frame||frame->pid!=pid||frame->match!=snap.match){overlaycompositor::host().hide(overlaycompositor::World);return;}
 RECT client{};GetClientRect(game,&client);if(client.right!=frame->clientW||client.bottom!=frame->clientH)return;
 if(context.token){
  // Immutable pixels are shared with the single compositor; no extra HWND is
  // shown for labels, passenger bubbles, timers, formation or world markers.
  auto pixels=std::shared_ptr<const std::vector<uint32_t>>(frame,&frame->pixels);
  if(frame->context.token!=context.token||frame->context.cameraX!=context.cameraX||frame->context.cameraY!=context.cameraY){overlaycompositor::host().hide(overlaycompositor::World);return;}
  overlaycompositor::host().publish({overlaycompositor::World,10,frame->context,{frame->relative.x,frame->relative.y,frame->w,frame->h},true,255,std::move(pixels)});
  if(IsWindowVisible(overlay))ShowWindow(overlay,SW_HIDE);
  displayed=frame;return;
 }
 overlaycompositor::host().hide(overlaycompositor::World);
 if((frame==displayed&&IsWindowVisible(overlay))||DWORD(GetTickCount()-frame->tick)>500)return;
 if(!surface.resize(frame->w,frame->h))return;memcpy(surface.bits,frame->pixels.data(),frame->pixels.size()*4);
 POINT origin{};ClientToScreen(game,&origin);origin.x+=frame->relative.x;origin.y+=frame->relative.y;POINT zero{};
 SIZE size{frame->w,frame->h};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
 bool ok=UpdateLayeredWindow(overlay,nullptr,&origin,&size,surface.dc,&zero,0,&blend,ULW_ALPHA)!=0;
 overlayRecovery.submitted(ok);if(ok){displayed=frame;++displayCounters.uploads;overlaystack::place(overlay,overlaystack::Layer::World,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);}'''+s[b:]
s=s.replace('if(!battlefield||snap.renderStatus!=1){worldrender::invalidate();','if(!battlefield||snap.renderStatus!=1){renderupgrade::stop();worldrender::invalidate();',1)
needle=' RECT r{};GetClientRect(game,&r);POINT origin{};ClientToScreen(game,&origin);\n minimap::update'
assert needle in s
s=s.replace(needle,''' RECT r{};GetClientRect(game,&r);POINT origin{};ClientToScreen(game,&origin);
 renderupgrade::update(origin,r,battlefield);
 {static uint64_t prior=0;auto c=overlaycompositor::host().capture(uintptr_t(game),pid);if(prior!=c.token){prior=c.token;++paintRevision;}}
 minimap::update''',1)
needle='if(!changed){if(!worldrender::displayed)return;'
assert needle in s;s=s.replace(needle,'if(!changed){if(overlaycompositor::host().capture(uintptr_t(game),pid).token)return;if(!worldrender::displayed)return;',1)
s=s.replace('case WM_COMMAND:{int id=LOWORD(wp);','''case WM_COMMAND:{int id=LOWORD(wp);
 if(id==9300||id==9301||id==9302){
  if(updatingControls)return 0;
  if(id==9302){if(HIWORD(wp)!=CBN_SELCHANGE)return 0;renderUpgradeLayer=std::clamp(int(SendDlgItemMessageW(w,id,CB_GETCURSEL,0,0)),0,1);}
  else{if(HIWORD(wp)!=BN_CLICKED)return 0;bool on=SendDlgItemMessageW(w,id,BM_GETCHECK,0,0)==BST_CHECKED;if(id==9300)renderUpgradeEnabled=on;else unifiedOverlayEnabled=on;}
  renderupgrade::stop();++paintRevision;changedSetting();paint();
  if(!renderUpgradeEnabled)SetWindowTextW(GetDlgItem(w,9303),unifiedOverlayEnabled?L"统一覆盖层：世界信息＋主屏统计；交互窗口尚未合并":L"渲染升级：关闭");
  return 0;
 }''',1)
s=s.replace('case WM_DESTROY:worldrender::worker.shutdown();','case WM_DESTROY:renderupgrade::shutdown();worldrender::worker.shutdown();',1)
p.write_text(s)
# Persistence independent of existing minimap options and Runtime IPC.
p=r/'source/assist/source/extra_settings_io.h';s=p.read_text()
s=s.replace('static void loadAdditionalSettings(){','''static void loadAdditionalSettings(){
 renderUpgradeEnabled=value(L"Display",L"RenderUpgrade",0)!=0;
 unifiedOverlayEnabled=value(L"Display",L"UnifiedOverlay",0)!=0;
 renderUpgradeLayer=std::clamp(value(L"Display",L"RenderUpgradeLayer",0),0,1);''',1)
needle=' write(L"Display",L"MinimapMode",minimapMode);';assert needle in s;s=s.replace(needle,''' write(L"Display",L"RenderUpgrade",renderUpgradeEnabled);
 write(L"Display",L"UnifiedOverlay",unifiedOverlayEnabled);
 write(L"Display",L"RenderUpgradeLayer",renderUpgradeLayer);
'''+needle,1);p.write_text(s)
p=r/'source/assist/source/tactical_controls.h';s=p.read_text();needle='toggle(830,L"游戏内战术小地图",minimapEnabled);next();';assert needle in s;s=s.replace(needle,'''toggle(9300,L"渲染升级（全视口 · 实验性）",renderUpgradeEnabled);next();
label(L"渲染层级",20,260);choices(combo(9302),{L"覆盖层顶部",L"信息层下方"},renderUpgradeLayer);next();
toggle(9301,L"统一覆盖层（世界信息＋主屏统计）",unifiedOverlayEnabled);next();
note(L"渲染升级自动使用统一画布，只读重绘原生视口，不改变视野或游戏代码。镜头移动或素材未就绪时回退原画面。交互小地图、副相机、光标和副屏尚未合并；原生选择框、特效还原及实机联机验证未完成。",88);
item(L"STATIC",L"渲染升级：实验性，默认关闭",20,605,9303,0,48);next(48);
'''+needle,1);p.write_text(s)
# Passive statistics joins the same EXE global compositor; secondary display is
# intentionally unchanged (its drag/edit/input has a separate lifecycle).
p=r/'source/unit_monitor/source/main.cpp';s=p.read_text()
s=s.replace('#include "../../shared/source/overlay_stack.h"','#include "../../shared/source/overlay_stack.h"\n#include "../../shared/source/overlay_compositor.h"',1)
s=s.replace('ShowWindow(overlay,SW_HIDE);','hidePrimaryStats();')
needle='static external::ReadSession reader;';assert needle in s;s=s.replace(needle,'''static void hidePrimaryStats(){overlaycompositor::host().hide(overlaycompositor::Statistics);if(overlay)ShowWindow(overlay,SW_HIDE);}
'''+needle,1)
needle=' statsVisible(true);\n if(repair||alpha!=paintedAlpha'
assert needle in s;s=s.replace(needle,''' statsVisible(true);
 const auto context=!demo?overlaycompositor::host().capture(uintptr_t(game),pid):overlaycompositor::Context{};
 if(context.token){
  static uint64_t priorToken=0;static DWORD changedAt=0;
  if(priorToken!=context.token){priorToken=context.token;changedAt=GetTickCount();renderDirty=true;}
  // A new match/viewport may not reuse a bitmap sampled before it existed.
  if(renderedFrame&&renderedFrame->request.snap&&int32_t(renderedFrame->request.snap->tick-changedAt)>=0){
   POINT base{};ClientToScreen(game,&base);
   auto pixels=std::shared_ptr<const std::vector<uint32_t>>(renderedFrame,&renderedFrame->pixels);
   overlaycompositor::host().publish({overlaycompositor::Statistics,20,context,{origin.x-base.x,origin.y-base.y,surface.w,surface.h},false,BYTE(alpha),std::move(pixels)});
  }else overlaycompositor::host().hide(overlaycompositor::Statistics);
  if(IsWindowVisible(overlay))ShowWindow(overlay,SW_HIDE);paintedAlpha=-1;return;
 }
 overlaycompositor::host().hide(overlaycompositor::Statistics);
 if(repair||alpha!=paintedAlpha''',1)
p.write_text(s)


p=ROOT/'source/shared/source/overlay_stack.h';s=p.read_text();s=s.replace('Diagnostics=80,Cursor=100','Diagnostics=80,Cursor=100,RenderUpgrade=90');p.write_text(s)
p=ROOT/'source/assist/source/tactical_controls.h';s=p.read_text().replace('L"覆盖层顶部",L"信息层下方"','L"信息覆盖层顶部（光标保留）",L"信息层下方"');p.write_text(s)
p=ROOT/'source/assist/source/main.cpp';s=p.read_text();s=s.replace(' auto context=overlaycompositor::host().capture(uintptr_t(game),pid);\n if(!frame',' auto context=overlaycompositor::host().capture(uintptr_t(game),pid);\n if(context.token&&IsWindowVisible(overlay))ShowWindow(overlay,SW_HIDE);\n if(!frame',1);p.write_text(s)
for p,h in finished.items():
    if digest(p)!=h:raise SystemExit('Result mismatch: '+p)
print('PASS: integration matches locally validated source hashes')
