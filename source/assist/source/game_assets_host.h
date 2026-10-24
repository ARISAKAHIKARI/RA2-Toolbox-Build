#include "../../external_reader/source/camera_assets.h"
namespace gameassets {
using Context=external::cameraassets::Context<external::ReadSession>;
using ActorArt=Context::ActorArt;
static Context context(externalReader);
static auto&files=context.files;static auto&failure=context.failure;
static void update(){context.identity(pid,commandMatch());}
static uint64_t revision(){return context.revision();}
static bool cameo(const char*n,std::vector<uint8_t>&b){update();return context.cameo(n,b);}
static void resolveCameo(um::Type&t){update();context.resolveCameo(t);}
static std::string text(uint32_t p,unsigned n){return context.text(p,n);}
static bool palette(bool iso,std::array<uint32_t,256>&p){return context.palette(iso,p);}
static int buildingFrame(uint32_t p,uint32_t t){return context.buildingFrame(p,t);}
static bool overlay(uint32_t t,uint32_t p,int f,external::voxel::Image&i,bool terrain=false){return context.overlay(t,p,f,i,terrain);}
static bool sprite(uint32_t t,uint32_t p,const std::array<uint32_t,256>&c,int f,external::voxel::Image&i){return context.sprite(t,p,c,f,i);}
static bool tile(uint32_t t,unsigned s,const std::array<uint32_t,256>&p,external::voxel::Image&i){return context.tile(t,s,p,i);}
static bool actor(uint32_t p,uint32_t t,uint32_t id,double a,external::voxel::Image&i){return context.actor(p,t,id,a,i);}
static bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,ActorArt&a){return context.captureAnimation(p,t,id,a);}
static bool captureActor(uint32_t p,uint32_t t,uint32_t id,ActorArt&a){return context.captureActor(p,t,id,a);}
static size_t artBytes(const ActorArt&a){return Context::cost(a);}
static bool renderActor(const ActorArt&a,const external::visual::Pose&p,external::voxel::Image&i){return context.renderActor(a,p,i);}
}
