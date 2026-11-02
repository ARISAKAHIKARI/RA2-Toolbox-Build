#include "middle_input_host.h"
#include "mo_orders.h"
#include "../../external_reader/source/projection.h"
// Projection only. The existing overlay owns clipping, painting and threads.
namespace moordershost {
struct Drawing {moorders::Preview preview;external::Projection projection;bool active=false;std::wstring hint;};
static Drawing drawing(){
 Drawing out;if(!shared||snap.chat||!foregroundGame())return out;
 long sequence=shared->moOrdersPreviewSeq;if(sequence&1)return out;MemoryBarrier();out.preview=shared->moOrdersPreview;MemoryBarrier();
 if(sequence!=shared->moOrdersPreviewSeq||!out.preview.active||out.preview.match!=commandMatch()||DWORD(GetTickCount()-out.preview.tick)>500||out.preview.count<0||out.preview.count>moorders::Lines||out.preview.nodeCount<0||out.preview.nodeCount>moorders::Nodes)return out;
 out.projection={snap.cameraX,snap.cameraY,snap.width,snap.height,out.preview.zScale};out.active=true;out.hint=runtimeGestureDescription(uint32_t(out.preview.mode));return out;
}
static bool point(const Drawing&d,const moorders::Point&p,PointF&out){
 if(std::abs(int64_t(p.x))>1048576||std::abs(int64_t(p.y))>1048576||std::abs(int64_t(p.z))>1048576)return false;
 out={float((int64_t(p.x)-p.y)*30/256-d.projection.cameraX),float((int64_t(p.x)+p.y)*15/256-external::screenHeight(p.z,d.projection.zScale)-d.projection.cameraY)};return true;
}
static void draw(Graphics&g,const Drawing&d){
 if(!d.active)return;const auto&p=d.preview;Color color=p.mode==int(moorders::Fuzzy)?Color(235,255,90,80):p.mode==int(moorders::Area)?Color(235,100,255,160):Color(235,230,244,255);
 Pen under(Color(170,0,0,0),3.f),line(color,1.f);line.SetDashStyle(DashStyleDash);SolidBrush brush(color);
 for(int i=0;i<p.count;++i){PointF a,b;if(point(d,p.lines[i].from,a)&&point(d,p.lines[i].to,b)){g.DrawLine(&under,a,b);g.DrawLine(&line,a,b);}}
 Pen edge(color,1.5f);for(int i=0;i<p.nodeCount;++i){PointF a,b;if(point(d,p.nodes[i],a)){g.FillEllipse(&brush,a.X-2,a.Y-2,4.f,4.f);if(p.mode!=int(moorders::Patrol)&&point(d,p.nodes[(i+1)%p.nodeCount],b))g.DrawLine(&edge,a,b);}}
 const std::wstring text=(p.mode==int(moorders::Fuzzy)?L"模糊攻击 · ":p.mode==int(moorders::Area)?L"目标分配 · ":L"自定义巡逻 · ")+d.hint+L" · 按住预览，松开鼠标执行 · Esc取消";
 drawText(g,text,18,66,12,color);
}
static void draw(Graphics&g){draw(g,drawing());}
}
