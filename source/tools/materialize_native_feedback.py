"""Apply narrow reviewed integration edits; resulting C++ is committed by CI."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]

def insert(path, marker, sentinel, addition):
    p=root/path
    text=p.read_text(encoding='utf-8')
    if sentinel in text:
        return
    if text.count(marker)!=1:
        raise SystemExit(f'Expected one integration marker in {path}: {marker}')
    p.write_text(text.replace(marker,addition+marker),encoding='utf-8')

insert('external_reader/source/radar_scene.h',' struct NativeStats {','bool feedbackBounds(',r''' // Read-only metadata from the sprite actually displayed; no second decode.
 bool feedbackBounds(const Marker&m,int&x,int&y,int&w,int&h)const{
  auto found=sprites_.find(imageKey(m));
  if(found==sprites_.end()||!found->second.ready){auto old=displayed_.find({m.object,m.id});if(old!=displayed_.end())found=sprites_.find(old->second);}
  if(found==sprites_.end()||!found->second.ready)return false;
  const auto&i=found->second.image;x=i.x;y=i.y;w=i.width;h=i.height;
  return w>0&&h>0&&w<=2048&&h<=2048;
 }
 bool feedbackVisibility(const std::vector<uint8_t>*&out)const{
  out=nullptr;if(!complete_)return false;if(complete_->reveal)return true;
  if(maskSource_!=complete_||mask_.width!=width_||mask_.height!=height_||mask_.alpha.size()!=size_t(width_)*height_)return false;
  out=&mask_.alpha;return true;
 }
''')
p=root/'shared/source/overlay_compositor.h'
text=p.read_text(encoding='utf-8')
if '#include "overlay_group.h"' not in text:
    if text.count('#include "overlay_composition.h"')!=1:raise SystemExit('Compositor include marker changed')
    text=text.replace('#include "overlay_composition.h"','#include "overlay_composition.h"\n#include "overlay_group.h"\n#include <atomic>')
    p.write_text(text,encoding='utf-8')
insert('shared/source/overlay_compositor.h',' Context capture(uintptr_t window,uint32_t pid){','bool replaceGroup(',r''' // A single lock publishes/removes a complete scene/input group.
 bool replaceGroup(const std::vector<uint64_t>&ids,std::vector<Surface>layers){
  std::lock_guard<std::mutex>lock(mutex);
  if(!overlaycomposition::groupIds(ids)||!enabled||error)return false;
  if(!overlaycomposition::groupValid(ids,layers,context)){
   bool changed=false;for(auto id:ids)changed|=nodes.erase(id)!=0;
   if(changed){++revision;cv.notify_one();}return false;
  }
  bool changed=false;const auto now=GetTickCount64();
  for(auto id:ids){
   auto next=std::find_if(layers.begin(),layers.end(),[&](const Surface&s){return s.id==id;});
   if(next==layers.end()){changed|=nodes.erase(id)!=0;continue;}
   auto&node=nodes[id];const auto&old=node.surface;
   changed|=old.pixels!=next->pixels||old.rect!=next->rect||old.order!=next->order||old.opacity!=next->opacity||old.cameraBound!=next->cameraBound||
    old.context.token!=next->context.token||old.context.cameraX!=next->context.cameraX||old.context.cameraY!=next->context.cameraY;
   node={std::move(*next),now};
  }
  if(changed){++revision;cv.notify_one();}return true;
 }
''')
p=root/'assist/source/tactical_controls.h'
text=p.read_text(encoding='utf-8')
old='交互小地图、副相机、光标和副屏尚未合并；原生选择框、特效还原及实机联机验证未完成。'
new='框选读取原生矩形；选中框按已绘素材边界，血条和原帧软件光标进入统一层；电弧近似重绘。激光/波纹/粒子及完整附加标记待补。原高刷光标、交互窗口与副屏尚未合并；实机联机验证未完成。'
if '框选读取原生矩形' in text and '电弧近似重绘' in text and '激光/波纹/粒子及完整附加标记待补' in text:
    pass  # Additional palette/scale help may extend the already-integrated text.
elif new not in text:
    if text.count(old)!=1:raise SystemExit('Render feedback help marker changed')
    p.write_text(text.replace(old,new),encoding='utf-8')
print('PASS: materialized scene metadata, atomic layer groups and honest feature help')
