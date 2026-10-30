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
#include "../source/formation_engine.h"
struct Backend {
 int sharedRenders=0;
 using ActorArt=gameassets::ActorArt;void identity(uint32_t,uint32_t){}uint64_t revision(){return 1;}
 bool captureActor(uint32_t p,uint32_t t,uint32_t id,ActorArt&a){return gameassets::captureActor(p,t,id,a);}
 bool renderActor(const ActorArt&a,const external::visual::Pose&p,external::voxel::Image&i){return gameassets::renderActor(a,p,i);}
 bool renderSharedActor(const ActorArt&a,const external::visual::Pose&p,std::shared_ptr<const external::voxel::Image>&out){++sharedRenders;external::voxel::Image image;if(!renderActor(a,p,image))return false;out=std::make_shared<const external::voxel::Image>(std::move(image));return true;}
} backend;
int main(){
 formationengine::Renderer<decltype(externalReader),Backend> engine(externalReader,backend);engine.process=7;engine.epoch=1;
 auto&v=state.formationPreview;v.active=1;v.match=1;v.count=1;v.units[0]={100,0x10000,7};v.slots[0]={256,256,0,1};memory[0x10010]=100;memory[0x1021c]=2;
 auto step=[&]{clockValue+=2;external::Projection p{0,0,800,600};p.zScale=external::projectionZScale(engine.read);engine.update(v,p);};
 step();assert(engine.visuals.size()==1&&gameassets::captures==1&&gameassets::renders==1&&backend.sharedRenders==1);auto published=engine.visuals[0].image;
 auto ghost=engine.compose(false,1);assert(ghost&&engine.compose(false,1)==ghost);bool visible=false;for(auto pixel:ghost->pixels){assert((pixel>>24)<=124);if(pixel>>24)visible=true;}assert(visible);
 v.facing=formation::Pi/2;step();assert(gameassets::captures==1&&gameassets::renders==2&&engine.visuals[0].image->pixels[0]==0xff804010);
 v.facing=0;step();assert(gameassets::renders==2&&engine.visuals[0].image==published);
 int oldX=engine.visuals[0].x;v.slots[0].x+=256;step();assert(engine.visuals[0].x==oldX+30);assert(engine.compose(false,1)!=ghost);v.slots[0].x-=256;
 memory[0xA8B230]=0xC00000;memory[0xC0352C]=75;for(int off:{0x3534,0x3538,0x353C})memory[0xC00000+off]=100;step();assert(gameassets::renders==2);
 std::vector<uint32_t> pixels;external::camera::previewPixels(*published,engine.visuals[0].light,124,pixels);assert(pixels[0]>>24==124&&pixels[0]!=published->pixels[0]);
 double zScale=.144337567;uint32_t words[2];memcpy(words,&zScale,8);memory[0xB0CD48]=words[0];memory[0xB0CD4C]=words[1];
 memory[0x87F7E8+0x13C]=0x200000;memory[0x87F7E8+0x140]=512*512;memory[0x200000+(512+1)*4]=0x300000;memory[0x300024]=0x00010001;memory[0x300034]=0;memory[0x300104]=65536;memory[0x300108]=1100u<<16;memory[0x30010C]=900|(1350u<<16);memory[0x300140]=0;
 v.slots[0].z=104;step();assert(engine.visuals[0].y==15&&engine.visuals[0].light.intensity==1100);
 memory[0x300140]=0x80;memory[0x300118]=2u<<24;memory[0x89E7C0]=104;v.slots[0].z=624;step();assert(engine.visuals[0].light.intensity==1350);
 // The same compositor must preserve intrinsic coverage on a transparent base.
 auto edge=std::make_shared<external::voxel::Image>();edge->width=2;edge->height=1;edge->pixels={0x80402010,0};edge->intrinsicAlpha=true;engine.visuals={{20,20,edge,{}}};++engine.paintRevision;
 auto partial=engine.compose(false,1);assert(partial);unsigned maximum=0;for(auto pixel:partial->pixels){maximum=std::max(maximum,pixel>>24);assert((pixel>>16&255)<=(pixel>>24)&&(pixel>>8&255)<=(pixel>>24)&&(pixel&255)<=(pixel>>24));}assert(maximum==62);
 // A quiet round may decode more than four unique models/headings.
 engine.reset();engine.decodeBudgetMs=10000;v.count=12;for(int n=0;n<v.count;++n){uint32_t object=0x40000+n*0x1000;v.units[n]={100,object,uint32_t(0x80000+n*0x1000)};v.slots[n]={256+n*32,256,0,1};memory[object+0x10]=100;memory[object+0x21C]=2;}step();assert(engine.visuals.size()==12&&engine.missing==0&&engine.models.size()==12);
 // A published frame owns its images even after the renderer drops every cache.
 engine.reset();assert(engine.models.empty()&&published->pixels[0]==0xff804008);
 std::cout<<"PASS shared preview material calls and compositor, warm composite reuse, intrinsic alpha/premultiplication, twelve new models in one round, cached headings, image ownership and live destination/bridge lighting\n";
}
