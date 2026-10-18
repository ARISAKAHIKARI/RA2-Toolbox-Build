"""Execute the shipped enemy drawing helper with GDI+ recording stubs.
Geometry, cache and text checks are not Windows pixel/screenshot validation.
"""
from pathlib import Path
import subprocess,tempfile
R=Path(__file__).resolve().parents[2]
main=(R/'assist/source/main.cpp').read_text()
draw='static void drawEnemyMarker'+main.split('static void drawEnemyMarker',1)[1].split('#include "air_path_host.h"',1)[0].split('#include "construction_host.h"',1)[0]
prefix=r'''
#include "../../external_reader/source/enemy_markers.h"
#include "label_motion.h"
#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace assist;using ARGB=uint32_t;
constexpr int PixelFormat32bppPARGB=1,SmoothingModeAntiAlias=2,StringFormatFlagsNoWrap=3,FontStyleBold=4,LineJoinRound=5;
struct Color{ARGB v;Color(ARGB x):v(x){}Color(int a,int r,int g,int b):v((unsigned(a)<<24)|(r<<16)|(g<<8)|b){}};
struct PointF{float X,Y;PointF(float x,float y):X(x),Y(y){}};
struct Pen{Color color;float width;Pen(Color c,float w):color(c),width(w){}void SetLineJoin(int){}};
struct SolidBrush{Color color;SolidBrush(Color c):color(c){}};
static int allocations=0;
struct Bitmap{int w,h;Bitmap(int a,int b,int):w(a),h(b){++allocations;}int GetWidth(){return w;}int GetHeight(){return h;}};
struct FontFamily{FontFamily(const wchar_t*){}};
struct StringFormat{void SetFormatFlags(int){}};
struct GraphicsPath{void AddString(const wchar_t*,int,FontFamily*,int,float,PointF,StringFormat*){}};
struct Call{int kind;float x,y,w,h;ARGB color;};
struct Graphics{std::vector<Call> calls;Graphics(Bitmap*){}void Clear(Color){}void SetSmoothingMode(int){}
 void DrawRectangle(Pen*p,float x,float y,float w,float h){calls.push_back({1,x,y,w,h,p->color.v});}
 void DrawPolygon(Pen*p,PointF*points,int n){assert(n==4);calls.push_back({2,points[0].X,points[0].Y,0,0,p->color.v});}
 void DrawImage(Bitmap*,float x,float y,float w,float h){calls.push_back({3,x,y,w,h,0});}
 void DrawPath(Pen*,GraphicsPath*){}void FillPath(SolidBrush*,GraphicsPath*){}
};
struct EnemyRaster{std::wstring text;ARGB color{};std::unique_ptr<Bitmap>image;};
static std::unordered_map<uint64_t,EnemyRaster>enemyRasters;
static uint64_t enemyRasterKey(const external::enemies::Marker&m){return(uint64_t(m.type)<<32)|m.owner;}
'''
suffix=r'''
int main(){
 external::enemies::Marker m{};m.id=1;m.owner=0x680000;m.type=0x650000;m.player=1;m.kind=3;m.color=0xFFDC5046;
 const char16_t name[]=u"犀牛坦克";for(unsigned i=0;i<sizeof(name)/sizeof(name[0]);++i)m.name[i]=name[i];
 visual::LabelView v{};v.id=1;v.x=100.25f;v.y=80.5f;Graphics g(nullptr);
 drawEnemyMarker(g,m,v,2.f,1.5f);assert(allocations==1&&g.calls.size()==3&&g.calls[0].kind==1&&g.calls[1].color==m.color);
 assert(g.calls[0].x==193.5f&&g.calls[0].y==113.75f&&g.calls[2].x==208.5f&&g.calls[2].y==124.75f);
 assert(enemyRasters.begin()->second.text==L"犀牛坦克");
 drawEnemyMarker(g,m,v,2.f,1.5f);m.id=2;v.id=2;drawEnemyMarker(g,m,v,1,1);assert(allocations==1); // shared type/owner raster, not one bitmap per unit
 m.kind=2;g.calls.clear();drawEnemyMarker(g,m,v,1,1);assert(g.calls[0].kind==2&&g.calls[1].color==m.color);
 m.owner+=0x10000;m.player=2;drawEnemyMarker(g,m,v,1,1);assert(allocations==2&&enemyRasters.size()==2);
 for(auto&ch:m.name)ch=u'长';drawEnemyMarker(g,m,v,1,1);auto&cached=enemyRasters[enemyRasterKey(m)];assert(cached.text.size()==18&&cached.image->GetWidth()<=280);
 for(auto&ch:m.name)ch=0;m.name[0]=10;drawEnemyMarker(g,m,v,1,1);assert(enemyRasters[enemyRasterKey(m)].text==L"单位");
}
'''
with tempfile.TemporaryDirectory(prefix='enemy-draw-') as temp:
    cpp=Path(temp)/'draw.cpp';out=Path(temp)/'draw';cpp.write_text(prefix+draw+suffix)
    subprocess.run(['g++','-O2','-std=c++17','-I'+str(R/'assist/source'),str(cpp),'-o',str(out)],check=True)
    subprocess.run([str(out)],check=True)
assert 'externalSampler.sample(displayNow,flags(),extra.enemyMarkers)' in main
assert 'enemyMotion.update(enemySample,f.frame)' in main and 'enemyMotion.render(renderNow,enemyViews)' in main
assert 'extra.enemyMarkers' not in main.split('static uint32_t flags(){',1)[1].split('\n',1)[0]
io=(R/'assist/source/extra_settings_io.h').read_text()
assert 'L"EnemyMarkers",0' in io and 'write(L"Display",L"EnemyMarkers",extra.enemyMarkers)' in io
assert 'enemyMarkers' not in io.split('static uint32_t additionalFlags(){',1)[1].split('\n',1)[0]
assert '!snap.crateCount&&!enemyCount' in main
assert 'memcmp(drawnEnemyViews,enemyViews' in main and 'memcmp(drawnEnemies,enemyFrame.items' in main
assert 'g.SetClip(RectF(0,0,enemyFrame.width*sx,enemyFrame.height*sy)' in main and 'g.Restore(enemyClip)' in main
assert 'if(!foregroundGame()||snap.renderStatus!=1){ShowWindow(overlay,SW_HIDE);' in main
sampler=(R/'external_reader/source/tactical_sampler.h').read_text()
assert 'enemies::latest(read_,at,out,n,0)' in sampler
assert 'reader->readFresh(p,out,n)' in (R/'external_reader/source/shared_reader.h').read_text()
for text in (sampler,draw,(R/'external_reader/source/enemy_markers.h').read_text()):
    assert not any(word in text for word in ('WriteProcessMemory','VirtualProtect','SetTimer','nativeObjectEvent','submitConstructed','RevealMap','577D90'))
assert 'EnemyMarker' not in (R/'assist/source/protocol.h').read_text()
print('PASS: production enemy drawing helper shapes/coordinates/owner colors/text/cache; common sample/Motion/paint clock; viewport clip and invalidation; default-off saved setting; read-only host data, no DLL protocol/command/reveal path')
