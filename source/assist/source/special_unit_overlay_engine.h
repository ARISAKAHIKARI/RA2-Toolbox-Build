#pragma once
#include "formation_engine.h"
#include "../../external_reader/source/enemy_markers.h"
#include <unordered_map>
namespace specialoverlay {
inline bool enabled(bool markers,bool only,int mode){return markers&&only&&mode==1;}
inline uint64_t identity(const external::enemies::Marker&m){return (uint64_t(m.object)<<32)|m.id;}
struct Identity {uint32_t object=0,id=0,type=0,owner=0;};
inline bool stillPresent(const std::vector<Identity>&drawn,const external::enemies::Frame&current){
 std::unordered_map<uint64_t,const external::enemies::Marker*> present;
 for(int i=0;i<current.count&&i<external::enemies::Maximum;++i)present.emplace(identity(current.items[i]),&current.items[i]);
 for(const auto&v:drawn){auto i=present.find((uint64_t(v.object)<<32)|v.id);if(i==present.end()||i->second->type!=v.type||i->second->owner!=v.owner)return false;}
 return true;
}
template<class Reader,class Assets>class Renderer {
 using Preview=formationengine::Renderer<Reader,Assets>;
 Reader&reader_;Assets&assets_;
 struct Retained {uint32_t type=0,owner=0,tick=0;std::shared_ptr<const external::voxel::Image> image;};
 std::unordered_map<uint64_t,Retained> retained_;
public:
 Preview paint;std::vector<Identity> identities;uint64_t signature=0;
 Renderer(Reader&r,Assets&a):reader_(r),assets_(a),paint(r,a){}
 void reset(){paint.reset();identities.clear();retained_.clear();signature=0;}
 void update(const std::vector<external::enemies::Marker>&targets,const external::Projection&projection,int logicFrame){
  assets_.identity(paint.process,paint.epoch);
  if(paint.assetRevision!=assets_.revision()){paint.assetRevision=assets_.revision();reset();}
  paint.visuals.clear();paint.wanted.clear();identities.clear();paint.missing=0;
  external::GameView game(paint.read);auto scene=external::lighting::sample(paint.read);
  external::lighting::ObjectLightContext lights;
  struct Candidate {formation::Unit unit;int sx,sy;typename Preview::ModelKey key;};std::vector<Candidate> candidates;
  candidates.reserve(targets.size());
  for(const auto&m:targets){
   const auto p=m.object;int kind=game.kind(p);
   if(kind!=m.kind||kind<0||kind>2||game.template get<uint32_t>(p+0x10)!=m.id||game.type(p,kind)!=m.type||game.template get<uint32_t>(p+0x21C)!=m.owner||
      game.template get<int>(p+0x6C)<=0||!game.template get<uint8_t>(p+0x74)||game.template get<uint8_t>(p+0x81)||!game.template get<uint8_t>(p+0x90)||
      game.template get<uint32_t>(p+0x11C)||!external::enemies::special(paint.read,p,m.type,kind))continue;
   const int x=game.template get<int>(p+0x9C,INT32_MIN),y=game.template get<int>(p+0xA0,INT32_MIN),z=game.template get<int>(p+0xA4,INT32_MIN);int sx,sy;
   if(x<0||y<0||x>=512*256||y>=512*256||!external::project(projection,x,y,z,sx,sy)||sx < -512||sy < -512||sx>projection.width+512||sy>projection.height+512)continue;
   formation::Unit u{m.id,p,m.type,0,m.color,x,y,z,kind};auto key=paint.modelKey(u);paint.wanted.push_back(key);candidates.push_back({u,sx,sy,key});
  }
  for(auto&entry:paint.models){if(!entry.second.ready)continue;bool changed=false;if(!Preview::refresh(assets_,entry.second.art,changed,0)){entry.second.ready=false;entry.second.attempted=false;}else if(changed)entry.second.views.clear();}
  std::unordered_map<uint64_t,Retained> retained;const auto now=GetTickCount();
  unsigned work=0;auto start=std::chrono::steady_clock::now();auto room=[&]{return !work||(work<512&&std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<paint.decodeBudgetMs);};
  const size_t first=candidates.empty()?0:paint.decodeCursor%candidates.size();
  for(size_t scanned=0;scanned<candidates.size();++scanned){const auto index=(first+scanned)%candidates.size();const auto&v=candidates[index];const auto&u=v.unit;
   auto found=paint.models.find(v.key);if(found==paint.models.end()){if(paint.models.size()>=formation::Maximum&&!paint.evictModel()){++paint.missing;continue;}found=paint.models.emplace(v.key,typename Preview::Cached{}).first;}auto&cache=found->second;cache.used=++paint.useSerial;
   if(!cache.ready&&room()&&(!cache.attempted||int32_t(GetTickCount()-cache.retry)>=0)){++work;paint.decodeCursor=index+1;
    if(!paint.capture(u,cache)){cache.art={};if(!reader_.worldFrameComplete())cache.attempted=false;}}
   // Actual headings and animation, with the real Type/House. Disguise and
   // cloak change admission only; never draw or erase their native shape.
   const auto owner=std::get<1>(v.key);const uint64_t id=(uint64_t(u.pointer)<<32)|u.id;uint32_t renderedAt=now;
   std::shared_ptr<const external::voxel::Image> image;
   if(cache.ready&&room()){++work;paint.decodeCursor=index+1;
    auto pose=external::visual::pose(game,u.pointer,u.type,u.kind,logicFrame);
    if(!Preview::shared(assets_,cache.art,pose,image,0)||!image||image->incomplete)image.reset();
   }
   if(!image){++paint.missing;auto old=retained_.find(id);
    // Budget pressure must not alternate whole subsets on/off. Reuse only
    // the same live identity's recent pose, at its freshly sampled position.
    // A reused pose never renews this deadline, and dead/removed units never
    // enter candidates, so they cannot inherit the hold.
    if(old==retained_.end()||old->second.type!=u.type||old->second.owner!=owner||uint32_t(now-old->second.tick)>=150)continue;
    image=old->second.image;renderedAt=old->second.tick;
   }
   retained.emplace(id,Retained{u.type,owner,renderedAt,image});
   paint.visuals.push_back({v.sx,v.sy,image,external::lighting::object(paint.read,u.type,std::get<1>(v.key),u.kind,u.x,u.y,u.z,scene,&lights)});
   identities.push_back({u.pointer,u.id,u.type,owner});
  }
  retained_.swap(retained);
  std::sort(paint.visuals.begin(),paint.visuals.end(),[](const auto&a,const auto&b){if(a.y!=b.y)return a.y<b.y;if(a.x!=b.x)return a.x<b.x;return std::less<const external::voxel::Image*>{}(a.image.get(),b.image.get());});
  uint64_t hash=1469598103934665603ull;auto mix=[&](uint64_t v){hash=(hash^v)*1099511628211ull;};
  for(const auto&v:paint.visuals){mix(uint32_t(v.x));mix(uint32_t(v.y));mix(uintptr_t(v.image.get()));mix(v.light.intensity);for(auto c:v.light.tint)mix(c);}
  if(hash!=signature){signature=hash;++paint.paintRevision;}
 }
 std::shared_ptr<const external::voxel::Image> compose(bool precision,unsigned scale){return paint.compose(precision,scale,255);}
};
}
