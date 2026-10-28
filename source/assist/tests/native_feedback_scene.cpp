#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../shared/source/overlay_group.h"
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Assets {
 struct ActorArt{};unsigned actorLoads=0;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xff204030);i.writeDepth=false;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int f,external::voxel::Image&i){i={};i.canvasWidth=i.width=60;i.canvasHeight=i.height=30;i.x=-30;i.y=-15;i.indices.assign(1800,f==15?2:127);return true;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){++actorLoads;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const external::visual::Pose&,external::voxel::Image&i){i.width=20;i.height=10;i.x=-10;i.y=-10;i.pixels.assign(200,0xffc0c080);i.writeDepth=false;return true;}
};
int main(){
 using namespace external;using namespace external::radar;
 auto read=[](uint32_t,void*,size_t){return false;};Assets assets;SceneRenderer<Assets> renderer;
 auto terrain=std::make_shared<Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};
 terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);terrain->colors[7*512+7]=0x204030;terrain->art[7*512+7].type=1;terrain->shade[7*512+7]={0,-1};
 Frame frame;frame.terrain=terrain;frame.tick=100;Marker m;m.object=0x30000;m.id=7;m.type=0x40000;m.owner=0x20000;m.x=m.y=7*256;m.kind=0;frame.markers.push_back(m);
 auto scene=renderer.render(frame,240,120,read,assets);CHECK(scene);int x=0,y=0,w=0,h=0;
 CHECK(renderer.feedbackBounds(m,x,y,w,h));CHECK(x==-10&&y==-10&&w==20&&h==10);
 const unsigned loads=assets.actorLoads;CHECK(renderer.feedbackBounds(m,x,y,w,h)&&assets.actorLoads==loads);
 const std::vector<uint8_t>*mask=nullptr;CHECK(renderer.feedbackVisibility(mask)&&mask&&mask->size()==240*120);
 mainview::View view;view.scenario=0x10000;view.house=0x20000;view.tactical=0x30000;view.frame=100;view.screenW=640;view.screenH=480;view.width=320;view.height=240;CHECK(view.valid());
 auto geometry=nativefeedback::objectGeometry(renderer,frame,view,640,480,.08660254);CHECK(geometry.size()==1&&geometry[0].id==7&&geometry[0].bounds.w==48);
 renderer.reset();CHECK(!renderer.feedbackBounds(m,x,y,w,h));CHECK(!renderer.feedbackVisibility(mask));
 Scene base;base.width=128;base.height=64;base.pixels.assign(128*64,0xff102030);view.width=128;view.height=64;view.cameraY=80;
 std::vector<nativefeedback::Bolt> bolts={{{768,768,0},{1536,768,0},42,false}};
 // Glow occupies fractional-alpha pixels while retaining an opaque core.
 nativefeedback::Canvas glow;CHECK(glow.reset(96,64));
 nativefeedback::bolt(glow,8,32,86,32,42,false,1.0);
 bool core=false,halo=false;
 for(uint32_t p:glow.pixels){unsigned a=p>>24;if(a==255)core=true;else if(a>0)halo=true;
  CHECK(((p>>16)&255)<=a&&((p>>8)&255)<=a&&(p&255)<=a);
 }
 CHECK(core&&halo);
 auto decorated=base;auto state=nativefeedback::effects(bolts,decorated,view,nullptr,.08660254);CHECK(state.complete&&state.bolts==1&&decorated.pixels!=base.pixels);
 std::vector<uint8_t>hidden(base.pixels.size(),2);decorated=base;state=nativefeedback::effects(bolts,decorated,view,&hidden,.08660254);CHECK(state.complete&&decorated.pixels==base.pixels);
 hidden.resize(3);decorated=base;state=nativefeedback::effects(bolts,decorated,view,&hidden,.08660254);CHECK(!state.complete&&decorated.pixels==base.pixels);
 decorated=base;CHECK(!nativefeedback::effects(bolts,decorated,view,nullptr,NAN).complete);
 using namespace overlaycomposition;
 Context context{1,100,200,300,8,8,0,0};auto pixels=std::make_shared<const std::vector<uint32_t>>(64,0xff102030);
 Surface background{3,110,context,{0,0,8,8},true,255,pixels};Surface selected{4,200,context,{0,0,8,8},true,255,std::make_shared<const std::vector<uint32_t>>(64,0xff40ff40)};
 Surface cursor{5,300,context,{2,2,1,1},false,255,std::make_shared<const std::vector<uint32_t>>(1,0xffffffff)};
 CHECK(groupValid({3,4,5},{background,selected,cursor},context));CHECK(groupValid({3,4,5},{},context));
 CHECK(!groupValid({3,4,5},{background,background},context));CHECK(!groupValid({3,4},{cursor},context));CHECK(!groupValid({3,3},{background},context));CHECK(!groupIds({}));CHECK(!groupIds({0,3}));
 auto stale=cursor;stale.context.token=2;CHECK(!groupValid({3,4,5},{background,stale},context));
 std::vector<uint32_t>out;CHECK(compose(context,{cursor,background,selected},out));CHECK(out[0]==0xff40ff40&&out[2*8+2]==0xffffffff);
 background.order=5;CHECK(compose(context,{cursor,selected,background},out));CHECK(out[0]==0xff40ff40&&out[2*8+2]==0xffffffff);
 std::printf("PASS: %u renderer/group checks; actual cached sprite bounds, visibility, no duplicate decode, effect masking, input layers on top\n",checks);
}
