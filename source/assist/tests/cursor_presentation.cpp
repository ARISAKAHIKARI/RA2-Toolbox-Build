#include "../../cursor/source/cursor_presentation.h"
#include <cstring>
#include <cwchar>
#include <iostream>
#include <limits>

using namespace cursorpresentation;
struct Checks {
 int scenarios=0,checks=0,failed=0;
 void group(){++scenarios;}
 void expect(bool ok,const char*name){++checks;if(!ok){++failed;std::cerr<<"FAIL: "<<name<<'\n';}}
 void point(const Result&r,int x,int y,const char*name){expect(r.point.x==x&&r.point.y==y,name);}
 void identity(const Input&i,const Result&r,const char*name){expect(!r.mapped&&r.point.x==i.point.x&&r.point.y==i.point.y,name);}
};
static Input fullscreen(){
 Input i;i.point={1280,720};i.client=i.monitor={0,0,3840,2160};
 i.clip={0,0,2560,1440};i.physical=i.clipValid=true;i.gameW=2560;i.gameH=1440;return i;
}
int main(){
 Checks t;
 {t.group();auto i=fullscreen();auto r=resolve(i);t.expect(r.mapped,"compat mapping selected");t.point(r,1920,1080,"2K canvas center to 4K center");t.expect(sameRect(r.input,i.clip)&&sameRect(r.output,i.client),"source and target evidence retained");t.expect(!std::wcscmp(r.route,L"automatic-canvas-clip-compat"),"automatic route calls out inference");}
 {t.group();auto i=fullscreen();i.clip=i.client;auto r=resolve(i);t.identity(i,r,"ordinary physical point is not scaled twice");}
 {t.group();auto i=fullscreen();i.client=i.monitor=i.clip={0,0,2560,1440};auto r=resolve(i);t.identity(i,r,"actual 2K desktop remains 2K");}
 {t.group();auto i=fullscreen();i.client=i.monitor={-3840,-2160,0,0};i.clip={-3840,-2160,-1280,-720};i.point={-2560,-1440};auto r=resolve(i);t.expect(r.mapped,"negative monitor mapping selected");t.point(r,-1920,-1080,"negative origins preserved");}
 {t.group();auto i=fullscreen();i.clipValid=false;auto r=resolve(i);t.identity(i,r,"failed clip read keeps physical point");}
 {t.group();auto i=fullscreen();i.clip.right=i.clip.left;auto r=resolve(i);t.identity(i,r,"empty clip rejected");}
 {t.group();auto i=fullscreen();i.gameW=1920;i.gameH=1080;auto r=resolve(i);t.identity(i,r,"stale dimensions do not match current clip");}
 {t.group();auto i=fullscreen();i.client={100,100,3940,2260};i.clip={100,100,2660,1540};auto r=resolve(i);t.identity(i,r,"large windowed client is not auto mapped");}
 {t.group();auto i=fullscreen();i.clip.right=2559;auto r=resolve(i);t.identity(i,r,"partial clip dimensions rejected");}
 {t.group();auto i=fullscreen();i.clip={100,100,2660,1540};auto r=resolve(i);t.identity(i,r,"same-size partial clip with different origin rejected");}
 {t.group();auto i=fullscreen();i.physical=false;auto r=resolve(i);t.identity(i,r,"logical fallback never enters pixel canvas mapping");i.mode=CanvasFullscreen;t.identity(i,resolve(i),"explicit mapping also requires physical domain");}
 {t.group();auto i=fullscreen();i.overrideW=1920;i.overrideH=1080;auto r=resolve(i);t.expect(r.mapped,"auto ignores manual source overrides");t.point(r,1920,1080,"auto uses actual game size");i.gameW=i.gameH=0;t.identity(i,resolve(i),"auto rejects missing actual size even with override");}
 {t.group();auto i=fullscreen();i.point={2560,720};t.identity(i,resolve(i),"right clip edge is outside input");i.point={-1,720};t.identity(i,resolve(i),"outside point is not clamped");i.point={1280,1440};t.identity(i,resolve(i),"bottom clip edge is outside input");}
 {t.group();auto i=fullscreen();i.client=i.monitor={0,0,3840,2400};auto r=resolve(i);t.identity(i,r,"automatic anisotropic stretch rejected");i.mode=CanvasFullscreen;r=resolve(i);t.expect(r.mapped,"explicit nonuniform stretch accepted");t.point(r,1920,1200,"explicit stretch maps each axis");}
 {t.group();auto i=fullscreen();i.client=i.monitor={0,0,3840,2400};i.aspectFit=true;i.clip={0,120,2560,1560};i.point={1280,840};auto r=resolve(i);t.expect(r.mapped,"known fit viewport origin accepted");t.point(r,1920,1200,"letterbox center correctly translated");t.expect(sameRect(r.output,{0,120,3840,2280}),"letterbox target excludes bars");i.clip={0,0,2560,1440};i.point={1280,720};t.identity(i,resolve(i),"auto rejects clip outside known fit origin");i.mode=CanvasFullscreen;r=resolve(i);t.point(r,1920,1200,"explicit fit may select independent source origin");}
 {t.group();auto i=fullscreen();i.mode=CanvasFullscreen;i.client={100,60,2660,1500};i.clipValid=false;i.point={1380,780};auto r=resolve(i);t.expect(r.mapped,"explicit uses client source origin without clip");t.point(r,1920,1080,"explicit source origin translated to monitor viewport");t.expect(sameRect(r.input,{100,60,2660,1500}),"explicit source canvas recorded");}
 {t.group();auto i=fullscreen();i.mode=CanvasFullscreen;i.clip={400,300,2960,1740};i.point={1680,1020};auto r=resolve(i);t.point(r,1920,1080,"matching clip preferred over client origin");}
 {t.group();auto i=fullscreen();i.mode=CanvasFullscreen;i.gameW=i.gameH=0;i.overrideW=1920;i.overrideH=1080;i.point={960,540};auto r=resolve(i);t.expect(r.mapped,"explicit paired override supplies source extent");t.point(r,1920,1080,"explicit override is applied only once");i.overrideH=0;t.identity(i,resolve(i),"partial override rejected");i.overrideH=20000;t.identity(i,resolve(i),"invalid paired override rejected");}
 {t.group();auto i=fullscreen();i.mode=CanvasFullscreen;i.client=i.monitor={0,0,1280,720};i.point={2559,1439};auto r=resolve(i);t.point(r,1279,719,"downscaled last source pixel remains inside target");t.expect(r.output.contains(r.point),"downscale edge never rounds outside viewport");}
 {t.group();auto i=fullscreen();i.mode=Screen;i.physical=false;i.clipValid=false;auto r=resolve(i);t.identity(i,r,"screen mode explicitly keeps physical source unchanged");i.mode=99;t.identity(i,resolve(i),"unknown mode does not guess");}
 {t.group();auto i=fullscreen();i.monitor={};t.identity(i,resolve(i),"missing monitor rejects auto");i.mode=CanvasFullscreen;t.identity(i,resolve(i),"missing monitor rejects explicit");}
 {t.group();auto i=fullscreen();i.client.right-=1;t.identity(i,resolve(i),"almost fullscreen stays conservative");}
 {t.group();auto i=fullscreen();auto r=resolve(i);auto p=cursorcoordinates::spriteOrigin(r.point,12,18);t.expect(p.x==1908&&p.y==1062,"hotspot subtracted after presentation mapping");t.point(r,1920,1080,"mapping itself is independent of hotspot");}
 {t.group();auto i=fullscreen();i.mode=CanvasFullscreen;i.clipValid=false;i.client={std::numeric_limits<int>::max()-100,0,std::numeric_limits<int>::max(),100};i.point={i.client.left,0};t.identity(i,resolve(i),"source rectangle overflow rejected");}
 {t.group();auto i=fullscreen();i.client=i.monitor={0,0,3840,2160};i.gameW=320;i.gameH=200;i.clip={0,0,320,200};i.point={160,100};t.identity(i,resolve(i),"canvas with different aspect ratio needs explicit choice");}
 {t.group();auto i=fullscreen();i.mode=CanvasFullscreen;i.client=i.monitor=i.clip={0,0,2560,1440};auto r=resolve(i);t.identity(i,r,"explicit same-size identity stays unmarked");t.expect(!std::wcscmp(r.route,L"explicit-canvas-identity"),"explicit identity has distinct route");}
 std::cout<<"{\"scenarios\":"<<t.scenarios<<",\"checks\":"<<t.checks<<",\"failed\":"<<t.failed<<"}\n";
 return t.failed?1:0;
}
