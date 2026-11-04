#pragma once
#include "game_view.h"
#include "scene_order.h"
#include <map>
#include <set>
namespace external { namespace tetherdraw {
// GPL-3.0, Vera20K 3d42426 instances/tether_redraw.rs; YRpp d979554.
// Building43DA80 redraws radio contacts before its body during Unload.
// Unit73B0B0 suppresses only its ordinary body slot; status UI keeps its owner.
using Key=std::pair<uint32_t,uint32_t>;
template<class Read,class Markers>void capture(Read read,Markers&markers){
 GameView game(read);std::map<uint32_t,typename Markers::value_type*>visible;
 for(auto&m:markers){m.tetherHidden=false;m.tetherRedraw.clear();if(m.kind>=0&&m.kind<=3&&!m.disguised)visible[m.object]=&m;}
 for(auto&m:markers){if(m.disguised)continue;
  if(m.kind==2&&game.template get<uint8_t>(m.object+0x418)){
   uint32_t links=game.template get<uint32_t>(m.object+0xE4);int count=game.template get<int>(m.object+0xE8,-1);
   uint32_t first=count>0&&count<=64?game.template get<uint32_t>(links):0;
   if(game.kind(first)==3&&(game.template get<int>(first+0xAC,-1)==16||game.template get<int>(first+0xB4,-1)==16))m.tetherHidden=true;
  }
  if(m.kind!=3||!game.template get<uint8_t>(m.object+0x418)||game.template get<int>(m.object+0xAC,-1)!=16)continue;
  uint32_t links=game.template get<uint32_t>(m.object+0xE4);int count=game.template get<int>(m.object+0xE8,-1);if(count<0||count>64||(count&&links<0x10000))continue;
  std::vector<uint32_t>before(size_t(count),0),after(size_t(count),0);if(count&&!read(links,before.data(),before.size()*4))continue;
  std::set<uint32_t>seen;for(auto contact:before){auto found=visible.find(contact);if(found==visible.end()||found->second->kind==3||!seen.insert(contact).second||game.template get<uint8_t>(contact+0x81))continue;m.tetherRedraw.push_back({contact,found->second->id});}
  if((count&&(!read(links,after.data(),after.size()*4)||after!=before))||game.template get<uint32_t>(m.object+0xE4)!=links||game.template get<int>(m.object+0xE8,-1)!=count||game.template get<int>(m.object+0xAC,-1)!=16)m.tetherRedraw.clear();
 }
}
template<class Marker>auto displayIndex(const Marker&m,int)->decltype(m.displayOrder,int()){return m.displayOrder;}
template<class Marker>int displayIndex(const Marker&,long){return -1;}
template<class Markers>auto order(const Markers&markers,bool replay=true){
 using Marker=typename Markers::value_type;std::vector<const Marker*>sorted,result;std::map<Key,const Marker*>lookup;
 for(const auto&m:markers){sorted.push_back(&m);lookup[{m.object,m.id}]=&m;}
 std::stable_sort(sorted.begin(),sorted.end(),[](auto a,auto b){return camera::orderKey(a->layer,a->kind,a->x,a->y,a->sort)<camera::orderKey(b->layer,b->kind,b->x,b->y,b->sort);});
 // Keep directly drawn families (particles/attached anims) near their existing
 // Y-sort anchors, while native Display members keep their exact list order.
 std::vector<const Marker*>nativeSorted;nativeSorted.reserve(sorted.size());
 for(size_t at=0;at<sorted.size();){size_t end=at+1;while(end<sorted.size()&&sorted[end]->layer==sorted[at]->layer)++end;
  std::vector<const Marker*>known,extra;for(size_t n=at;n<end;++n)(displayIndex(*sorted[n],0)>=0?known:extra).push_back(sorted[n]);
  std::stable_sort(known.begin(),known.end(),[](auto a,auto b){return displayIndex(*a,0)<displayIndex(*b,0);});size_t next=0;
  for(auto m:known){while(next<extra.size()&&camera::orderKey(extra[next]->layer,extra[next]->kind,extra[next]->x,extra[next]->y,extra[next]->sort)<camera::orderKey(m->layer,m->kind,m->x,m->y,m->sort))nativeSorted.push_back(extra[next++]);nativeSorted.push_back(m);}while(next<extra.size())nativeSorted.push_back(extra[next++]);at=end;
 }sorted=std::move(nativeSorted);
 for(auto m:sorted){if(replay)for(auto key:m->tetherRedraw){auto found=lookup.find(key);if(found!=lookup.end())result.push_back(found->second);}if(!replay||!m->tetherHidden)result.push_back(m);}
 return result;
}
template<class Draws,class Markers>void replay(Draws&draws,const Markers&markers){
 // Ground/frozen draws precede the actor run; trails follow it. Reorder whole
 // actor groups, so a harvester's shadow, body and gather piece stay together.
 size_t first=0;while(first<draws.size()&&!draws[first].object)++first;size_t last=first;while(last<draws.size()&&draws[last].object)++last;if(first==last)return;
 std::map<Key,std::vector<typename Draws::value_type>>groups;for(size_t i=first;i<last;++i)groups[{draws[i].object,draws[i].id}].push_back(draws[i]);
 Draws result;result.reserve(draws.size()+64);result.insert(result.end(),draws.begin(),draws.begin()+first);
 for(auto m:order(markers))if(auto found=groups.find({m->object,m->id});found!=groups.end())result.insert(result.end(),found->second.begin(),found->second.end());
 result.insert(result.end(),draws.begin()+last,draws.end());draws=std::move(result);
}
}}
