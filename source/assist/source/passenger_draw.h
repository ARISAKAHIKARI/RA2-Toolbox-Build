#include "../../external_reader/source/passenger_bubbles.h"
struct PassengerVisual {
 uint32_t id=0,color=0;int left=0,top=0,width=0,height=0,anchor=0;bool danger=false;std::vector<std::wstring> lines;
 bool operator==(const PassengerVisual&o)const{return id==o.id&&color==o.color&&left==o.left&&top==o.top&&width==o.width&&height==o.height&&anchor==o.anchor&&danger==o.danger&&lines==o.lines;}
};
static std::vector<PassengerVisual> passengerView(DWORD now){
 static DWORD last=0;static uint32_t match=0,options=0,process=0;static std::vector<PassengerVisual> cached;
 uint32_t currentOptions=uint32_t(extra.passengerBubbles)|(uint32_t(extra.dangerPassengers)<<1);
 std::vector<PassengerVisual> out;if(!enabled||!externalData||snap.chat||(!extra.passengerBubbles&&!extra.dangerPassengers)){cached.clear();last=0;return out;}
 if(last&&DWORD(now-last)<50&&match==uint32_t(snap.match)&&options==currentOptions&&process==pid)return cached;
 last=now;match=snap.match;options=currentOptions;process=pid;
 external::Projection projection;projection.width=snap.width-160;projection.height=snap.height;projection.cameraX=snap.cameraX;projection.cameraY=snap.cameraY;
 projection.zScale=externalReader.get<double>(0xB0CD48,0.0866025403784);
 auto read=[](uint32_t at,void*p,size_t n){return externalReader.read(at,p,n);};
 for(const auto&b:external::passengers::sample(read,externalReader.get<uint32_t>(0xA83D4C),projection,extra.passengerBubbles,extra.dangerPassengers)){
  PassengerVisual v;v.id=b.id;v.color=b.color;v.danger=b.dangerous;size_t longest=0;
  for(const auto&group:b.groups){std::wstring line(reinterpret_cast<const wchar_t*>(group.name));if(line.size()>18)line=line.substr(0,17)+L"…";line+=L" ×"+std::to_wstring(group.count);longest=std::max(longest,line.size());v.lines.push_back(std::move(line));}
  size_t limit=size_t(std::max(2,std::min(10,(projection.height-48)/17)));if(v.lines.size()>limit){size_t remaining=v.lines.size()-limit+1;v.lines.resize(limit-1);v.lines.push_back(L"另有 "+std::to_wstring(remaining)+L" 类乘员");longest=std::max(longest,v.lines.back().size());}
  v.width=std::clamp(int(longest)*12+16,70,282);v.height=int(v.lines.size())*17+12;
  v.left=std::clamp(b.x-v.width/2,0,std::max(0,projection.width-v.width));v.top=std::max(0,b.y-26-v.height);v.anchor=std::clamp(b.x,v.left+8,v.left+v.width-8);out.push_back(std::move(v));
 }cached=out;return out;
}
static void drawPassengerBubble(Graphics&g,const PassengerVisual&b){
 SolidBrush bg(Color(230,17,25,35)),ink(Color(b.color));Pen outline(Color(b.danger?255:210,b.danger?255:140,b.danger?190:170,b.danger?75:195),1.f);
 PointF tail[]={{float(b.anchor-4),float(b.top+b.height-1)},{float(b.anchor+4),float(b.top+b.height-1)},{float(b.anchor),float(b.top+b.height+5)}};
 g.FillPolygon(&bg,tail,3);g.FillRectangle(&bg,float(b.left),float(b.top),float(b.width),float(b.height));g.DrawRectangle(&outline,float(b.left),float(b.top),float(b.width),float(b.height));
 g.FillRectangle(&ink,float(b.left+1),float(b.top+1),2.f,float(b.height-2));
 for(size_t i=0;i<b.lines.size();++i)drawText(g,b.lines[i],float(b.left+8),float(b.top+5+i*17),12.f,Color(255,240,244,249));
}
