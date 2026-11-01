#include "passenger_motion.h"
#include "display_sample_lease.h"
static passengers::Motion passengerMotion;
struct PassengerDisplayKey {
 uint32_t pid=0,match=0,house=0;int width=0,height=0,cameraX=0,cameraY=0;bool inspect=false,persistent=false;
 bool operator==(const PassengerDisplayKey&o)const{return pid==o.pid&&match==o.match&&house==o.house&&width==o.width&&height==o.height&&cameraX==o.cameraX&&cameraY==o.cameraY&&inspect==o.inspect&&persistent==o.persistent;}
};
static displaylease::Sample<PassengerDisplayKey,std::vector<external::passengers::Bubble>> passengerSamples;
static void clearPassengerDisplay(){passengerMotion.clear();passengerSamples.clear();}
struct PassengerVisual {
 uint32_t id=0,color=0;float left=0,top=0,anchor=0;int width=0,height=0;bool danger=false;std::vector<std::wstring> lines;
 bool operator==(const PassengerVisual&o)const{return id==o.id&&color==o.color&&left==o.left&&top==o.top&&width==o.width&&height==o.height&&anchor==o.anchor&&danger==o.danger&&lines==o.lines;}
};
static std::vector<PassengerVisual> passengerView(uint32_t renderNow){
 // Called once per overlay paint, after the reader's per-paint page cache and
 // camera are refreshed. Position and contents share the overlay's cadence.
 std::vector<PassengerVisual> out;if(!enabled||!externalData||snap.chat||(!extra.passengerBubbles&&!extra.dangerPassengers)){clearPassengerDisplay();return out;}
 external::Projection projection;projection.width=snap.width-160;projection.height=snap.height;projection.cameraX=snap.cameraX;projection.cameraY=snap.cameraY;
 projection.zScale=externalReader.get<double>(0xB0CD48,0.0866025403784);
 auto read=[](uint32_t at,void*p,size_t n){return externalReader.read(at,p,n);};
 uint32_t house=0;bool houseReady=read(0xA83D4C,&house,4)&&house>=0x10000;
 PassengerDisplayKey key{pid,externalData->match,house,projection.width,projection.height,projection.cameraX,projection.cameraY,extra.passengerBubbles,extra.dangerPassengers};
 auto sampled=external::passengers::sampleChecked(read,house,projection,extra.passengerBubbles,extra.dangerPassengers);
 const auto*held=passengerSamples.observe(key,houseReady&&sampled.complete,std::move(sampled.bubbles),renderNow);
 if(!held){passengerMotion.clear();return out;}const auto&bubbles=*held;
 const auto* anchors=passengerMotion.update(bubbles,externalData->match,externalData->frame,externalData->sampledTick,renderNow,projection.cameraX,projection.cameraY);
 for(size_t index=0;index<bubbles.size();++index){const auto&b=bubbles[index];const auto&anchor=anchors[index];
  PassengerVisual v;v.id=b.id;v.color=b.color;v.danger=b.dangerous;size_t longest=0;
  for(const auto&group:b.groups){std::wstring line(reinterpret_cast<const wchar_t*>(group.name));if(line.size()>18)line=line.substr(0,17)+L"…";line+=L" ×"+std::to_wstring(group.count);longest=std::max(longest,line.size());v.lines.push_back(std::move(line));}
  size_t limit=size_t(std::max(2,std::min(10,(projection.height-48)/17)));if(v.lines.size()>limit){size_t remaining=v.lines.size()-limit+1;v.lines.resize(limit-1);v.lines.push_back(L"另有 "+std::to_wstring(remaining)+L" 类乘员");longest=std::max(longest,v.lines.back().size());}
  v.width=std::clamp(int(longest)*12+16,70,282);v.height=int(v.lines.size())*17+12;
  auto bounds=passengers::bounds(anchor.x,anchor.y,v.width,v.height,projection.width);v.left=bounds.left;v.top=bounds.top;v.anchor=bounds.anchor;out.push_back(std::move(v));
 }return out;
}
static void drawPassengerBubble(Graphics&g,const PassengerVisual&b){
 SolidBrush bg(Color(230,17,25,35)),ink(Color(b.color));Pen outline(Color(b.danger?255:210,b.danger?255:140,b.danger?190:170,b.danger?75:195),1.f);
 PointF tail[]={{float(b.anchor-4),float(b.top+b.height-1)},{float(b.anchor+4),float(b.top+b.height-1)},{float(b.anchor),float(b.top+b.height+5)}};
 g.FillPolygon(&bg,tail,3);g.FillRectangle(&bg,float(b.left),float(b.top),float(b.width),float(b.height));g.DrawRectangle(&outline,float(b.left),float(b.top),float(b.width),float(b.height));
 g.FillRectangle(&ink,float(b.left+1),float(b.top+1),2.f,float(b.height-2));
 for(size_t i=0;i<b.lines.size();++i)drawText(g,b.lines[i],float(b.left+8),float(b.top+5+i*17),12.f,Color(255,240,244,249));
}
