// Execute the real vector drawing helper after the real sampler/Motion path.
// GDI calls are recorded; this does not claim a Windows screenshot test.
#define main previous_enemy_marker_test_main
#include "enemy_markers.cpp"
#undef main
#include "../source/status_badges.h"
using namespace assist;
struct Color {uint32_t v;Color(uint32_t c):v(c){}Color(int a,int r,int g,int b):v((uint32_t(a)<<24)|(r<<16)|(g<<8)|b){}int GetR(){return(v>>16)&255;}int GetG(){return(v>>8)&255;}int GetB(){return v&255;}};
struct PointF {float x,y;};struct Pen {Color color;float width;Pen(Color c,float w):color(c),width(w){}void SetLineJoin(int){}};
struct SolidBrush {Color color;SolidBrush(Color c):color(c){}};
constexpr int SmoothingModeAntiAlias=1,LineJoinRound=2;
struct Graphics {
 struct Transform{float x=0,y=0,sx=1,sy=1;}t;
 struct Call{float l,t,r,b;uint32_t color;};std::vector<Call> calls;std::vector<Transform> saved;
 int Save(){saved.push_back(t);return int(saved.size());}void Restore(int i){t=saved.at(i-1);saved.resize(i-1);}void SetSmoothingMode(int){}
 void TranslateTransform(float x,float y){t.x+=x*t.sx;t.y+=y*t.sy;}void ScaleTransform(float x,float y){t.sx*=x;t.sy*=y;}
 void lines(Pen*p,PointF*v,int n){float l=1e6f,u=1e6f,r=-1e6f,b=-1e6f;for(int i=0;i<n;++i){l=std::min(l,v[i].x-p->width/2);r=std::max(r,v[i].x+p->width/2);u=std::min(u,v[i].y-p->width/2);b=std::max(b,v[i].y+p->width/2);}calls.push_back({t.x+l*t.sx,t.y+u*t.sy,t.x+r*t.sx,t.y+b*t.sy,p->color.v});}
 void DrawLines(Pen*p,PointF*v,int n){lines(p,v,n);}void DrawPolygon(Pen*p,PointF*v,int n){lines(p,v,n);}
 void DrawLine(Pen*p,float x,float y,float a,float b){PointF q[]={{x,y},{a,b}};lines(p,q,2);}
 void FillEllipse(SolidBrush*p,float x,float y,float w,float h){calls.push_back({t.x+x*t.sx,t.y+y*t.sy,t.x+(x+w)*t.sx,t.y+(y+h)*t.sy,p->color.v});}
 void FillPolygon(SolidBrush*p,PointF*v,int n){Pen edge(p->color,0);lines(&edge,v,n);}
};
#include "../source/status_draw.h"
static void near(float a,float b){assert(std::abs(a-b)<.001f);}
int main(){
 external::status::Marker m;m.buffs=7;m.parasite=1;m.parasiteColor=0xffaabbcc;
 visual::LabelView v{};v.id=1;v.x=100.125f;v.y=200.375f;Graphics first;drawUnitStatus(first,m,v,true);auto box=statusbadges::bounds(v.x,v.y,m.buffs,true);
 for(const auto&c:first.calls){assert(c.l>=box.x-.001f&&c.r<=box.x+box.w+.001f&&c.t>=box.y-.001f&&c.b<=box.y+box.h+.001f);}
 assert(first.calls.size()>20&&first.saved.empty());
 for(int step=0;step<500;++step){auto q=v;q.x+=step*.125f;q.y-=step*.25f;m.x=step%2?1000:-1000;m.y=step%3?300:-300;Graphics g;drawUnitStatus(g,m,q,true);assert(g.calls.size()==first.calls.size());
  for(size_t i=0;i<g.calls.size();++i){near(g.calls[i].l-first.calls[i].l,step*.125f);near(g.calls[i].t-first.calls[i].t,-step*.25f);}
 }
 EnemyFixture f;f.mem.put<double>(EnemyFixture::unit+0x160,1.5);f.mem.put<double>(EnemyFixture::unit+0x158,1.5);f.mem.put<double>(EnemyFixture::unit+0x580,1.5);
 visual::Motion labels,status;Snapshot input;visual::LabelView labelsOut[MaxLabels],statusOut[MaxLabels];
 for(int step=0;step<1000;++step){if(step%8==0)f.next();f.now+=2;f.mem.put<int>(EnemyFixture::unit+0x9C,42*256+step%128*2);f.mem.put<int>(EnemyFixture::tactical+0xB0,-320+step/3);
  const auto&s=f.sampler.sample(f.now,Capacity,false,3);const auto&markers=f.sampler.unitStatuses();assert(markers.tick==s.sampledTick&&markers.frame==s.frame&&markers.count==1);
  labels.update(s);input.labels=markers.count;input.match=markers.match;input.sampledTick=markers.tick;input.frame=markers.frame;input.cameraX=s.cameraX;input.cameraY=s.cameraY;input.label[0]={markers.items[0].id,markers.items[0].x,markers.items[0].y};status.update(input);
  assert(labels.render(f.now,labelsOut)==1&&status.render(f.now,statusOut)==1);near(labelsOut[0].x,statusOut[0].x);near(labelsOut[0].y,statusOut[0].y);
  Graphics g;drawUnitStatus(g,markers.items[0],statusOut[0],true);auto l=statusbadges::layout(labelsOut[0].x,labelsOut[0].y,7,false);near(g.calls[0].l,l.icons[0].x-4.5f);near(g.calls[0].t,l.icons[0].y-4.5f);
 }
 std::cout<<"PASS production spider/buff strokes inside shared bounds, 500 subpixel draws ignore raw-cell jumps, and 1000 shared sampler/Motion/GDI frames agree with carrier overlays\n";
}
