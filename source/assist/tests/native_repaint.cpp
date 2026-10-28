#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <climits>
using namespace external;
using Canvas=camera::NativeCanvas;
static voxel::Image sprite(int w,int h,uint32_t color,float z){
 voxel::Image image;image.width=w;image.height=h;image.pixels.assign(size_t(w)*h,color);image.depth.assign(image.pixels.size(),z);return image;
}
static void scene(Canvas&canvas,int ore){
 lighting::State light;light.intensity=1000;light.tint={{1000,1000,1000}};
 canvas.underlayRect(0,0,768,384,0x203040);
 auto ground=sprite(180,120,0xff42683a,0);ground.depth[17]=std::numeric_limits<float>::quiet_NaN();canvas.blit(ground,95,110,1,1,30,light,0);
 canvas.diamond(280,190,1,1,80,0xff608030,light);
 auto building=sprite(100,92,0xff986544,12);building.x=-50;building.y=-80;
 auto shadow=sprite(100,40,0xa0000000,0);shadow.x=-50;shadow.y=-15;shadow.shadow=true;shadow.flat=true;shadow.depthBias=1;shadow.writeDepth=true;
 building.attachments.push_back(shadow);canvas.blit(building,225,205,1,1,60,light,2);
 if(ore>=0){auto resource=sprite(18,22,ore?0xffe0ba30:0xffb09020,2);resource.x=-9;resource.y=-11;canvas.blit(resource,167,170,1,1,90,light,0);}
 auto foreground=sprite(90,65,0x80603018,15);foreground.x=-20;foreground.y=-15;foreground.writeDepth=false;canvas.blit(foreground,235,190,1,1,105,light,1);
 auto glow=sprite(5,5,0xffd04090,0);glow.depth.assign(25,std::numeric_limits<float>::quiet_NaN());canvas.blit(glow,210,160,1,1,0,light);
}
static std::vector<uint32_t> colors(const Canvas&canvas){std::vector<uint32_t> out;assert(canvas.presentRegion(768,384,out,0,0,768,384));return out;}
static std::vector<float> depths(const Canvas&canvas){std::vector<float> out;assert(canvas.depthRegion(0,0,768,384,out));return out;}
static bool equalDepth(const std::vector<float>&a,const std::vector<float>&b){return a.size()==b.size()&&!std::memcmp(a.data(),b.data(),a.size()*sizeof(float));}
int main(){
 auto budget=std::make_shared<Canvas::Budget>();budget->maximum=64;
 Canvas original;assert(original.reset(768,384,budget));original.precision(colorprecision::Mode::SmoothRGB);scene(original,0);original.clean();
 const auto oldColors=colors(original);const auto oldDepths=depths(original);const auto oldPages=original.pageCount();assert(oldPages==18);
 // Capture immutable raw neighborhoods before the page repaint. The changed
 // page may invalidate neighboring filter halos, never far-away owners.
 std::vector<vulkancompute::TileJob> oldTiles;assert(original.gpuRegion(1536,768,0,0,768,384,3,oldTiles));assert(oldTiles.size()==6);
 for(int ore:{1,-1}){
  Canvas changed;assert(changed.repaint(original,{{158,159,176,181}}));assert(changed.dirtyPages()==1);
  assert(changed.intersectsRepaint(128,128,256,256)&&!changed.intersectsRepaint(256,128,384,256));
  assert(!changed.intersectsRepaint(NAN,0,1,1)&&!changed.intersectsRepaint(0,0,128,128));
  scene(changed,ore);assert(!changed.failed());
  Canvas expected;assert(expected.reset(768,384));expected.precision(colorprecision::Mode::SmoothRGB);scene(expected,ore);
  const auto actual=colors(changed);assert(actual==colors(expected)&&equalDepth(depths(changed),depths(expected)));
  assert(actual!=oldColors&&colors(original)==oldColors&&equalDepth(depths(original),oldDepths));
  assert(changed.pageCount()==oldPages&&budget->pages==oldPages+1); // unchanged pages shared, one replacement
  auto dirtyPixels=oldColors;assert(changed.presentDirty(768,384,dirtyPixels)&&dirtyPixels==actual);
  std::vector<vulkancompute::TileJob> nextTiles;assert(changed.gpuRegion(1536,768,0,0,768,384,3,nextTiles));
  size_t retained=0,replaced=0;for(const auto&old:oldTiles){auto at=std::find_if(nextTiles.begin(),nextTiles.end(),[&](const auto&t){return t.left==old.left&&t.top==old.top;});assert(at!=nextTiles.end());if(at->pixels==old.pixels)++retained;else ++replaced;}
  assert(retained==2&&replaced==4); // full128px page + xBRZ3px halo, not whole source cache
  std::vector<vulkancompute::TileJob> sourceAgain;assert(original.gpuRegion(1536,768,0,0,768,384,3,sourceAgain));
  for(const auto&old:oldTiles){auto at=std::find_if(sourceAgain.begin(),sourceAgain.end(),[&](const auto&t){return t.left==old.left&&t.top==old.top;});assert(at!=sourceAgain.end()&&at->pixels==old.pixels);}
  // Neither fork nor a committed/clean bank retains the one-page replay gate.
  auto fork=changed.fork();assert(fork.intersectsRepaint(500,0,510,10));auto icon=sprite(4,4,0xffb020e0,0);lighting::State neutral;
  fork.blit(icon,500,10,1,1,200,neutral);assert(fork.pixel(500,10)!=changed.pixel(500,10));
  changed.clean();assert(!changed.dirtyPages()&&changed.intersectsRepaint(500,0,510,10));
 }
 assert(budget->pages==oldPages);
 // A removed final sprite can leave a page empty: its erase still counts as
 // dirty output, even when no subsequent source calls allocate the page.
 Canvas isolated;assert(isolated.reset(256,128));auto pixel=sprite(1,1,0xffffffff,10);lighting::State neutral;isolated.blit(pixel,5,5,1,1,1,neutral);
 std::vector<uint32_t> erased;assert(isolated.present(256,128,erased));Canvas empty;assert(empty.repaint(isolated,{{5,5,6,6}}));assert(empty.pageCount()==0&&empty.dirtyPages()==1);
 assert(empty.presentDirty(256,128,erased)&&erased[5*256+5]==0xff080c08u);
 // restrictPaint is page aligned output coverage, not a source admission
 // rule. A source straddling its boundary can still paint its inside portion.
 Canvas local;assert(local.reset(768,384));local.restrictPaint(130,130,255,255);local.underlayRect(0,0,768,384,0x112233);
 auto wide=sprite(300,300,0xffb04020,0);local.blit(wide,50,50,1,1,10,neutral);assert(local.pageCount()==1);
 assert(local.pixel(128,128)==0xffb04020&&local.pixel(255,255)==0xffb04020&&local.pixel(127,128)==0xff080c08u&&local.pixel(256,128)==0xff080c08u);
 local.clean();assert(local.intersectsRepaint(200,200,201,201)&&!local.intersectsRepaint(0,0,10,10));
 auto localFork=local.fork();localFork.underlayRect(0,0,128,128,0xff00ff);assert(localFork.pageCount()==1);
 assert(local.reset(768,384));local.underlayRect(0,0,128,128,0xff00ff);assert(local.pageCount()==1&&local.pixel(1,1)==0xffff00ffu);
 // Preflight reserves replacement pages while the immutable source is still
 // retained. Failure leaves the destination and source unchanged.
 auto small=std::make_shared<Canvas::Budget>();small->maximum=4;Canvas full;assert(full.reset(384,128,small));full.underlayRect(0,0,384,128,0x334455);assert(small->pages==3);
 Canvas denied;assert(!denied.repaint(full,{{0,0,256,128}})&&denied.width()==0&&small->pages==3);
 {Canvas one;assert(one.repaint(full,{{0,0,1,1}}));one.underlayRect(0,0,384,128,0x445566);assert(!one.failed()&&small->pages==4&&one.pixel(0,0)==0xff445566u&&one.pixel(129,0)==0xff334455u);}
 assert(small->pages==3);
 Canvas huge;assert(huge.reset(4096,4096));assert(!denied.repaint(huge,{{INT_MIN,INT_MIN,INT_MAX,INT_MAX}})&&denied.width()==0);
 assert(!original.repaint(original,{{0,0,1,1}}));
 std::puts("PASS: sparse native page repaint equals full color/depth/shadow/underlay reconstruction; source and far raw-cache owners unchanged, deletion dirtiness, page-aligned coverage, fork/clean recovery and bounded failure");
}
