#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "../source/protocol.h"
#include "../source/additional_settings.h"
#include "../source/native_viewport_host.h"
#include "../source/formation_quick.h"
using namespace assist;
static Shared storage,*shared=&storage;static Snapshot snap;static AdditionalSettings extra;static uint32_t pid=100,epoch=7;static DWORD now=1000;
static DWORD testTick(){return now;}
#define GetTickCount testTick
static uint32_t commandMatch(){return epoch;}static bool foregroundGame(){return true;}static std::wstring runtimeGestureDescription(uint32_t){return L"gesture";}
struct PointF{float X=0,Y=0;PointF()=default;PointF(float x,float y):X(x),Y(y){}};struct RectF{float X,Y,Width,Height;RectF(float x,float y,float w,float h):X(x),Y(y),Width(w),Height(h){}};
struct Color{template<class...T>Color(T...){} };struct Pen{Pen(Color,float){}void SetDashStyle(int){}};struct SolidBrush{SolidBrush(Color){}};
constexpr int DashStyleDash=1,CombineModeIntersect=1,PixelFormat32bppPARGB=1,UnitPixel=1;
struct Bitmap{Bitmap(int,int,int,int,BYTE*){}};
struct Graphics{
 std::vector<std::array<float,4>>lines,images;std::vector<std::vector<PointF>>polygons;std::vector<RectF>clips;std::vector<PointF>texts;
 int Save(){return 1;}void Restore(int){}void SetClip(RectF r,int){clips.push_back(r);}
 void DrawLine(Pen*,PointF a,PointF b){lines.push_back({a.X,a.Y,b.X,b.Y});}
 void FillEllipse(SolidBrush*,float,float,float,float){}void FillPolygon(SolidBrush*,PointF*p,int n){polygons.emplace_back(p,p+n);}void DrawPolygon(Pen*,PointF*,int){}
 void DrawImage(Bitmap*,RectF r,float,float,float,float,int,void*){images.push_back({r.X,r.Y,r.Width,r.Height});}
};
static void drawText(Graphics&g,const std::wstring&,float x,float y,int,Color){g.texts.push_back({x,y});}
namespace formationhost {struct Image {int x=200,y=200,width=60,height=30;std::vector<uint32_t>pixels=std::vector<uint32_t>(1800,0xff000000);};struct Frame {uint32_t pid=100,match=7,tick=1000;external::Projection projection;formation::Preview preview;int missing=0;std::shared_ptr<Image>composite;nativeviewhost::Transform transform;};static std::shared_ptr<Frame>frame;static bool visible(){return true;}}
namespace airway {struct Point{uint32_t cell=0;int z=0;};struct Job{bool selected=true;uint32_t match=7;int x=2560,y=2560,z=0;std::vector<Point>points;};struct Line{int x,y,tx,ty;};static std::vector<Job>jobs;static bool active(){return true;}}
#include MO_DRAWING_ROUTES
static int checks=0;static void check(bool yes){assert(yes);++checks;}static bool closeFloat(float x,float y){return std::abs(x-y)<.001f;}
static nativeviewhost::Transform transform{{168,16,632,568},{0,0,948,852},7,true};
static void publish(){shared->viewportSerial+=1;shared->viewportActive=transform.active;shared->viewportGeneration=transform.generation;const int a[]={168,16,632,568},r[]={0,0,948,852};for(int n=0;n<4;++n){shared->viewportPhysical[n]=a[n];shared->viewportLogical[n]=r[n];}shared->viewportSerial+=1;}
int main(){
 snap.cameraX=-200;snap.cameraY=100;snap.width=800;snap.height=600;publish();extra.moFeatures=uint32_t(moorders::Fuzzy);extra.formationMouse=2;
 shared->placementPreview.active=1;shared->placementPreview.match=epoch;shared->placementPreview.tick=now;shared->placementPreview.status=1;shared->placementPreview.x=shared->placementPreview.y=2560;shared->placementPreview.width=shared->placementPreview.height=1;shared->placementPreview.cameraX=-200;shared->placementPreview.cameraY=100;shared->placementPreview.zScale=.0866025403784;shared->placementPreview.viewportGeneration=7;
 auto p=placementdrawing::capture();check(p.active&&p.transform==transform);Graphics g;placementdrawing::draw(g,p);check(g.polygons.size()==1&&g.polygons[0].size()==4);check(closeFloat(g.polygons[0][0].X,301.333333f)&&closeFloat(g.polygons[0][0].Y,149.333333f));check(closeFloat(g.polygons[0][1].X,321.333333f)&&closeFloat(g.polygons[0][1].Y,159.333333f));check(g.texts.size()==1&&g.texts[0].X==18&&g.texts[0].Y==88);check(g.clips.size()==1&&g.clips[0].X==168&&g.clips[0].Width==632);
 ++transform.generation;publish();check(!placementdrawing::capture().active);shared->placementPreview.viewportGeneration=8;auto fresh=placementdrawing::capture();check(fresh.active&&!placementdrawing::same(p,fresh));shared->viewportSerial|=1;check(!placementdrawing::capture().active);++shared->viewportSerial;
 shared->moFeatures=uint32_t(moorders::Fuzzy);auto&m=shared->moOrdersPreview;m.active=1;m.match=epoch;m.tick=now;m.mode=int(moorders::Fuzzy);m.count=1;m.lines[0].from={2560,2560,0};m.lines[0].to={2816,2560,0};m.zScale=.0866025403784;
 auto mo=moordershost::drawing();check(mo.active&&mo.transform.generation==8);PointF q;check(moordershost::point(mo,{2560,2560,0},q)&&closeFloat(q.X,301.333333f)&&closeFloat(q.Y,149.333333f));Graphics mg;moordershost::draw(mg,mo);check(mg.lines.size()==2&&closeFloat(mg.lines[0][0],301.333333f)&&closeFloat(mg.lines[0][2],321.333333f));check(mg.texts.size()==1&&mg.texts[0].X==18&&mg.texts[0].Y==66);
 auto old=mo;++transform.generation;publish();mo=moordershost::drawing();check(mo.active&&!moordershost::sameDrawing(old,mo));shared->viewportSerial|=1;check(!moordershost::drawing().active);++shared->viewportSerial;
 formationhost::frame=std::make_shared<formationhost::Frame>();formationhost::frame->transform=transform;formationhost::frame->projection={-200,100,948,852,.0866025403784};formationhost::frame->composite=std::make_shared<formationhost::Image>();auto f=formationhost::drawing();check(f.frame&&f.transform==transform);Graphics fg;formationhost::draw(fg,f);check(fg.images.size()==1&&closeFloat(fg.images[0][0],301.333333f)&&closeFloat(fg.images[0][1],149.333333f)&&closeFloat(fg.images[0][2],40)&&closeFloat(fg.images[0][3],20));check(!fg.texts.empty()&&fg.texts[0].X==18&&fg.texts[0].Y==44);++transform.generation;publish();check(!formationhost::drawing().frame);
 airway::Job job;job.points.push_back({10010,0});airway::jobs.push_back(job);auto lines=airway::lines();check(lines.size()==1&&lines[0].x==301&&lines[0].y==149);shared->viewportSerial|=1;check(airway::lines().empty());++shared->viewportSerial;
 printf("{\"passed\":true,\"checks\":%d,\"scope\":\"actual placement/MO/formation/air-path drawing and capture functions; recorded GDI+ calls and Shared snapshots; no Windows rendering acceptance\"}\n",checks);
}
