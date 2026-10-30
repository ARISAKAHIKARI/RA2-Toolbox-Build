#pragma once
#include "native_ui_font.h"
#include "main_viewport.h"
#include "feedback_selection.h"
#include "building_animations.h"
#include "map_bounds.h"
#include <set>
namespace external {namespace localui {
// Main-view UI rendered from values and real font coverage. No framebuffer,
// passthrough rectangle or cutout is accepted or produced by this module.
struct Plan {std::array<std::vector<nativefeedback::Primitive>,4>layers;unsigned messages=0,timers=0,primary=0,rallies=0;uint64_t glyphDecodes=0;};
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
template<class Emit>void patternLine(std::array<int,2>a,std::array<int,2>b,int phase,Emit emit){
 int direction=1;if(a[0]>b[0]){phase=(phase+std::max(a[0]-b[0],std::abs(a[1]-b[1])+1))%16;direction=-1;std::swap(a,b);}int dx=b[0]-a[0],dy=std::abs(b[1]-a[1]),ys=a[1]>b[1]?-1:1,count=std::max(dx,dy)+int(dx==0||dy==0),error=2*std::min(dx,dy)-std::max(dx,dy);
 for(int i=0;i<count;++i){int bit=(phase%16+16)%16;if(bit%8<5)emit(a[0],a[1]);phase+=direction;if(dx>dy){if(error>0){a[1]+=ys;error-=2*dx;}++a[0];error+=2*dy;}else{if(error>0){++a[0];error-=2*dy;}a[1]+=ys;error+=2*dx;}}
}
class Painter {
 Plan&plan_;int nativeW_,nativeH_,width_,height_;double sx_,sy_;bool overflow_=false;size_t lane_=0,total_=0;
public:
 Painter(Plan&p,int nw,int nh,int w,int h):plan_(p),nativeW_(nw),nativeH_(nh),width_(w),height_(h),sx_(double(w)/nw),sy_(double(h)/nh){}
 bool good()const{return !overflow_;}
 void lane(size_t value){lane_=std::min<size_t>(value,3);}
 void fill(nativefeedback::Rect r,uint32_t color){r=nativefeedback::intersect(r,{0,0,nativeW_,nativeH_});if(!r.valid()||overflow_)return;auto edge=[](int v,double s){return int(std::ceil(v*s-.5));};int x=edge(r.x,sx_),y=edge(r.y,sy_),right=edge(r.x+r.w,sx_),bottom=edge(r.y+r.h,sy_);auto result=nativefeedback::intersect({x,y,right-x,bottom-y},{0,0,width_,height_});if(!result.valid())return;if(total_>=262144){overflow_=true;return;}plan_.layers[lane_].push_back({result,color});++total_;}
 template<class Read>bool text(BitmapFont&font,Read read,const std::u16string&s,int x,int y,uint32_t color,uint32_t flags=0,bool background=false,int mx=1,int my=1,int maxWidth=INT32_MAX){
  int measured=0;if(!font.measure(read,s,measured))return false;if(flags&0x100)x-=measured/2;else if(flags&0x200)x-=measured;if(background)fill({x-mx,y-my,measured+2*mx,font.lineHeight()+2*my},0xFF000000);
  int start=x;const int rightLimit=int(std::min<int64_t>(nativeW_,int64_t(x)+maxWidth));
  for(auto ch:s){if(ch=='\r')continue;if(ch=='\n'){x=start;y+=font.lineHeight();continue;}if(ch=='\t'){x=start+(x-start+64)/64*64;continue;}auto*g=font.glyph(read,ch);if(!g)return false;uint32_t ink=g->missing?missingColor(color):color;
   if(x<rightLimit&&x+g->width>0&&y<nativeH_&&y+g->rows>0)for(int pass=0;pass<2;++pass)for(int row=0;row<g->rows;++row)for(int col=0;col<g->width;){if(!g->ink[size_t(row)*g->width+col]){++col;continue;}int left=col++;while(col<g->width&&g->ink[size_t(row)*g->width+col])++col;int xx=x+left+int(pass==0),rr=std::min(x+col+int(pass==0),rightLimit);if(rr>xx)fill({xx,y+row+int(pass==0),rr-xx,1},pass?ink:0xFF000000);}
   x+=g->width+1;
  }return good();
 }
 void rally(std::array<int,2>from,std::array<int,2>to,uint32_t color,int frame){int phase=int((int64_t(INT32_MAX)-frame)%15);from[1]+=2;to[1]+=2;for(uint32_t pass:{0xFF000000u,color,color}){if(clipLine(from,to,nativeW_,nativeH_))patternLine(from,to,phase,[&](int x,int y){fill({x,y,1,1},pass);});--from[1];--to[1];}}
};
class Engine {
 BitmapFont font_;Strings strings_;
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
  for(auto super:supers){std::array<uint8_t,0x80>d{};if(!nativefeedback::range(super,d.size())||!read(super,d.data(),d.size())||nativefeedback::u32(d.data())!=0x7F3FE8)return false;uint32_t type=nativefeedback::u32(d.data()+0x28),owner=nativefeedback::u32(d.data()+0x2C);int first=int32_t(nativefeedback::u32(d.data()+0x30)),ticks=int32_t(nativefeedback::u32(d.data()+0x38));if(ticks<0)return false;
   std::u16string text;uint32_t color=0,timeColor=0;if(!nativefeedback::range(type,0x64)||!read(type+0x60,&title,4)||!string(read,title,text)||!houseColor(read,owner,color))return false;timeColor=color;if(!remaining(first,ticks,v.frame)&&d[0x40]&&!schemeColor(read,5,timeColor))return false;
   if(!draw(text,remaining(first,ticks,v.frame),color,timeColor))return false;std::array<uint8_t,0x14>verify{};if(!read(super+0x28,verify.data(),verify.size())||std::memcmp(d.data()+0x28,verify.data(),verify.size()))return false;
  }
  std::vector<uint32_t>houses;if(!game.vecChecked(0xA80228,houses,64))return false;for(auto house:houses){int first=-1,ticks=0;if(!nativefeedback::range(house,0x2B0)||!read(house+0x2A4,&first,4)||!read(house+0x2AC,&ticks,4))return false;int left=remaining(first,ticks,v.frame);if(left<=0)continue;std::u16string text;uint32_t color=0;if(!strings_.lookup(read,"MSG:BlackoutTimer",text)||!houseColor(read,house,color)||!draw(text,left,color,color))return false;}return true;
 }
 template<class Read>bool buildings(Read read,const mainview::View&v,const std::vector<nativefeedback::Object>&objects,Painter&p,Plan&out,bool&fontUsed){
  GameView game(read);double zscale=projectionZScale(read);auto project=[&](int x,int y,int z){return std::array<int,2>{int((int64_t(x)-y)*30/256)-v.cameraX,int((int64_t(x)+y)*15/256)-screenHeight(z,zscale)-v.cameraY};};
  for(const auto&o:objects){if(o.kind!=3)continue;std::array<uint8_t,0x530>d{};if(!nativefeedback::range(o.pointer,d.size())||!read(o.pointer,d.data(),d.size()))continue;if(nativefeedback::u32(d.data())!=0x7E3EBC||nativefeedback::u32(d.data()+0x10)!=o.id||nativefeedback::u32(d.data()+0x520)!=o.type||nativefeedback::u32(d.data()+0x21C)!=o.owner||d[0x81]||!d[0x74]||!d[0x90]||!d[0x83]||int32_t(nativefeedback::u32(d.data()+0x6C))<=0)continue;if(!game.allied(o.owner,v.house))continue;
   int xyz[3]{};std::memcpy(xyz,d.data()+0x9C,sizeof xyz);if(!nativefeedback::worldPoint({xyz[0],xyz[1],xyz[2]}))continue;int fw=0,fh=0;bool foundation=animations::foundation(read,o.type,fw,fh);uint32_t color=0;if(!houseColor(read,o.owner,color))return false;
   if(d[0x3D3]){p.lane(2);auto point=project(xyz[0]-128,xyz[1]-128,xyz[2]);std::u16string text;if(!strings_.lookup(read,foundation&&fw==1?"TXT_PRI":"TXT_PRIMARY",text))return false;if(!fontUsed&&!font_.begin(read))return false;fontUsed=true;if(!p.text(font_,read,text,point[0],point[1],color,0x149,true,4,2))return false;++out.primary;}
   // Building HasRallyPoint455DA0: Unit/Infantry factory, UnitRepair or
   // Cloning. VERA20K rally.py independently exercises these live fields.
   int factory=game.template get<int>(o.type+0xEB8,-1);bool rallyAllowed=factory==0x28||factory==16||game.template get<uint8_t>(o.type+0x16A9)||game.template get<uint8_t>(o.type+0x16AC);
   uint32_t target=nativefeedback::u32(d.data()+0x218);if(o.owner==v.house&&target&&foundation&&rallyAllowed){int16_t cell[2]{};auto bounds=mapbounds::sample(read);uint32_t cells=0,actual=0;int count=0;int8_t level=0;if(!nativefeedback::range(target,0x148)||!read(target+0x24,cell,sizeof cell)||!bounds.legal(cell[0],cell[1]))continue;int index=int(cell[1])*512+cell[0];if(!read(mapbounds::Map+0x13C,&cells,4)||!read(mapbounds::Map+0x140,&count,4)||index>=count||count>512*512||!nativefeedback::range(cells,size_t(count)*4)||!read(cells+index*4,&actual,4)||actual!=target||!read(target+0x11B,&level,1))continue;uint32_t flags=0;if(!read(target+0x140,&flags,4))return false;
    auto from=project(xyz[0]+(fw-1)*128,xyz[1]+(fh-1)*128,xyz[2]);std::array<int,2>to{(int(cell[0])-cell[1])*30-v.cameraX,(int(cell[0])+cell[1]+1)*15-int(level)*15-((flags&0x100)?60:0)-v.cameraY};p.lane(3);p.rally(from,to,color,v.frame);++out.rallies;
   }
   std::array<uint8_t,0x530>verify{};if(!read(o.pointer,verify.data(),verify.size()))return false;for(auto field:{std::pair<size_t,size_t>{0,4},{0x10,4},{0x6C,4},{0x74,1},{0x81,3},{0x90,1},{0x9C,12},{0x218,8},{0x3D3,1},{0x520,4}})if(std::memcmp(d.data()+field.first,verify.data()+field.first,field.second))return false;
  }return true;
 }
public:
 void reset(){font_.reset();strings_.reset();}
 template<class Read>bool capture(Read read,const mainview::View&view,const std::vector<nativefeedback::Object>&objects,int width,int height,Plan&out){out={};if(!view.valid()||width<1||height<1||width>8192||height>8192||uint64_t(width)*height>16*1024*1024||objects.size()>8192)return false;Painter painter(out,view.width,view.height,width,height);bool fontUsed=false;painter.lane(0);if(!labels(read,view,painter,out,fontUsed)){out={};return false;}painter.lane(1);if(!timers(read,view,painter,out,fontUsed)){out={};return false;}if(!buildings(read,view,objects,painter,out,fontUsed)||!painter.good()||(fontUsed&&!font_.stable(read))){out={};return false;}out.glyphDecodes=font_.decodes();return true;}
};
}}
