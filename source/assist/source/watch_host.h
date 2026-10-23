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
static constexpr int Width=320,Height=256;static int left=12,top=48;
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
 tracking=external::watch::choose(read,pinned,tracking,extra.watchDanger);frame=external::watch::sample(read,tracking,extra.renderReveal||extra.mapStrategy==3,20);
 shown=bool(frame.focus);++paintRevision;if(!shown)return;
 gameassets::update();
 std::array<uint32_t,256> palette{};if((external::terrain::palette(read,0xABBED0,palette)||gameassets::palette(true,palette))&&palette!=terrainPalette){terrainPalette=palette;tiles.clear();overlays.clear();for(auto it=images.begin();it!=images.end();)if(it->first.second<0x10000)it=images.erase(it);else ++it;}
 auto fetchStatic=[&](auto&cache,bool tmp){
  std::vector<std::pair<uint32_t,uint32_t>> wanted;for(const auto&t:frame.tiles){uint32_t p=tmp?t.type:t.overlayType;if(p)wanted.emplace_back(p,uint32_t(tmp?t.sub:t.overlayFrame));}
  for(auto it=cache.begin();it!=cache.end();)if(std::find(wanted.begin(),wanted.end(),it->first)==wanted.end())it=cache.erase(it);else ++it;
  size_t pixels=0;for(const auto&item:cache)pixels+=item.second.image.pixels.size();int work=0;
  for(auto key:wanted){auto&c=cache[key];if(c.ready||work>=4||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
   uint32_t pointer=0;read(key.first+0xA4,&pointer,4);
   bool ok=tmp?gameassets::tile(key.first,key.second,terrainPalette,image):gameassets::sprite(key.first,pointer,terrainPalette,int(key.second),image);
   if(!ok)ok=tmp?external::terrain::tile(read,pointer,key.second,terrainPalette,image):external::voxel::sprite(read,pointer,terrainPalette,image,int(key.second));
   if(ok&&pixels+image.pixels.size()<=4*1024*1024){pixels+=image.pixels.size();for(auto&p:image.pixels)if(unsigned alpha=p>>24)p=0xff000000u|(std::min(255u,((p>>16)&255)*255/alpha)<<16)|(std::min(255u,((p>>8)&255)*255/alpha)<<8)|std::min(255u,(p&255)*255/alpha);c.image=std::move(image);c.ready=true;}
  }
 };fetchStatic(tiles,true);fetchStatic(overlays,false);
 std::vector<std::pair<uint32_t,uint32_t>> keys;for(const auto&a:frame.actors)keys.emplace_back(a.type,a.kind==4?uint32_t(a.stage):a.owner);
 auto evict=[&](){auto old=images.end();for(auto it=images.begin();it!=images.end();++it)if(std::find(keys.begin(),keys.end(),it->first)==keys.end()&&(old==images.end()||it->second.used<old->second.used))old=it;if(old==images.end())return false;images.erase(old);return true;};
 int work=0;
 for(const auto&a:frame.actors){auto key=std::make_pair(a.type,a.kind==4?uint32_t(a.stage):a.owner);auto it=images.find(key);if(it==images.end()){if(images.size()>=32&&!evict())continue;it=images.emplace(key,Cached{}).first;}auto&c=it->second;c.used=++serial;
  if(c.ready||work||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
  bool ok=false;if(a.kind==4){uint32_t shp=0;read(a.type+0xA4,&shp,4);ok=gameassets::sprite(a.type,shp,terrainPalette,a.stage,image)||external::voxel::sprite(read,shp,terrainPalette,image,a.stage);}else ok=gameassets::actor(a.target.pointer,a.type,a.target.id,0,image)||external::voxel::actorImage(read,a.target.pointer,a.type,a.target.id,0,image);if(!ok)continue;
  auto count=[&](){size_t n=image.pixels.size();for(const auto&i:images)n+=i.second.image.pixels.size();return n;};while(count()>4*1024*1024&&evict()){}if(count()>4*1024*1024)continue;
  // Same genuine model as the formation preview, made opaque for observation.
  for(auto&p:image.pixels)if(unsigned alpha=p>>24){p=0xff000000u|(std::min(255u,((p>>16)&255)*255/alpha)<<16)|(std::min(255u,((p>>8)&255)*255/alpha)<<8)|std::min(255u,(p&255)*255/alpha);}
  c.image=std::move(image);c.ready=true;
 }
}
static void draw(Graphics&g){
 if(!shown)return;auto state=g.Save();g.SetClip(RectF(float(left),float(top),float(Width),float(Height)),CombineModeIntersect);g.TranslateTransform(float(left),float(top));
 SolidBrush bg(Color(255,8,12,8));g.FillRectangle(&bg,0.f,0.f,float(Width),float(Height));
 // Overscan the diamond map in world space and crop only at the rectangular
 // viewport. There is no separate title, status/footer or fixed 11x11 diamond.
 const float ox=Width/2.f,oy=Height/2.f,scale=.5f;int side=frame.side,radius=frame.radius,centerLevel=frame.tiles[radius*side+radius].level;
 auto image=[&](external::voxel::Image&i,float x,float y){Bitmap bitmap(i.width,i.height,i.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(i.pixels.data()));g.DrawImage(&bitmap,RectF(x+i.x*scale,y+i.y*scale,i.width*scale,i.height*scale));};
 struct Item {float depth;int tile,actor;};std::vector<Item> order;
 for(int n=0;n<side*side;++n)order.push_back({float(n%side+n/side-2*radius),n,-1});
 for(size_t n=0;n<frame.actors.size();++n){const auto&a=frame.actors[n];order.push_back({float(a.x+a.y)/256-frame.cx-frame.cy-1,-1,int(n)});}
 std::stable_sort(order.begin(),order.end(),[](const Item&a,const Item&b){return a.depth<b.depth;});
 for(const auto&item:order){if(item.tile>=0){int n=item.tile,x=n%side-radius,y=n/side-radius;const auto&t=frame.tiles[n];float px=ox+(x-y)*30*scale,py=oy+(x+y)*15*scale-(t.level-centerLevel)*15*scale;
   if(!frame.cells[n]||px<-128||px>Width+128||py<-128||py>Height+128)continue;auto found=tiles.find({t.type,uint32_t(t.sub)});
   if(found!=tiles.end()&&found->second.ready)image(found->second.image,px,py);
   else{SolidBrush ground(Color(frame.cells[n]));PointF diamond[]={{px,py-15*scale},{px+30*scale,py},{px,py+15*scale},{px-30*scale,py}};g.FillPolygon(&ground,diamond,4);}
   auto over=overlays.find({t.overlayType,uint32_t(t.overlayFrame)});if(over!=overlays.end()&&over->second.ready)image(over->second.image,px,py);
  }else{const auto&a=frame.actors[item.actor];int cx=a.x/256-frame.cx+radius,cy=a.y/256-frame.cy+radius;if(cx<0||cy<0||cx>=side||cy>=side)continue;
   float x=ox+(float(a.x-a.y)/256-frame.cx+frame.cy)*30*scale,y=oy+item.depth*15*scale-(float(a.z)*60/256-centerLevel*15)*scale;
   auto found=images.find({a.type,a.kind==4?uint32_t(a.stage):a.owner});
   if(found!=images.end()&&found->second.ready)image(found->second.image,x,y);else{SolidBrush marker(Color(a.color));g.FillEllipse(&marker,x-3,y-2,6.f,4.f);}
   if(a.target==frame.focus){Pen ring(Color(255,255,224,111),1.3f);g.DrawEllipse(&ring,x-10,y-5,20.f,10.f);}
  }
 }
 Pen border(Color(255,frame.dangerous?255:105,frame.dangerous?167:195,frame.dangerous?62:225),1.f);g.DrawRectangle(&border,.5f,.5f,float(Width-1),float(Height-1));g.Restore(state);
}
}
