#include "../../external_reader/source/watch_view.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/terrain_preview.h"
namespace watchhost {
static external::watch::Target pinned,tracking;
static external::watch::Frame frame;
struct Cached {external::voxel::Image image;DWORD retry=0;uint64_t used=0;bool ready=false;};
static std::map<std::pair<uint32_t,uint32_t>,Cached> images;
static std::map<std::pair<uint32_t,uint32_t>,Cached> tiles,overlays;
static std::array<uint32_t,256> terrainPalette{};
static uint64_t serial=0;static DWORD last=0,process=0;static uint32_t epoch=0;static bool shown=false;
static constexpr int Width=320,Height=288;static int left=12,top=48;
static bool read(uint32_t p,void*out,size_t n){return externalReader.read(p,out,n);}
static void reset(){pinned=tracking={};frame={};images.clear();tiles.clear();overlays.clear();terrainPalette={};last=0;if(shown){shown=false;++paintRevision;}}
static void pin(){
 if(!enabled||!extra.watchEnabled||!foregroundGame()||snap.chat||formationInputOwns())return;
 if(process!=pid||epoch!=commandMatch()){reset();process=pid;epoch=commandMatch();}
 auto t=external::watch::manual(read);if(!t){notify(L"请将鼠标放到单位上，或先选中一个单位。");return;}
 pinned=pinned==t?external::watch::Target{}:t;last=0;++paintRevision;notify(pinned?L"已固定观察此单位；再次对它按观察键可解除。":L"已解除手动观察，恢复自动目标。");
}
static void update(){
 if(process!=pid||epoch!=commandMatch()){reset();process=pid;epoch=commandMatch();}
 bool active=enabled&&extra.watchEnabled&&!snap.chat&&snap.renderStatus==1&&snap.width>=360&&snap.height>=320;
 if(!active){if(shown){shown=false;++paintRevision;}return;}
 DWORD now=GetTickCount();top=std::max(32,(snap.height-Height)/2);if(last&&DWORD(now-last)<50)return;last=now;
 external::GameView<decltype(&read)> game(&read);if(pinned&&!external::watch::valid(game,pinned))pinned={};
 tracking=external::watch::choose(read,pinned,tracking,extra.watchDanger);frame=external::watch::sample(read,tracking,extra.renderReveal||extra.mapStrategy==3);
 shown=bool(frame.focus);++paintRevision;if(!shown)return;
 std::array<uint32_t,256> palette{};if(external::terrain::palette(read,0xABBED0,palette)&&palette!=terrainPalette){terrainPalette=palette;tiles.clear();overlays.clear();for(auto it=images.begin();it!=images.end();)if(it->first.second<0x10000)it=images.erase(it);else ++it;}
 auto fetchStatic=[&](auto&cache,bool tmp){
  std::vector<std::pair<uint32_t,uint32_t>> wanted;for(const auto&t:frame.tiles){uint32_t p=tmp?t.image:t.overlay;if(p)wanted.emplace_back(p,uint32_t(tmp?t.sub:t.overlayFrame));}
  for(auto it=cache.begin();it!=cache.end();)if(std::find(wanted.begin(),wanted.end(),it->first)==wanted.end())it=cache.erase(it);else ++it;
  size_t pixels=0;for(const auto&item:cache)pixels+=item.second.image.pixels.size();int work=0;
  for(auto key:wanted){auto&c=cache[key];if(c.ready||work>=4||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
   bool ok=tmp?external::terrain::tile(read,key.first,key.second,terrainPalette,image):external::voxel::sprite(read,key.first,terrainPalette,image,int(key.second));
   if(ok&&pixels+image.pixels.size()<=4*1024*1024){pixels+=image.pixels.size();for(auto&p:image.pixels)if(unsigned alpha=p>>24)p=0xff000000u|(std::min(255u,((p>>16)&255)*255/alpha)<<16)|(std::min(255u,((p>>8)&255)*255/alpha)<<8)|std::min(255u,(p&255)*255/alpha);c.image=std::move(image);c.ready=true;}
  }
 };fetchStatic(tiles,true);fetchStatic(overlays,false);
 std::vector<std::pair<uint32_t,uint32_t>> keys;for(const auto&a:frame.actors)keys.emplace_back(a.type,a.kind==4?uint32_t(a.stage):a.owner);
 auto evict=[&](){auto old=images.end();for(auto it=images.begin();it!=images.end();++it)if(std::find(keys.begin(),keys.end(),it->first)==keys.end()&&(old==images.end()||it->second.used<old->second.used))old=it;if(old==images.end())return false;images.erase(old);return true;};
 int work=0;
 for(const auto&a:frame.actors){auto key=std::make_pair(a.type,a.kind==4?uint32_t(a.stage):a.owner);auto it=images.find(key);if(it==images.end()){if(images.size()>=32&&!evict())continue;it=images.emplace(key,Cached{}).first;}auto&c=it->second;c.used=++serial;
  if(c.ready||work||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
  bool ok=false;if(a.kind==4){uint32_t shp=0;read(a.type+0xA4,&shp,4);ok=external::voxel::sprite(read,shp,terrainPalette,image,a.stage);}else ok=external::voxel::actorImage(read,a.target.pointer,a.type,a.target.id,0,image);if(!ok)continue;
  auto count=[&](){size_t n=image.pixels.size();for(const auto&i:images)n+=i.second.image.pixels.size();return n;};while(count()>4*1024*1024&&evict()){}if(count()>4*1024*1024)continue;
  // Same genuine model as the formation preview, made opaque for observation.
  for(auto&p:image.pixels)if(unsigned alpha=p>>24){p=0xff000000u|(std::min(255u,((p>>16)&255)*255/alpha)<<16)|(std::min(255u,((p>>8)&255)*255/alpha)<<8)|std::min(255u,(p&255)*255/alpha);}
  c.image=std::move(image);c.ready=true;
 }
}
static void draw(Graphics&g){
 if(!shown)return;auto state=g.Save();g.SetClip(RectF(float(left),float(top),float(Width),float(Height)),CombineModeIntersect);g.TranslateTransform(float(left),float(top));
 SolidBrush bg(Color(244,14,22,31));Pen border(Color(255,frame.dangerous?255:105,frame.dangerous?167:195,frame.dangerous?62:225),1.5f);g.FillRectangle(&bg,0.f,0.f,float(Width),float(Height));g.DrawRectangle(&border,1.f,1.f,float(Width-2),float(Height-2));
 drawText(g,pinned?L"固定观察 · 11×11 格":L"危险载员 · 自动跟随 · 11×11 格",10,6,12,Color(255,235,242,249));
 const float ox=Width/2.f,oy=133.f,scale=26.f/60;int centerLevel=frame.tiles[60].level;
 auto image=[&](external::voxel::Image&i,float x,float y){Bitmap bitmap(i.width,i.height,i.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(i.pixels.data()));g.DrawImage(&bitmap,RectF(x+i.x*scale,y+i.y*scale,i.width*scale,i.height*scale));};
 struct Item {float depth;int tile,actor;};std::vector<Item> order;
 for(int n=0;n<121;++n)order.push_back({float((n%11-5)+(n/11-5)),n,-1});
 for(size_t n=0;n<frame.actors.size();++n){const auto&a=frame.actors[n];order.push_back({float(a.x+a.y)/256-frame.cx-frame.cy-1,-1,int(n)});}
 std::stable_sort(order.begin(),order.end(),[](const Item&a,const Item&b){return a.depth<b.depth;});
 std::wstring focusName;int health=0,missing=0,missingTiles=0;
 auto scene=g.Save();g.SetClip(RectF(2,27,Width-4,182),CombineModeIntersect);
 for(const auto&item:order){if(item.tile>=0){int n=item.tile,x=n%11-5,y=n/11-5;const auto&t=frame.tiles[n];float px=ox+(x-y)*13,py=oy+(x+y)*6.5f-(t.level-centerLevel)*15*scale;
   if(!frame.cells[n])continue;auto found=tiles.find({t.image,uint32_t(t.sub)});
   if(found!=tiles.end()&&found->second.ready)image(found->second.image,px,py);
   else{++missingTiles;SolidBrush ground(Color(frame.cells[n]));PointF diamond[]={{px,py-6.5f},{px+13,py},{px,py+6.5f},{px-13,py}};g.FillPolygon(&ground,diamond,4);}
   auto over=overlays.find({t.overlay,uint32_t(t.overlayFrame)});if(over!=overlays.end()&&over->second.ready)image(over->second.image,px,py);
  }else{const auto&a=frame.actors[item.actor];int cx=a.x/256-frame.cx+5,cy=a.y/256-frame.cy+5;int level=frame.tiles[cy*11+cx].level;
   float x=ox+(float(a.x-a.y)/256-frame.cx+frame.cy)*13,y=oy+item.depth*6.5f-(level-centerLevel)*15*scale;
   auto found=images.find({a.type,a.kind==4?uint32_t(a.stage):a.owner});
   if(found!=images.end()&&found->second.ready)image(found->second.image,x,y);else{++missing;SolidBrush marker(Color(a.color));g.FillEllipse(&marker,x-3,y-2,6.f,4.f);}
   if(a.target==frame.focus){Pen ring(Color(255,255,224,111),1.6f);g.DrawEllipse(&ring,x-10,y-5,20.f,10.f);focusName=reinterpret_cast<const wchar_t*>(a.name);health=a.health;}
  }
 }g.Restore(scene);
 if(focusName.empty())focusName=L"目标位置";if(focusName.size()>17)focusName=focusName.substr(0,16)+L"…";
 drawText(g,focusName+L" · HP "+std::to_wstring(health),10,213,12,Color(255,240,244,249));
 std::wstring cargo;for(const auto&p:frame.passengers){if(!cargo.empty())cargo+=L" / ";cargo+=std::wstring(reinterpret_cast<const wchar_t*>(p.name))+L"×"+std::to_wstring(p.count);if(cargo.size()>42){cargo=cargo.substr(0,41)+L"…";break;}}
 drawText(g,cargo.empty()?L"观察键："+keyText(WORD(extra.watchKey))+L"（悬停或选中单位）":cargo,10,232,11,Color(255,255,208,133));
 drawText(g,missingTiles?std::to_wstring(missingTiles)+L" 块地表待载入 · 已载入部分使用真实贴图":L"真实 TMP 地表 / SHP 静物 / VXL 模型",10,251,10,Color(255,149,168,187));
 drawText(g,missing?std::to_wstring(missing)+L" 个模型未载入，使用阵营标记":L"外部观察 · 单位固定朝向 · 不移动主视角",10,266,10,Color(255,149,168,187));g.Restore(state);
}
}
