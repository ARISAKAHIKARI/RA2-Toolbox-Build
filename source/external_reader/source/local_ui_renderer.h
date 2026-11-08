#pragma once
#include "native_ui_font.h"
#include "main_viewport.h"
#include "feedback_selection.h"
#include "building_animations.h"
#include "map_bounds.h"
#include "ground_geometry.h"
#include "repair_visuals.h"
#include "live_shape_cache.h"
#include "native_unit_ui_stencil.h"
#include "native_planning.h"
#include "native_radial_indicators.h"
#include <set>
namespace external {namespace localui {
// Main-view UI rendered from values and real font coverage. No framebuffer,
// passthrough rectangle or cutout is accepted or produced by this module.
constexpr size_t LayerCount=5;
struct Plan {std::array<std::vector<nativefeedback::Primitive>,LayerCount>layers;unsigned messages=0,timers=0,primary=0,rallies=0;uint64_t glyphDecodes=0;bool complete=true;};
inline int remaining(int start,int duration,int frame){return int(std::clamp<int64_t>(start==-1?duration:int64_t(duration)-frame+start,0,INT32_MAX));}
inline std::u16string number(int value,int digits=0){auto n=std::to_string(value);if(int(n.size())<digits)n=std::string(digits-n.size(),'0')+n;return {n.begin(),n.end()};}
inline std::u16string clockText(int frames){int s=frames/15,h=s/3600,m=s/60%60;return h?number(h)+u":"+number(m,2)+u":"+number(s%60,2):number(m,2)+u":"+number(s%60,2);}
template<class Read>bool schemeColor(Read read,int index,uint32_t&out){uint32_t items=0,scheme=0;int count=0;uint8_t rgb[3]{};
 if(!read(0xB054D4,&items,4)||!read(0xB054E0,&count,4)||count<=0||count>512||!nativefeedback::range(items,size_t(count)*4))return false;if(index<=0||index>=count)index=0;
 if(!read(items+index*4,&scheme,4)||!nativefeedback::range(scheme,0x30C)||!read(scheme+0x308,rgb,3))return false;out=rgb565(uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2]);return true;
}
template<class Read>bool houseColor(Read read,uint32_t house,uint32_t&out){uint8_t rgb[3]{};if(!nativefeedback::range(house,0x56FC)||!read(house+0x56F9,rgb,3))return false;out=rgb565(uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2]);return true;}
// Native pixel walk/clip/pattern reference: VERA20K 0b6a073, surface_line.rs
// and presentation/target_lines.rs (GPL-3.0). Pixels are ours; no native RNG.
inline bool clipLine(std::array<int,2>&a,std::array<int,2>&b,int width,int height){
 std::array<double,2>p{double(a[0]),double(a[1])},q{double(b[0]),double(b[1])};double xy=(q[0]-p[0])/(q[1]-p[1]),yx=(q[1]-p[1])/(q[0]-p[0]);auto code=[&](auto v){return (v[0]<0?1:v[0]>=width?2:0)|(v[1]<0?8:v[1]>=height?4:0);};
 for(int i=0;i<16;++i){int pc=code(p),qc=code(q);if(!(pc|qc)){a={int(p[0]),int(p[1])};b={int(q[0]),int(q[1])};return true;}if(pc&qc)return false;int c=pc?pc:qc;std::array<double,2>v;
  if(c&8)v={-p[1]*xy+p[0],0};else if(c&4)v={(height-1-p[1])*xy+p[0],double(height-1)};else if(c&2)v={double(width-1),(width-1-p[0])*yx+p[1]};else v={0,-p[0]*yx+p[1]};
  if(!std::isfinite(v[0])||!std::isfinite(v[1]))return false;if(c==pc)p=v;else q=v;
 }return false;
}
template<class Emit>void patternLine(std::array<int,2>a,std::array<int,2>b,int phase,Emit emit,int run=5){
 int direction=1;if(a[0]>b[0]){phase=(phase+std::max(a[0]-b[0],std::abs(a[1]-b[1])+1))%16;direction=-1;std::swap(a,b);}int dx=b[0]-a[0],dy=std::abs(b[1]-a[1]),ys=a[1]>b[1]?-1:1,count=std::max(dx,dy)+int(dx==0||dy==0),error=2*std::min(dx,dy)-std::max(dx,dy);
 for(int i=0;i<count;++i){int bit=(phase%16+16)%16;if(bit%8<run)emit(a[0],a[1]);phase+=direction;if(dx>dy){if(error>0){a[1]+=ys;error-=2*dx;}++a[0];error+=2*dy;}else{if(error>0){++a[0];error-=2*dy;}a[1]+=ys;error+=2*dx;}}
}
class Painter {
 Plan&plan_;int nativeW_,nativeH_,width_,height_;double sx_,sy_;bool overflow_=false;size_t lane_=0,total_=0;
public:
 Painter(Plan&p,int nw,int nh,int w,int h):plan_(p),nativeW_(nw),nativeH_(nh),width_(w),height_(h),sx_(double(w)/nw),sy_(double(h)/nh){}
 bool good()const{return !overflow_;}
 void lane(size_t value){lane_=std::min<size_t>(value,LayerCount-1);}
 void fill(nativefeedback::Rect r,uint32_t color){r=nativefeedback::intersect(r,{0,0,nativeW_,nativeH_});if(!r.valid()||overflow_)return;auto edge=[](int v,double s){return int(std::ceil(v*s-.5));};int x=edge(r.x,sx_),y=edge(r.y,sy_),right=edge(r.x+r.w,sx_),bottom=edge(r.y+r.h,sy_);auto result=nativefeedback::intersect({x,y,right-x,bottom-y},{0,0,width_,height_});if(!result.valid())return;if(total_>=262144){overflow_=true;return;}plan_.layers[lane_].push_back({result,color});++total_;}
 template<class Read>bool text(BitmapFont&font,Read read,const std::u16string&s,int x,int y,uint32_t color,uint32_t flags=0,bool background=false,int mx=1,int my=1,int maxWidth=INT32_MAX){
  int measured=0;if(!font.measure(read,s,measured))return false;if(flags&0x100)x-=measured/2;else if(flags&0x200)x-=measured;if(background)fill({x-mx,y-my,measured+2*mx,font.lineHeight()+2*my},0xFF000000);
  int start=x;const int rightLimit=int(std::min<int64_t>(nativeW_,int64_t(x)+maxWidth));
  for(auto ch:s){if(ch=='\r')continue;if(ch=='\n'){x=start;y+=font.lineHeight();continue;}if(ch=='\t'){x=start+(x-start+64)/64*64;continue;}auto*g=font.glyph(read,ch);if(!g)return false;uint32_t ink=g->missing?missingColor(color):color;
   if(x<rightLimit&&x+g->width>0&&y<nativeH_&&y+g->rows>0)for(int pass=0;pass<2;++pass)for(int row=0;row<g->rows;++row)for(int col=0;col<g->width;){if(!g->ink[size_t(row)*g->width+col]){++col;continue;}int left=col++;while(col<g->width&&g->ink[size_t(row)*g->width+col])++col;int xx=x+left+int(pass==0),rr=std::min(x+col+int(pass==0),rightLimit);if(rr>xx)fill({xx,y+row+int(pass==0),rr-xx,1},pass?ink:0xFF000000);}
   x+=g->width+1;
  }return good();
 }
 void planning(std::array<int,2>from,std::array<int,2>to,uint32_t color,uint32_t tick){int phase=int((uint32_t(0)-tick)>>5)&15;if(clipLine(from,to,nativeW_,nativeH_))patternLine(from,to,phase,[&](int x,int y){fill({x,y,1,1},color);},4);}
 void outline(nativefeedback::Rect r,uint32_t color){fill({r.x,r.y,r.w,1},color);fill({r.x,r.y+r.h-1,r.w,1},color);fill({r.x,r.y,1,r.h},color);fill({r.x+r.w-1,r.y,1,r.h},color);}
 void marker(std::array<int,2>at,int size,uint32_t color,bool outer){nativefeedback::Rect r{at[0]-size/2,at[1]-size/2,size,size};if(outer)outline(r,color);else fill(r,color);}
 void planningStroke(std::array<int,2>from,std::array<int,2>to,uint32_t color,uint32_t tick,int width,bool outer){
  int minor=std::abs(to[1]-from[1])<std::abs(to[0]-from[0])?1:0;
  for(int offset=outer?width/2-width+1:0;offset<=(outer?width/2:width-1);++offset){auto a=from,b=to;a[minor]+=offset;b[minor]+=offset;
   if(!outer){planning(a,b,color,tick);continue;}if(!clipLine(a,b,nativeW_,nativeH_))continue;
   patternLine(a,b,0,[&](int x,int y){fill({x,y,1,1},color);},8);
  }
 }
 void rally(std::array<int,2>from,std::array<int,2>to,uint32_t color,int frame){int phase=int((int64_t(INT32_MAX)-frame)%15);from[1]+=2;to[1]+=2;for(uint32_t pass:{0xFF000000u,color,color}){if(clipLine(from,to,nativeW_,nativeH_))patternLine(from,to,phase,[&](int x,int y){fill({x,y,1,1},pass);});--from[1];--to[1];}}
};
class Engine {
 BitmapFont font_;Strings strings_;
 liveshape::Cache shapes_;nativeeffects::UnitUIStencil unitPips_;
 template<class Read>bool icon(Read read,Painter&p,uint32_t shape,int frame,int x,int y,uint32_t palette=0x87F6C4,bool required=false){
  auto image=shapes_.get(read,shape,frame,palette);if(!image)return !required;
  for(int row=0;row<image->height;++row)for(int col=0;col<image->width;){uint32_t color=image->pixels[size_t(row)*image->width+col];if(!(color>>24)){++col;continue;}int start=col++;while(col<image->width&&image->pixels[size_t(row)*image->width+col]==color)++col;p.fill({x+image->x+start,y+image->y+row,col-start,1},color);}return p.good();
 }
 template<class Read>bool pips(Read read,const mainview::View&v,const std::vector<nativefeedback::Object>&objects,Painter&p){
  std::vector<nativeeffects::UnitUIStencil::Command>commands;if(!unitPips_.commands(read,v,objects,commands))return false;p.lane(2);
  for(const auto&c:commands)if(!icon(read,p,c.shape,int(c.frame),c.x,c.y,0x87F6C4,true))return false;return true;
 }
 template<class Read>bool statuses(Read read,const mainview::View&v,const std::vector<nativefeedback::Object>&objects,Painter&p,bool&fontUsed){GameView game(read);double scale=projectionZScale(read);p.lane(2);
  auto project=[&](nativefeedback::Point3 q){return std::array<int,2>{{int((int64_t(q.x)-q.y)*30/256)-v.cameraX,int((int64_t(q.x)+q.y)*15/256)-screenHeight(q.z,scale)-v.cameraY}};};
  for(const auto&o:objects){int kind=game.kind(o.pointer);if(kind<0||kind!=o.kind||game.template get<uint32_t>(o.pointer+0x10)!=o.id||game.type(o.pointer,kind)!=o.type||!game.live(o.pointer)||game.template get<int>(o.pointer+0x220)!=0)continue;nativefeedback::Point3 point;if(!read(o.pointer+0x9C,&point,12)||!nativefeedback::worldPoint(point))continue;int w=1,h=1;if(kind==3&&animations::foundation(read,o.type,w,h)){point.x+=(w-1)*128;point.y+=(h-1)*128;}auto at=project(point);
   // Raw Veterancy, before selected/hovered health admission (70A990).
   if(o.owner==v.house||!game.template get<uint32_t>(o.pointer+0x518)){float raw=0;if(read(o.pointer+0x150,&raw,4)&&std::isfinite(raw)){int rank=raw<0?19:raw>=2?15:raw>=1?14:-1;if(rank>=0){uint32_t shape=game.template get<uint32_t>(0xAC147C);if(!icon(read,p,shape,rank,at[0]+(kind==1?5:10),at[1]+(kind==1?2:6)))return false;}}}
   if(o.owner==v.house&&game.template get<uint8_t>(o.pointer+0x83)){int group=game.template get<int>(o.pointer+0x214,-1);if(group>=0&&group<10){int x=at[0]-14,y=at[1]-(kind==1?29:26);if(kind==3){x=at[0]-4-(w+h)*15;y=at[1]-36+(h-w)*15/2;}if(!fontUsed&&!font_.begin(read))return false;fontUsed=true;uint32_t color=0;if(!houseColor(read,o.owner,color)||!p.text(font_,read,number((group+1)%10),x,y,color,0x49,true,2,-2))return false;}}
   if(kind==3){repairvisuals::Sprite repair;if(repairvisuals::sample(read,o.pointer,o.id,repair)){auto q=project({repair.x,repair.y,repair.z});if(!icon(read,p,repair.shape,repair.frame,q[0],q[1],0x87F6C8))return false;}}
  }return true;
 }
 template<class Read,class UI>bool paths(Read fresh,UI ui,const mainview::View&v,Painter&p,uint32_t tick){
  planning::Graph graph;if(!planning::capture(fresh,ui,v.house,graph))return false;if(!graph.active)return true;p.lane(3);
  double scale=projectionZScale(fresh);auto project=[&](nativefeedback::Point3 q){return std::array<int,2>{{int((int64_t(q.x)-q.y)*30/256)-v.cameraX,int((int64_t(q.x)+q.y)*15/256)-screenHeight(q.z,scale)-v.cameraY}};};
  auto color=[&](uint32_t owner,bool bright){const auto&rgb=graph.colors.at(owner);uint32_t value=0;for(int n=0;n<3;++n){int c=rgb[n],target=bright?255:0;c+=(target-c)*(bright?100:80)/256;value|=uint32_t(c)<<(16-8*n);}return rgb565(value);};
  auto outer=[&](uint32_t actor){return graph.white.count(actor)?0xFFFFFFFFu:0xFF000000u;};
  auto markerColor=[&](const planning::Node&node){uint32_t dark=color(node.owner,false);for(const auto&line:node.incoming)if(!line.loop&&color(line.owner,line.bright)==dark)return dark;return color(node.owner,node.bright);};
  uint32_t mouse=0,shape=0;ui(nativefeedback::MousePointer,&mouse,4);if(nativefeedback::range(mouse,8))ui(mouse+4,&shape,4);
  auto badge=[&](int frame,std::array<int,2>from,std::array<int,2>to,uint32_t ink,uint32_t background,bool edge){
   if(!frame||std::hypot(double(to[0])-from[0],double(to[1])-from[1])<37)return;
   int x=(from[0]+to[0])/2,y=(from[1]+to[1])/2;
   if(edge){p.outline({x-10,y-10,21,21},background);return;}
   p.outline({x-9,y-9,19,19},ink);p.fill({x-8,y-8,17,17},background);
   // Mouse rows76/77 are SHP frames433/434, not numbered text. A missing
   // cursor asset omits only its icon, never the complete planning graph.
   if(auto image=shapes_.get(fresh,shape,frame,0x87F6C8))for(int row=0;row<image->height;++row)for(int col=0;col<image->width;++col){uint32_t c=image->pixels[size_t(row)*image->width+col];if(c>>24)p.fill({x-image->width/2-1+col,y-image->height/2+row,1,1},c);}
  };
  // PlanMgr63B0A0 must finish every outline before63B150 interiors.
  for(const auto&node:graph.nodes){uint32_t edge=outer(node.members.front());for(const auto&line:node.incoming){auto from=project(line.from),to=project(line.to);p.planningStroke(from,to,edge,tick,4,true);badge(node.badge,from,to,edge,edge,true);}p.marker(project(node.point),8,edge,true);}
  auto actorLines=[&](const planning::Actor&actor,bool edge){uint32_t border=outer(actor.pointer);auto draw=[&](const planning::Segment&line,int frame,int width,bool end=true){auto from=project(line.from),to=project(line.to);uint32_t ink=edge?border:color(line.owner,line.bright);p.marker(from,edge?6:4,ink,edge);p.planningStroke(from,to,ink,tick,width,edge);if(end)p.marker(to,edge?6:4,ink,edge);badge(frame,from,to,ink,border,edge);};
   if(actor.markerOnly){p.marker(project(actor.marker),edge?6:4,edge?border:color(actor.owner,false),edge);return;}
   if(actor.hasCurrent)draw(actor.current,actor.currentBadge,edge?3:1);if(actor.hasMega)draw(actor.mega,434,edge?3:1);if(actor.hasConnection)draw(actor.connection,actor.badge,actor.hasCurrent?(edge?3:1):(edge?4:2),edge);
  };
  for(const auto&actor:graph.actors)actorLines(actor,true);
  for(const auto&node:graph.nodes){for(const auto&line:node.incoming){auto from=project(line.from),to=project(line.to);uint32_t ink=color(line.owner,line.bright);p.planningStroke(from,to,ink,tick,2,false);badge(node.badge,from,to,ink,outer(node.members.front()),false);}p.marker(project(node.point),6,markerColor(node),false);}
  for(const auto&node:graph.nodes)p.marker(project(node.point),6,markerColor(node),false);
  for(const auto&actor:graph.actors)actorLines(actor,false);
  p.lane(4); // PlanMgr63B2F0 follows ordinary action lines in Tactical.
  for(const auto&node:graph.nodes)for(auto highlight:{std::pair<uint32_t,int>{graph.hover,16},{graph.highlight,24}})if(node.pointer==highlight.first){auto at=project(node.point);for(int inset=0;inset<3;++inset){int size=highlight.second;p.outline({at[0]-size/2+inset,at[1]-size/2+inset,size-inset*2,size-inset*2},0xFFFFFFFFu);}}
  return p.good();
 }
 template<class Read>bool labels(Read read,const mainview::View&v,Painter&p,Plan&out,bool&fontUsed){
  std::array<uint8_t,40>head{},after{};if(!read(0xA8BC60,head.data(),head.size()))return false;std::set<uint32_t>seen;
  auto draw=[&](uint32_t label){if(!nativefeedback::range(label,0x4C)||seen.size()>=256)return false;if(!seen.insert(label).second)return true;std::array<uint8_t,0x4C>d{},verify{};if(!read(label,d.data(),d.size())||!read(label,verify.data(),verify.size())||d!=verify||nativefeedback::u32(d.data())!=0x7F5B44)return false;if(d[0x40])return true;
   std::u16string value;if(!string(read,nativefeedback::u32(d.data()+0x30),value))return false;if(value.empty())return true;int64_t x=int32_t(nativefeedback::u32(d.data()+0xC))-int64_t(v.x),y=int32_t(nativefeedback::u32(d.data()+0x10))-int64_t(v.y);if(std::abs(x)>32768||std::abs(y)>32768)return false;
   if(!fontUsed&&!font_.begin(read))return false;fontUsed=true;uint32_t color=0;if(!schemeColor(read,int32_t(nativefeedback::u32(d.data()+0x34)),color))return false;int limit=int32_t(nativefeedback::u32(d.data()+0x38));if(limit<0)limit=v.width;limit=std::clamp(limit,0,v.width);
   if(!p.text(font_,read,value,int(x),int(y),color,0,false,0,0,limit))return false;++out.messages;return true;
  };
  uint32_t label=nativefeedback::u32(head.data());while(label){if(seen.count(label)||!draw(label))return false;uint32_t next=0;if(!read(label+4,&next,4))return false;label=next;}if(head[0x19]){uint32_t edit=nativefeedback::u32(head.data()+0x24);if(edit&&!draw(edit))return false;}
  return read(0xA8BC60,after.data(),after.size())&&head==after;
 }
 template<class Read>bool timers(Read read,const mainview::View&v,Painter&p,Plan&out,bool&fontUsed){
  auto draw=[&](const std::u16string&title,int left,uint32_t color,uint32_t timeColor){if(out.timers>unsigned(v.height))return true;if(!fontUsed&&!font_.begin(read))return false;fontUsed=true;auto label=title+u"  ",time=clockText(left);int tw=0;if(!font_.measure(read,time,tw))return false;int y=v.height-(font_.lineHeight()+2)*int(out.timers+1);++out.timers;return p.text(font_,read,label,v.width-3-tw,y,color,0x200,true)&&p.text(font_,read,time,v.width-3,y,timeColor,0x200,true);};
  int start=-1,duration=0;uint32_t title=0;if(!read(v.scenario+0x11E8,&start,4)||!read(v.scenario+0x11F0,&duration,4))return false;
  if(start!=-1&&duration>0){std::u16string text;uint32_t color=0;if(!read(v.scenario+0x11F4,&title,4)||!string(read,title,text)||!houseColor(read,v.house,color)||!draw(text,remaining(start,duration,v.frame),color,color))return false;}
  GameView game(read);std::vector<uint32_t>supers;if(!game.vecChecked(0xA83D50,supers,8192))return false;
  for(auto super:supers){std::array<uint8_t,0x80>d{};if(!nativefeedback::range(super,d.size())||!read(super,d.data(),d.size())||nativefeedback::u32(d.data())!=0x7F3FE8)return false;uint32_t type=nativefeedback::u32(d.data()+0x28),owner=nativefeedback::u32(d.data()+0x2C);int first=int32_t(nativefeedback::u32(d.data()+0x30)),ticks=int32_t(nativefeedback::u32(d.data()+0x38));if(ticks<0)return false;int recharge=int32_t(nativefeedback::u32(d.data()+0x24));if(recharge<0)recharge=game.template get<int>(type+0xB0,-1);if(game.template get<int>(0xA8B238,-1)==0&&d[0x70]&&remaining(first,ticks,v.frame)==recharge)continue;
   std::u16string text;uint32_t color=0,timeColor=0;if(!nativefeedback::range(type,0x64)||!read(type+0x60,&title,4)||!string(read,title,text)||!houseColor(read,owner,color))return false;timeColor=color;if(!remaining(first,ticks,v.frame)&&d[0x40]&&!schemeColor(read,5,timeColor))return false;
   if(!draw(text,remaining(first,ticks,v.frame),color,timeColor))return false;std::array<uint8_t,0x14>verify{};if(!read(super+0x28,verify.data(),verify.size())||std::memcmp(d.data()+0x28,verify.data(),verify.size()))return false;
  }
  std::vector<uint32_t>houses;if(!game.vecChecked(0xA80228,houses,64))return false;for(auto house:houses){int first=-1,ticks=0;if(!nativefeedback::range(house,0x2B0)||!read(house+0x2A4,&first,4)||!read(house+0x2AC,&ticks,4))return false;int left=remaining(first,ticks,v.frame);if(left<=0)continue;std::u16string text;uint32_t color=0;if(!strings_.lookup(read,"MSG:BlackoutTimer",text)||!houseColor(read,house,color)||!draw(text,left,color,color))return false;}return true;
 }
 template<class Read>bool buildings(Read read,const mainview::View&v,const std::vector<nativefeedback::Object>&objects,Painter&p,Plan&out,bool&fontUsed){
  GameView game(read);double zscale=projectionZScale(read);auto project=[&](int x,int y,int z){return std::array<int,2>{int((int64_t(x)-y)*30/256)-v.cameraX,int((int64_t(x)+y)*15/256)-screenHeight(z,zscale)-v.cameraY};};
  for(const auto&o:objects){if(o.kind!=3)continue;std::array<uint8_t,0x530>d{};if(!nativefeedback::range(o.pointer,d.size())||!read(o.pointer,d.data(),d.size()))continue;if(nativefeedback::u32(d.data())!=0x7E3EBC||nativefeedback::u32(d.data()+0x10)!=o.id||nativefeedback::u32(d.data()+0x520)!=o.type||nativefeedback::u32(d.data()+0x21C)!=o.owner||d[0x81]||!d[0x74]||!d[0x90]||!d[0x83]||int32_t(nativefeedback::u32(d.data()+0x6C))<=0)continue;if(!game.allied(o.owner,v.house))continue;
   int xyz[3]{};std::memcpy(xyz,d.data()+0x9C,sizeof xyz);if(!nativefeedback::worldPoint({xyz[0],xyz[1],xyz[2]}))continue;int fw=0,fh=0;bool foundation=animations::foundation(read,o.type,fw,fh);uint32_t color=0;if(!houseColor(read,o.owner,color))return false;
   if(d[0x3D3]||game.template get<int>(o.type+0xEE4)>0){p.lane(2);auto point=project(xyz[0]-128,xyz[1]-128,xyz[2]);std::u16string text;if(d[0x3D3]){if(!strings_.lookup(read,foundation&&fw==1?"TXT_PRI":"TXT_PRIMARY",text))return false;}else{if(!strings_.lookup(read,"TXT_POWER_DRAIN2",text))return false;int values[2]{};if(!read(o.owner+0x53A4,values,8))return false;for(int value:values){size_t at=text.find(u"%d"),size=2;if(at==std::u16string::npos){at=text.find(u"%ld");size=3;}if(at!=std::u16string::npos)text.replace(at,size,number(value));}}if(!fontUsed&&!font_.begin(read))return false;fontUsed=true;if(!p.text(font_,read,text,point[0],point[1],color,0x149,true,4,2))return false;out.primary+=bool(d[0x3D3]);}
   // Building HasRallyPoint455DA0: Unit/Infantry factory, UnitRepair or
   // Cloning. VERA20K rally.py independently exercises these live fields.
   int factory=game.template get<int>(o.type+0xEB8,-1);bool rallyAllowed=factory==0x28||factory==16||game.template get<uint8_t>(o.type+0x16A9)||game.template get<uint8_t>(o.type+0x16AC);
   uint32_t target=nativefeedback::u32(d.data()+0x218);if(o.owner==v.house&&target&&foundation&&rallyAllowed){nativefeedback::Point3 destination;if(ground::target(read,target,destination)){auto from=project(xyz[0]+(fw-1)*128,xyz[1]+(fh-1)*128,xyz[2]);auto to=project(destination.x,destination.y,destination.z);p.lane(3);p.rally(from,to,color,v.frame);++out.rallies;}}

   std::array<uint8_t,0x530>verify{};if(!read(o.pointer,verify.data(),verify.size()))return false;for(auto field:{std::pair<size_t,size_t>{0,4},{0x10,4},{0x6C,4},{0x74,1},{0x81,3},{0x90,1},{0x9C,12},{0x218,8},{0x3D3,1},{0x520,4}})if(std::memcmp(d.data()+field.first,verify.data()+field.first,field.second))return false;
  }return true;
 }
public:
 void reset(){font_.reset();strings_.reset();shapes_.reset();unitPips_.clear();}
 template<class Read,class Fresh>bool capture(Read read,Fresh fresh,const mainview::View&view,const std::vector<nativefeedback::Object>&objects,int width,int height,Plan&out,uint32_t tick=0){
  out={};shapes_.begin();if(!view.valid()||width<1||height<1||width>8192||height>8192||uint64_t(width)*height>32*1024*1024||objects.size()>8192)return false;
  // Each independent producer publishes a complete layer transaction. A bad
  // timer/font/passenger read discards that producer. The task keeps its
  // last complete output until all active producers are ready again.
  const auto physicalView=mainview::physicalView(view);
  size_t total=0;auto capture=[&](auto producer,bool physical=false){Plan next;Painter painter(next,physical?physicalView.width:view.width,physical?physicalView.height:view.height,width,height);bool fontUsed=false;
   if(!producer(painter,next,fontUsed)||!painter.good()||(fontUsed&&!font_.stable(read))){out.complete=false;return;}
   size_t count=0;for(const auto&layer:next.layers)count+=layer.size();if(total+count>262144){out.complete=false;return;}total+=count;
   for(size_t n=0;n<out.layers.size();++n)out.layers[n].insert(out.layers[n].end(),next.layers[n].begin(),next.layers[n].end());
   out.messages+=next.messages;out.timers+=next.timers;out.primary+=next.primary;out.rallies+=next.rallies;
  };
  capture([&](Painter&p,Plan&next,bool&font){p.lane(0);return labels(fresh,physicalView,p,next,font);},true);
  capture([&](Painter&p,Plan&next,bool&font){p.lane(1);return timers(read,physicalView,p,next,font);},true);
  capture([&](Painter&p,Plan&next,bool&font){return buildings(read,view,objects,p,next,font);});
  capture([&](Painter&p,Plan&,bool&font){return statuses(read,view,objects,p,font);});
  capture([&](Painter&p,Plan&,bool&){return pips(read,view,objects,p);});
  capture([&](Painter&p,Plan&,bool&){return paths(read,fresh,view,p,tick);});
  // Range outlines/rays are owned by the main world surface (GPU or CPU).
  out.glyphDecodes=font_.decodes();return true;
 }
 template<class Read>bool capture(Read read,const mainview::View&view,const std::vector<nativefeedback::Object>&objects,int width,int height,Plan&out,uint32_t tick=0){return capture(read,read,view,objects,width,height,out,tick);}
};
}}
