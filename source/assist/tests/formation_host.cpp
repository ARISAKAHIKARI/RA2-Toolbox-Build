#include "../source/formation.h"
#include "../../external_reader/source/projection.h"
#include <string>
#include <map>
#include <cassert>
#include <iostream>
#include <cstring>
using namespace assist;
using DWORD=uint32_t;using WORD=uint16_t;using BYTE=uint8_t;using LONG=int32_t;
constexpr int VK_MENU=18,VK_CONTROL=17,VK_SHIFT=16,PixelFormat32bppPARGB=1;
static uint32_t clockValue=1000,pid=7,paintRevision=0;
static uint32_t GetTickCount(){return clockValue;}static uint32_t commandMatch(){return 1;}static bool foregroundGame(){return true;}
static void MemoryBarrier(){}static void InterlockedIncrement(volatile LONG*p){++*p;}static void wakeGame(){}
struct ColorMatrix {float m[5][5]{};};static ColorMatrix drawnMatrix;struct ImageAttributes {void SetColorMatrix(ColorMatrix*m){drawnMatrix=*m;}};struct RectF {RectF(float,float,float,float){}};constexpr int UnitPixel=2;
static uint32_t drawnPixel=0;struct Color {Color(int,int,int,int){}};struct Bitmap {BYTE*data;Bitmap(int,int,int,int,BYTE*p):data(p){}};struct Graphics {void DrawImage(Bitmap*b,RectF,float,float,float,float,int,ImageAttributes*){memcpy(&drawnPixel,b->data,4);}};
static void drawText(Graphics&,const std::wstring&,int,int,int,Color){}
static struct {bool formationEnabled=true;uint32_t formationHold=18,formationMouse=2,formationKeys[6]={'Z','X','C','V','B','N'};}extra;
static struct {int chat=0,cameraX=0,cameraY=0,width=800,height=600;}snap;
static struct State {formation::Preview formationPreview;LONG formationSeq=0,formationKey=0,formationKeyAck=0;uint32_t formationKeyCode=0;}state,*shared=&state;
static std::map<uint32_t,uint32_t> memory;static int ownerReads=0;static bool ownershipRace=false;
static struct {bool readFresh(uint32_t p,void*out,size_t n){if(ownershipRace&&p==0x1021c&&n==4){uint32_t owner=++ownerReads==1?2:3;memcpy(out,&owner,4);return true;}for(size_t i=0;i<n;++i){auto v=memory.upper_bound(p+i);if(v==memory.begin())return false;--v;if(p+i-v->first>=4)return false;static_cast<uint8_t*>(out)[i]=uint8_t(v->second>>((p+i-v->first)*8));}return true;}}externalReader;
#include "../source/type_hotkey.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/visual_pose.h"
namespace gameassets {static uint64_t revision(){return 1;}static int captures=0,renders=0;struct ActorArt {external::voxel::Model model;bool voxel=false;};static void update(){}static bool captureActor(uint32_t,uint32_t,uint32_t id,ActorArt&){if(id!=100)return false;++captures;return true;}static bool renderActor(const ActorArt&,const external::visual::Pose&pose,external::voxel::Image&image){++renders;image.width=image.height=1;image.pixels={uint32_t(0xff804000|pose.body)};return true;}}
#include "../source/formation_host.h"
int main(){using namespace formationhost;
 auto step=[](){clockValue+=40;update();};
 auto ready=[](uint32_t type){auto&c=models[{type,2,-1}];c.ready=true;c.used=type;c.views[0].width=c.views[0].height=1;c.views[0].pixels={0x7c123456};};
 step();assetRevision=gameassets::revision();for(uint32_t t=1;t<=formation::Maximum;++t)ready(t);auto&v=shared->formationPreview;v.active=1;v.match=1;v.count=1;v.units[0]={99,0x10000,formation::Maximum};v.slots[0]={256,256,0,1};memory[0x10010]=99;memory[0x1021c]=2;
 step();assert(visuals.size()==1&&models.size()==formation::Maximum);auto image=visuals[0].image;auto before=paintRevision;
 shared->formationSeq=1;step();assert(visuals.size()==1&&visuals[0].image==image&&paintRevision==before);shared->formationSeq=2;
 // Redraw depends on content revision even when the allocator reuses addresses.
 ++revision;step();assert(paintRevision>before);
 v.units[0].type=formation::Maximum+1;step();assert(models.size()==formation::Maximum&&models.count({formation::Maximum+1,2,-1})&&!models.count({1,2,-1})&&missing==1);
 ownershipRace=true;ownerReads=0;step();assert(needed({formation::Maximum+1,3,-1})&&models.count({formation::Maximum+1,3,-1}));ownershipRace=false;
 // Full budgets evict unused images/models, never currently published pointers.
 models.clear();wanted={{1,2,-1}};wantedFacing=5;ready(1);ready(2);auto&keep=models[{1,2,-1}];keep.art.voxel=true;keep.views[5].pixels.resize(1024);keep.viewUsed[5]=1;
 auto&old=models[{2,2,-1}];old.views[3].pixels.resize(4*1024*1024);old.viewUsed[3]=0;assert(roomForImage(1024)&&keep.views.count(5)&&!old.views.count(3));
 wanted={{1,2,-1}};models.erase({2,2,-1});keep.views[5].pixels.resize(4*1024*1024);assert(!roomForImage(4*1024*1024)&&models.count({1,2,-1}));
 // Formation and observation use the same source capture/render API. A turn
 // selects a new pose while retaining source art and premultiplied alpha.
 reset();wanted.clear();v.units[0]={100,0x10000,7};memory[0x10010]=100;v.facing=0;step();assert(visuals.size()==1&&gameassets::captures==1&&gameassets::renders==1);assert(visuals[0].image->pixels[0]==0xff804008);
 v.facing=formation::Pi/2;step();assert(visuals.size()==1&&gameassets::captures==1&&gameassets::renders==2);assert(visuals[0].image->pixels[0]==0xff804010);v.facing=0;step();assert(gameassets::renders==2);
 // A new preview point is visible in the next 2ms overlay tick, without
 // the old independent 33ms gate, even when the pose/art stay cached.
 int oldX=visuals[0].x;v.slots[0].x+=256;clockValue+=2;update();assert(visuals[0].x==oldX+30);v.slots[0].x-=256;clockValue+=2;update();assert(visuals[0].x==oldX);
 // Stationary preview still repaints a lighting change without rerendering poses.
 before=paintRevision;int rendered=gameassets::renders;memory[0xA8B230]=0xC00000;memory[0xC0352C]=75;for(int off:{0x3534,0x3538,0x353C})memory[0xC00000+off]=100;step();assert(paintRevision>before&&gameassets::renders==rendered);Graphics g;draw(g);assert((drawnPixel>>24)==PreviewOpacity&&drawnPixel!=visuals[0].image->pixels[0]);auto expected=[&](){std::vector<uint32_t> p;external::camera::previewPixels(*visuals[0].image,visuals[0].light,PreviewOpacity,p);return p[0];};assert(drawnPixel==expected());
 // Use live projection scale and the destination cell, not the source unit.
 double zScale=.144337567;uint32_t words[2];memcpy(words,&zScale,8);memory[0xB0CD48]=words[0];memory[0xB0CD4C]=words[1];
 memory[0x87F7E8+0x13C]=0x200000;memory[0x87F7E8+0x140]=512*512;memory[0x200000+(512+1)*4]=0x300000;
 memory[0x300024]=0x00010001;memory[0x300034]=0;memory[0x300104]=65536;memory[0x300108]=1100u<<16;memory[0x30010C]=900|(1350u<<16);memory[0x300140]=0;
 v.slots[0].z=104;step();assert(visuals[0].y==15&&visuals[0].light.intensity==1100);draw(g);assert(drawnPixel==expected()&&gameassets::renders==rendered);
 before=paintRevision;memory[0x300108]=1250u<<16;step();draw(g);assert(paintRevision>before&&drawnPixel==expected()&&gameassets::renders==rendered);
 // A bridge destination uses its deck light and live projection height.
 memory[0x300140]=0x80;memory[0x300118]=2u<<24;memory[0x89E7C0]=104;v.slots[0].z=624;step();draw(g);assert(visuals[0].y==30-external::screenHeight(624,zScale)&&visuals[0].light.intensity==1350&&drawnPixel==expected());
 v.slots[0].z=208;step();draw(g);assert(visuals[0].light.intensity==1250&&drawnPixel==expected());
 v.active=0;step();assert(visuals.empty());reset();assert(models.empty());
 std::cout<<"PASS preview seqlock/cache ownership, presentation opacity, common SHP/VXL lighting, live destination projection and separate bridge/ground light\n";
}
