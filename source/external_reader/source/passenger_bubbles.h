#pragma once
#include "game_view.h"
#include "projection.h"
#include <array>
#include <vector>
namespace external { namespace passengers {
struct Group {uint32_t type=0;int count=0;bool dangerous=false;uint16_t name[40]{};};
struct Bubble {uint32_t id=0,color=0;int x=0,y=0;bool dangerous=false;std::vector<Group> groups;};
// TechnoClass::Passengers +114/+118, NextObject +30. All links are bounded
// and verified against Transporter; a torn/cyclic list is never shown.
template<class Read> bool contents(Read read,uint32_t carrier,std::vector<Group>&out,bool&danger){
 GameView<Read> game(read);out.clear();danger=false;int count=game.template get<int>(carrier+0x114,-1);
 uint32_t q=game.template get<uint32_t>(carrier+0x118);if(count<=0||count>256||q<0x10000)return false;
 uint32_t seen[256]{};int n=0;
 while(q&&n<count){if(q<0x10000||game.template get<uint32_t>(q+0x11C)!=carrier||game.template get<int>(q+0x6C)<=0)return false;
  for(int i=0;i<n;++i)if(seen[i]==q)return false;seen[n++]=q;
  int kind=game.kind(q);uint32_t t=game.type(q,kind);if(kind<0||kind>2||!t)return false;
  bool special=kind==1&&(game.template get<uint8_t>(t+0xEC3)||game.template get<uint8_t>(t+0xEBE)||game.template get<uint8_t>(t+0xEC2)||game.template get<uint8_t>(t+0xEAE));
  // Demolition trucks and mod equivalents use WeaponType::Suicide +144.
  // Infantry Ivan/C4/Engineer/Infiltrate are native type capabilities.
  auto actor=game.combat(q,0,true);
  for(const auto&w:actor.weapons){if(w.slot<0||w.slot>=18)continue;uint32_t weapon=game.template get<uint32_t>(t+0x898+w.slot*28);
   if(weapon>=0x10000&&game.template get<uint8_t>(weapon+0x144))special=true;
  }
  danger|=special;auto found=std::find_if(out.begin(),out.end(),[&](const Group&g){return g.type==t;});
  if(found==out.end()){Group group;group.type=t;group.dangerous=special;uint32_t name=game.template get<uint32_t>(t+0x60);
   if(name<0x10000||!read(name,group.name,sizeof group.name)){char id[24]{};read(t+0x24,id,23);for(int i=0;i<23;++i)group.name[i]=uint8_t(id[i]);}group.name[39]=0;
   out.push_back(group);found=out.end()-1;
  }++found->count;q=game.template get<uint32_t>(q+0x30);
 }
 return n==count&&!q;
}
struct Sample {std::vector<Bubble> bubbles;bool complete=true;};
template<class Read> Sample sampleChecked(Read source,uint32_t house,const Projection&proj,bool inspect,bool persistent){
 Sample result;auto&out=result.bubbles;if(!inspect&&!persistent)return result;
 auto read=[&](uint32_t at,void*p,size_t n){bool ok=source(at,p,n);if(!ok)result.complete=false;return ok;};
 GameView<decltype(read)> game(read);
 std::vector<uint32_t> objects;if(!game.vecChecked(0xA8EC78,objects)){result.complete=false;return result;}
 for(uint32_t p:objects){int kind=game.kind(p);if((kind!=0&&kind!=2)||!game.enemy(p,house)||game.template get<uint32_t>(p+0x11C)||game.template get<int>(p+0x220)!=0)continue;
  // Native hover flag, also set without controlling or selecting the enemy.
  bool focus=game.template get<uint8_t>(p+0x431)||game.template get<uint8_t>(p+0x83);
  if(!(inspect&&focus)&&!persistent)continue;Bubble b;
  if(!contents(read,p,b.groups,b.dangerous)){
   // A verified empty carrier is a real disappearance. A positive/bad count
   // with an inconsistent linked list is an incomplete external observation.
   if(game.template get<int>(p+0x114,-1)!=0||game.template get<uint32_t>(p+0x118))result.complete=false;continue;
  }
  if(!(inspect&&focus)&&!(persistent&&b.dangerous))continue;
  if(!project(proj,game.template get<int>(p+0x9C),game.template get<int>(p+0xA0),game.template get<int>(p+0xA4),b.x,b.y)||b.x<0||b.x>=proj.width||b.y<0||b.y>=proj.height)continue;
  uint32_t owner=game.template get<uint32_t>(p+0x21C);uint8_t rgb[3]{255,100,100};read(owner+22265,rgb,3);
  b.id=game.template get<uint32_t>(p+0x10);b.color=0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];
  out.push_back(std::move(b));if(out.size()>=256)break;
 }return result;
}
template<class Read> std::vector<Bubble> sample(Read read,uint32_t house,const Projection&proj,bool inspect,bool persistent){
 return sampleChecked(read,house,proj,inspect,persistent).bubbles;
}
}}
