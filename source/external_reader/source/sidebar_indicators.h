#pragma once
#include "local_ui_renderer.h"
namespace external { namespace sidebarindicators {
// GPL-3.0. Presentation semantics from Phobos f06797e, Hooks.UI.cpp and
// TechnoExt::IsHarvesting. No Phobos extension-memory layout is assumed.
// The provider covers native harvesters/slave miners. Custom Harvester.Counted
// overrides and Phobos-only cash generators require an explicit mod adapter.
struct Sample {int frame=-1,active=0,total=0,output=0,drain=0;bool blackout=false,buildings=false;};
inline uint32_t harvesterColor(int active,int total){double ratio=total?double(active)/total:1.;return ratio>.99?0xFF6CFC54u:ratio>.5?0xFFFFFF00u:0xFFFF3030u;}
inline uint32_t powerColor(const Sample&s){double ratio=s.output?double(s.drain)/s.output:s.drain?2.:.75;return s.blackout?0xFF909090u:ratio<.75?0xFF6CFC54u:ratio<=1.?0xFFFFFF00u:0xFFFF3030u;}
template<class Read>bool moving(GameView<Read>&g,uint32_t object){
 uint32_t loco=g.template get<uint32_t>(object+0x674);if(g.template get<uint32_t>(loco)!=0x7E7EB0||g.template get<uint32_t>(loco+8)!=object)return false;
 std::array<int,6>destination{};std::array<int,3>empty{},position{};if(!g.bytes(loco+0x30,destination.data(),24)||!g.bytes(0x8A0790,empty.data(),12)||!g.bytes(object+0x9C,position.data(),12))return false;
 // Drive4AFB80: next cell, then remaining destination vs current location.
 return !std::equal(empty.begin(),empty.end(),destination.begin())||(!std::equal(empty.begin(),empty.end(),destination.begin()+3)&&(destination[3]!=position[0]||destination[4]!=position[1]));
}
template<class Read>bool capture(Read read,uint32_t house,int frame,Sample&out,bool harvesters=true){
 GameView g(read);Sample next;next.frame=frame;uint32_t player=0;int before=-1;if(!read(0xA83D4C,&player,4)||player!=house||!read(0xA8ED84,&before,4)||before!=frame)return false;
 std::vector<uint32_t>objects,again;if(!g.vecChecked(0xA8EC78,objects))return false;std::set<uint32_t>present;std::vector<std::pair<uint32_t,uint32_t>>miners;
 for(auto p:objects){int kind=g.kind(p);if(kind<0)continue;uint32_t owner=0,type=0;int health=0;uint8_t alive=0,onMap=0,limbo=0;
  if(!read(p+0x21C,&owner,4)||!read(p+0x6C,&health,4)||!read(p+0x90,&alive,1)||!read(p+0x74,&onMap,1)||!read(p+0x81,&limbo,1))return false;
  if(owner!=house||health<=0||!alive)continue;type=g.type(p,kind);if(type<0x10000)return false;
  if(kind==3&&onMap&&!limbo){present.insert(type);next.buildings=true;}
  if(!harvesters)continue;
  uint8_t harvester=0;uint32_t slave=0;if(kind==2&&!read(type+0xE0E,&harvester,1))return false;if(!read(p+0x2D8,&slave,4))return false;
  if(!harvester&&!slave)continue;
  // IsOnMap stays set for the lifetime of placed native objects; passengers
  // can be in Limbo. Exclude factory placeholders before their first placement.
  if(!onMap&&!g.template get<uint32_t>(p+0x11C))continue;
  miners.push_back({p,type});++next.total;
 }
 int blackout[3]{};if(!read(house+0x53A4,&next.output,4)||!read(house+0x53A8,&next.drain,4)||!read(house+0x2A4,blackout,12)||std::abs(int64_t(next.output))>100000000||std::abs(int64_t(next.drain))>100000000)return false;
 next.blackout=blackout[0]!=-1&&int64_t(blackout[2])-frame+blackout[0]>0;
 for(auto[p,type]:miners){if(!g.live(p)||g.template get<uint32_t>(p+0x278)||g.template get<uint8_t>(p+0x270)||g.template get<uint8_t>(p+0x271)||g.template get<uint8_t>(p+0x1C8)||g.template get<int>(p+0x504)>0)continue;
  uint32_t slave=g.template get<uint32_t>(p+0x2D8);if(slave){int state=-1;uint32_t owner=0;if(!read(slave+0x24,&owner,4)||owner!=p||!read(slave+0x5C,&state,4)||state<0||state>6)return false;if(state){++next.active;continue;}}
  if(g.kind(p)==3){if(g.template get<uint8_t>(p+0x660)&&!next.blackout)++next.active;continue;}
  std::vector<uint32_t>docks;if(!g.vecChecked(type+0x3E8,docks,64))return false;bool available=false;for(auto dock:docks)available|=present.count(dock)!=0;if(!available)continue;
  int count=g.template get<int>(p+0xE8,-1);uint32_t links=g.template get<uint32_t>(p+0xE4);if(count<0||count>64||(count&&links<0x10000))return false;bool any=false;uint32_t first=0;
  for(int i=0;i<count;++i){uint32_t link=0;if(!read(links+i*4,&link,4))return false;if(!i)first=link;any|=link!=0;}
  int mission=g.template get<int>(p+0xAC,-1);bool active=false;
  if(mission==10){bool dockLink=g.kind(first)==3&&std::find(docks.begin(),docks.end(),g.type(first,3))!=docks.end();active=(!any||dockLink)&&!g.template get<uint8_t>(p+0x3D0);}
  else if(mission==16)active=true;
  else if(mission==7)active=!any||g.kind(first)==3;
  else if(mission==5&&g.template get<uint32_t>(p+0x218)){std::array<float,4>storage{};if(!read(p+0x33C,storage.data(),16))return false;double amount=0;for(float n:storage){if(!std::isfinite(n)||n<0||n>1000000)return false;amount+=n;}active=amount>0&&moving(g,p);}
  next.active+=active;
 }
 int after=-1;uint32_t afterHouse=0;if(!read(0xA8ED84,&after,4)||after!=frame||!read(0xA83D4C,&afterHouse,4)||afterHouse!=house||!g.vecChecked(0xA8EC78,again)||again!=objects)return false;
 out=next;return true;
}
class Engine {
 localui::BitmapFont font_;std::vector<nativefeedback::Primitive>prior_;std::shared_ptr<const std::vector<uint32_t>>pixels_;nativefeedback::Rect bounds_;
public:
 void reset(){font_.reset();prior_.clear();pixels_.reset();bounds_={};}
 template<class Read>bool render(Read read,const mainview::View&view,const Sample&s,int width,int height,bool harvesters,bool power,nativefeedback::Rect&bounds,std::shared_ptr<const std::vector<uint32_t>>&pixels){
  const auto map=view.physical();int left=map.x+map.w,right=view.screenW;if(map.x>=120){left=0;right=map.x;}if(right-left<120||right-left>512)return false;
  localui::Plan plan;localui::Painter painter(plan,view.screenW,view.screenH,width,height);if(!font_.begin(read))return false;
  auto icon=[&](int x,int y,uint32_t color,bool pickaxe){static constexpr uint8_t pick[9]={0x3C,0x7E,0x43,0x48,0x10,0x20,0x40,0x80,0},bolt[9]={0x0C,0x18,0x30,0x7C,0x18,0x30,0x60,0x40,0};auto rows=pickaxe?pick:bolt;for(int row=0;row<9;++row)for(int col=0;col<8;++col)if(rows[row]&(0x80>>col)){painter.fill({x+col+1,y+row+1,1,1},0xFF000000);painter.fill({x+col,y+row,1,1},color);}};
  const int y=2;if(power&&s.buildings){uint32_t color=powerColor(s);int delta=s.output-s.drain;auto text=s.blackout?std::u16string(u"OFF"):(delta>=0?u"+":u"")+localui::number(delta);icon(left+5,y+2,color,false);if(!painter.text(font_,read,text,left+16,y,color,0,false,0,0,(right-left)/2-18))return false;}
  if(harvesters){uint32_t color=harvesterColor(s.active,s.total);auto text=localui::number(s.active)+u"/"+localui::number(s.total);int measured=0;if(!font_.measure(read,text,measured))return false;int x=std::max(left+(right-left)/2,right-5-measured);icon(x-11,y+2,color,true);if(!painter.text(font_,read,text,x,y,color,0,false,0,0,right-x))return false;}
  if(!painter.good()||!font_.stable(read))return false;auto&commands=plan.layers[0];if(commands.empty()){bounds={};pixels.reset();return true;}
  if(commands!=prior_){nativefeedback::Canvas canvas;nativefeedback::Rect next;if(!nativefeedback::paintInteractions(commands,{},canvas,next))return false;bounds_=next;pixels_=std::make_shared<const std::vector<uint32_t>>(std::move(canvas.pixels));prior_=std::move(commands);}
  bounds=bounds_;pixels=pixels_;return true;
 }
};
}}
