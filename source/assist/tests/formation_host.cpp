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
struct Color {Color(int,int,int,int){}};struct Bitmap {Bitmap(int,int,int,int,BYTE*){}};struct Graphics {void DrawImage(Bitmap*,int,int,int,int){}};
static void drawText(Graphics&,const std::wstring&,int,int,int,Color){}
static struct {bool formationEnabled=true;uint32_t formationHold=18,formationKeys[4]={'Z','X','C','V'};}extra;
static struct {int chat=0,cameraX=0,cameraY=0,width=800,height=600;}snap;
static struct State {formation::Preview formationPreview;LONG formationSeq=0,formationKey=0,formationKeyAck=0;uint32_t formationKeyCode=0;}state,*shared=&state;
static std::map<uint32_t,uint32_t> memory;static int ownerReads=0;static bool ownershipRace=false;
static struct {bool readFresh(uint32_t p,void*out,size_t n){if(ownershipRace&&p==0x1021c&&n==4){uint32_t owner=++ownerReads==1?2:3;memcpy(out,&owner,4);return true;}auto it=memory.find(p);if(n!=4||it==memory.end())return false;memcpy(out,&it->second,4);return true;}}externalReader;
#include "../source/type_hotkey.h"
#include "../../external_reader/source/voxel_preview.h"
namespace gameassets {static void update(){}static bool actor(uint32_t,uint32_t,uint32_t,double,external::voxel::Image&){return false;}}
#include "../source/formation_host.h"
int main(){using namespace formationhost;
 auto step=[](){clockValue+=40;update();};
 auto ready=[](uint32_t type){auto&c=models[{type,2}];c.ready=true;c.used=type;c.sprite.width=c.sprite.height=1;c.sprite.pixels={0x7c123456};};
 step();for(uint32_t t=1;t<=64;++t)ready(t);auto&v=shared->formationPreview;v.active=1;v.match=1;v.count=1;v.units[0]={99,0x10000,64};v.slots[0]={256,256,0,1};memory[0x10010]=99;memory[0x1021c]=2;
 step();assert(visuals.size()==1&&models.size()==64);auto image=visuals[0].image;auto before=paintRevision;
 shared->formationSeq=1;step();assert(visuals.size()==1&&visuals[0].image==image&&paintRevision==before);shared->formationSeq=2;
 // Redraw depends on content revision even when the allocator reuses addresses.
 ++revision;step();assert(paintRevision>before);
 v.units[0].type=65;step();assert(models.size()==64&&models.count({65,2})&&!models.count({1,2})&&missing==1);
 ownershipRace=true;ownerReads=0;step();assert(needed({65,3})&&models.count({65,3}));ownershipRace=false;
 // Full budgets evict unused images/models, never currently published pointers.
 models.clear();wanted={{1,2}};wantedFacing=5;ready(1);ready(2);auto&keep=models[{1,2}];keep.isVoxel=true;keep.views[5].pixels.resize(1024);keep.viewUsed[5]=1;
 auto&old=models[{2,2}];old.views[3].pixels.resize(4*1024*1024);old.viewUsed[3]=0;assert(roomForImage(1024)&&keep.views.count(5)&&!old.views.count(3));
 wanted={{1,2}};models.erase({2,2});keep.sprite.pixels.resize(4*1024*1024);assert(!roomForImage(4*1024*1024)&&models.count({1,2}));
 v.active=0;step();assert(visuals.empty());reset();assert(models.empty());
 std::cout<<"PASS preview seqlock continuity, content revisions, ownership-race protection, full model-cache eviction, image-budget eviction and active-image protection\n";
}
