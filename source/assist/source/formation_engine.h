#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/scene_lighting.h"
#include "../../external_reader/source/camera_raster.h"
#include "../../external_reader/source/viewport_canvas.h"
#include <chrono>
#include <map>
#include <tuple>
#include <functional>
#include <memory>
namespace formationengine {
template<class Reader,class Assets>class Renderer {
public:
 Reader&reader;Assets&assets;std::function<bool(uint32_t,void*,size_t)> read;
 Renderer(Reader&r,Assets&a):reader(r),assets(a),read([&r](uint32_t p,void*out,size_t n){return r.readFresh(p,out,n);}){}
 uint32_t paintRevision=0,composedRevision=0;bool compositePrecision=false;unsigned compositeScale=0;std::shared_ptr<const external::voxel::Image>composed;
 size_t decodeCursor=0;double decodeBudgetMs=4.;external::camera::NativeCanvas blank;external::camera::ViewportCanvas compositor;std::vector<uint32_t>transparent;int compositeX=0,compositeY=0;
 template<class A>static auto shared(A&a,const typename A::ActorArt&art,const external::visual::Pose&pose,std::shared_ptr<const external::voxel::Image>&out,int)->decltype(a.renderSharedActor(art,pose,out),bool()){return a.renderSharedActor(art,pose,out);}
 template<class A>static bool shared(A&a,const typename A::ActorArt&art,const external::visual::Pose&pose,std::shared_ptr<const external::voxel::Image>&out,long){external::voxel::Image image;if(!a.renderActor(art,pose,image))return false;out=std::make_shared<const external::voxel::Image>(std::move(image));return true;}
 template<class A>static auto refresh(A&a,typename A::ActorArt&art,bool&changed,int)->decltype(a.refreshActorPalette(art,changed),bool()){return a.refreshActorPalette(art,changed);}
 template<class A>static bool refresh(A&,typename A::ActorArt&,bool&changed,long){changed=false;return true;}


struct Cached {typename Assets::ActorArt art;bool attempted=false,ready=false;uint32_t retry=0;uint64_t used=0,viewUsed[32]{};std::map<int,std::shared_ptr<const external::voxel::Image>> views;};
struct Visual {int x,y;std::shared_ptr<const external::voxel::Image> image;external::lighting::State light;};
using ModelKey=std::tuple<uint32_t,uint32_t,int>;
std::map<ModelKey,Cached> models;
formation::Preview preview;std::vector<Visual> visuals;
std::vector<ModelKey> wanted;uint64_t useSerial=0;int wantedFacing=0;
uint32_t epoch=0,process=0,revision=0,sampledAt=0;uint64_t visualSignature=0;int missing=0;
external::lighting::State sceneLight;
uint64_t assetRevision=0;
static constexpr unsigned PreviewOpacity=124;
uint32_t word(uint32_t p){uint32_t v=0;read(p,&v,4);return v;}
void reset(){models.clear();visuals.clear();preview={};missing=0;decodeCursor=0;blank={};compositor.reset();transparent.clear();composed.reset();++revision;}
bool needed(const ModelKey&key){return std::find(wanted.begin(),wanted.end(),key)!=wanted.end();}
bool evictModel(){auto oldest=models.end();for(auto it=models.begin();it!=models.end();++it)if(!needed(it->first)&&(oldest==models.end()||it->second.used<oldest->second.used))oldest=it;if(oldest==models.end())return false;models.erase(oldest);++revision;return true;}
size_t pixelCount(){size_t n=0;for(const auto&e:models){for(const auto&v:e.second.views)n+=external::camera::pixels(*v.second);}return n;}
bool roomForImage(size_t pixels){while(pixelCount()+pixels>4*1024*1024){Cached*owner=nullptr;int angle=-1;uint64_t oldest=UINT64_MAX;for(auto&e:models)for(const auto&v:e.second.views)if(!(needed(e.first)&&v.first==wantedFacing)&&e.second.viewUsed[v.first]<oldest){owner=&e.second;angle=v.first;oldest=owner->viewUsed[angle];}if(owner){owner->views.erase(angle);++revision;}else if(!evictModel())return false;}return true;}
ModelKey modelKey(const formation::Unit&u){external::GameView game(read);int variant=-1;
 if(int(word(u.type+0x808))>0&&!uint8_t(word(u.type+0xCD5))){int n=int(word(u.pointer+0x124));if(n>=0&&n<18)variant=n;}
 return {u.type,word(u.pointer+0x21C),variant};
}
bool capture(const formation::Unit&u,Cached&cache){
 cache={};cache.used=++useSerial;cache.attempted=true;cache.retry=GetTickCount()+2000;assets.identity(process,epoch);
 if(!assets.captureActor(u.pointer,u.type,u.id,cache.art))return false;
 for(;;){size_t faces=0;for(const auto&e:models)faces+=e.second.art.model.faces.size();if(faces<=400000)break;if(!evictModel()){cache.art={};return false;}}
 cache.ready=true;return true;
}
void update(const formation::Preview&next,const external::Projection&projection){
 assets.identity(process,epoch);if(assetRevision!=assets.revision()){assetRevision=assets.revision();reset();}
 auto light=external::lighting::sample(read);if(light!=sceneLight){sceneLight=light;++revision;}
 // Input may stand still indefinitely; validity is anchored by the live shared
 // context, not a timer that would make stationary previews disappear.
 visuals.clear();missing=0;wanted.clear();preview=next;
 int facing=int(std::lround(next.facing*32/(2*formation::Pi)))&31;wantedFacing=facing;int work=0;auto started=std::chrono::steady_clock::now();auto room=[&]{return work==0||(work<512&&std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<decodeBudgetMs);};for(auto&entry:models){bool changed=false;if(!refresh(assets,entry.second.art,changed,0)){entry.second.ready=false;entry.second.attempted=false;entry.second.views.clear();}else if(changed){entry.second.views.clear();++revision;}}
 for(int n=0;n<next.count;++n)if(next.slots[n].valid){int x,y;if(external::project(projection,next.slots[n].x,next.slots[n].y,next.slots[n].z,x,y))wanted.push_back(modelKey(next.units[n]));}
 const size_t start=next.count?decodeCursor%next.count:0;
 for(int scanned=0;scanned<next.count;++scanned){int n=int((start+scanned)%next.count);const auto&u=next.units[n];const auto&s=next.slots[n];if(!s.valid)continue;int x,y;if(!external::project(projection,s.x,s.y,s.z,x,y))continue;
  if(word(u.pointer+0x10)!=u.id)continue;auto key=modelKey(u);
  // Ownership can change between the wanted scan and this fresh read.
  // Protect this cache entry too before any eviction can invalidate c.
  if(!needed(key))wanted.push_back(key);
  auto found=models.find(key);if(found==models.end()){if(models.size()>=formation::Maximum&&!evictModel()){++missing;continue;}found=models.emplace(key,Cached{}).first;}auto&c=found->second;c.used=++useSerial;
  if(!c.ready&&room()&&(!c.attempted||int32_t(GetTickCount()-c.retry)>=0)){++work;decodeCursor=size_t(n)+1;if(!capture(u,c))c.art={};++revision;}
  if(!c.ready){++missing;continue;}
  auto it=c.views.find(facing);if(it==c.views.end()){if(!room()){++missing;continue;}++work;decodeCursor=size_t(n)+1;std::shared_ptr<const external::voxel::Image>rendered;external::GameView game(read);
   external::visual::Pose pose;pose.body=pose.turret=uint8_t(external::visual::previewBody(facing));pose.frame=external::visual::previewFrame(game,u.type,u.kind,facing);
   if(!shared(assets,c.art,pose,rendered,0)){++missing;continue;}
   // Both SHP and VXL keep rendered headings; changing the drag direction
   // never reopens archives or redecodes the source model.
   if(c.views.size()>=8){auto old=c.views.begin();for(auto v=c.views.begin();v!=c.views.end();++v)if(c.viewUsed[v->first]<c.viewUsed[old->first])old=v;c.views.erase(old);++revision;}
   if(!roomForImage(external::camera::pixels(*rendered))){++missing;continue;}it=c.views.emplace(facing,std::move(rendered)).first;++revision;
  }
  auto image=it->second;c.viewUsed[facing]=++useSerial;
  visuals.push_back({x,y,image,external::lighting::object(read,u.type,std::get<1>(key),u.kind,s.x,s.y,s.z,sceneLight)});
 }
 std::sort(visuals.begin(),visuals.end(),[](const Visual&a,const Visual&b){if(a.y!=b.y)return a.y<b.y;if(a.x!=b.x)return a.x<b.x;return std::less<const external::voxel::Image*>{}(a.image.get(),b.image.get());});
 uint64_t hash=(uint64_t(revision)<<32)^uint64_t(facing+1)*1099511628211ull^uint64_t(next.shape+1)*16777619ull^missing;for(const auto&v:visuals){hash=(hash^uint32_t(v.x))*1099511628211ull;hash=(hash^uint32_t(v.y))*1099511628211ull;hash=(hash^uintptr_t(v.image.get()))*1099511628211ull;hash=(hash^uint32_t(v.light.intensity))*1099511628211ull;for(int c:v.light.tint)hash=(hash^uint32_t(c))*1099511628211ull;}if(hash!=visualSignature){visualSignature=hash;++paintRevision;}
}
std::shared_ptr<const external::voxel::Image> compose(bool precision,unsigned scale){
 if(visuals.empty()){composed.reset();return {};}if(composed&&composedRevision==paintRevision&&compositePrecision==precision&&compositeScale==scale)return composed;int left=INT32_MAX,top=INT32_MAX,right=INT32_MIN,bottom=INT32_MIN;for(const auto&v:visuals){int sample=std::max(1,v.image->rasterScale);left=std::min(left,v.x+v.image->x/sample-8);top=std::min(top,v.y+v.image->y/sample-8);right=std::max(right,v.x+(v.image->x+v.image->width)/sample+8);bottom=std::max(bottom,v.y+(v.image->y+v.image->height)/sample+8);}int w=right-left,h=bottom-top;if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024)return {};
 if(blank.width()!=w||blank.height()!=h){if(!blank.reset(w,h))return {};transparent.assign(size_t(w)*h,0);compositor.reset();}auto mode=precision?external::colorprecision::Mode::SmoothRGB:external::colorprecision::Mode::NativeSteps;if(!compositor.prepare(blank,transparent,w,h,0,0,w,h,1,mode,scale))return {};
 for(const auto&v:visuals)compositor.blit(*v.image,float(v.x-left),float(v.y-top),1,1,float(v.y),v.light,1,0,v.image);auto image=std::make_shared<external::voxel::Image>();if(!compositor.finish(image->pixels,true))return {};image->width=w;image->height=h;image->x=left;image->y=top;image->intrinsicAlpha=true;for(auto&pixel:image->pixels){uint32_t next=0;for(int shift:{0,8,16,24})next|=((((pixel>>shift)&255)*PreviewOpacity+127)/255)<<shift;pixel=next;}composed=image;composedRevision=paintRevision;compositePrecision=precision;compositeScale=scale;return image;
}

};
}
