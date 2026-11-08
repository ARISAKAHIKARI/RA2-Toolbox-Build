#include "../../cursor/source/playback.h"
#include <iostream>
#include <new>
#include <stdexcept>

// The bank and the acceptance function below come verbatim from main.cpp.
static std::map<unsigned,std::shared_ptr<playback::Timeline>> motionCache;
static playback::RasterCache rasterCache;
static std::shared_ptr<playback::Timeline> activeMotion;
static size_t motionBytes=0,lastPhase=0;
static int lastFrame=0;
#include PLAYBACK_PRODUCTION

static unsigned checks=0,cases=0;
static constexpr size_t Budget=512u*1024*1024;
static void check(bool value,const char*reason){++checks;if(!value)throw std::runtime_error(reason);}
static std::shared_ptr<playback::Timeline> timeline(uint32_t color,int width=1){
 motion::Timeline source;source.w=width;source.h=1;
 source.images={motion::Image(size_t(width),color),motion::Image(size_t(width),color^1u)};
 return std::make_shared<playback::Timeline>(playback::Timeline::pack(source));
}
static void reset(){
 motionCache.clear();activeMotion.reset();rasterCache.clear();motionBytes=0;lastFrame=7;lastPhase=3;
}
static void seed(){
 auto current=timeline(0xff123456u);motionCache[1]=current;activeMotion=current;motionBytes=current->bytes();
 check(rasterCache.get(*current->images[0],1,1)[0]==0xff123456u,"seed uses the actual raster cache");
 check(rasterCache.bytes()==4,"seed is resident before acceptance");
}
static PlaybackBank bank(bool replace,unsigned key,const std::shared_ptr<playback::Timeline>& value){
 PlaybackBank result;result.replace=replace;result.timelines[key]=value;result.bytes=value->bytes();return result;
}
static void invalidated(){
 check(rasterCache.bytes()==0&&rasterCache.hits==0&&rasterCache.misses==0,"replacement invalidates address-keyed scaled pixels");
 check(!activeMotion,"replacement releases the old active timeline");
 check(lastFrame==-2&&lastPhase==size_t(-1),"replacement forces the next image submission and phase evaluation");
}
static void retained(const std::shared_ptr<playback::Timeline>& active){
 check(activeMotion==active,"unchanged timeline keeps the active animation");
 check(rasterCache.bytes()==4,"non-replacing acceptance retains scaled pixels");
 check(lastFrame==7&&lastPhase==3,"non-replacing acceptance preserves submission sentinels");
 check(rasterCache.get(*active->images[0],1,1)[0]==0xff123456u&&rasterCache.hits==1,"the retained raster remains a valid hit");
}
static void charge(const std::shared_ptr<playback::Timeline>& value,size_t total){
 // Exercise the real resident-byte admission arithmetic without allocating
 // half a gigabyte. Frame pixels and bank ownership remain real objects.
 const size_t overhead=value->bytes()-value->resident;
 check(total>=overhead,"synthetic resident charge includes fixed bookkeeping");
 value->resident=total-overhead;check(value->bytes()==total,"resident charge has the requested exact size");
}

int main(){
 try{
  // Legal destruction and reconstruction at one address makes the failure
  // deterministic, independent of a particular allocator's reuse choices.
  ++cases;reset();alignas(playback::Frame) unsigned char storage[sizeof(playback::Frame)];
  auto*first=new(storage)playback::Frame(playback::Frame::pack(motion::Image{0xff123456u},1,1));
  check(rasterCache.get(*first,1,1)[0]==0xff123456u,"initial address owns the first raster");first->~Frame();
  auto*second=new(storage)playback::Frame(playback::Frame::pack(motion::Image{0xffabcdefu},1,1));
  check(rasterCache.get(*second,1,1)[0]==0xff123456u,"address reuse reproduces the stale-raster contract without invalidation");
  PlaybackBank empty;acceptPlaybackBank(std::move(empty));invalidated();
  check(rasterCache.get(*second,1,1)[0]==0xffabcdefu,"the actual replacement route removes the stale address entry");second->~Frame();

  ++cases;reset();seed();std::weak_ptr<const playback::Frame> released=motionCache.at(1)->images[0];
  auto replacement=timeline(0xffabcdefu,2);auto full=bank(true,2,replacement);acceptPlaybackBank(std::move(full));
  check(motionCache.size()==1&&motionCache.at(2)==replacement&&!motionCache.count(1),"full reload replaces the complete bank");
  check(motionBytes==replacement->bytes(),"full reload publishes exact byte accounting");
  check(released.expired(),"full replacement releases old frames after clearing the active owner");invalidated();

  ++cases;reset();seed();PlaybackBank clear;acceptPlaybackBank(std::move(clear));
  check(motionCache.empty()&&motionBytes==0,"an empty full bank clears obsolete timelines");invalidated();

  ++cases;reset();seed();auto active=activeMotion;auto added=timeline(0xffabcdefu,2);
  const size_t before=motionBytes;auto addition=bank(false,2,added);acceptPlaybackBank(std::move(addition));
  check(motionCache.size()==2&&motionCache.at(1)==active&&motionCache.at(2)==added,"incremental addition preserves prior timelines");
  check(motionBytes==before+added->bytes(),"incremental addition charges only the added timeline");retained(active);

  ++cases;reset();seed();replacement=timeline(0xffabcdefu,3);auto overwrite=bank(false,1,replacement);
  acceptPlaybackBank(std::move(overwrite));check(motionCache.size()==1&&motionCache.at(1)==replacement,"incremental replacement publishes the new timeline");
  check(motionBytes==replacement->bytes(),"replacement subtracts the old timeline before charging the new one");invalidated();

  ++cases;reset();seed();active=activeMotion;charge(active,Budget);motionBytes=Budget;
  added=timeline(0xffabcdefu);auto rejectedAddition=bank(false,2,added);acceptPlaybackBank(std::move(rejectedAddition));
  check(motionCache.size()==1&&!motionCache.count(2)&&motionBytes==Budget,"a full bank rejects additional bytes");
  check(rejectedAddition.timelines.at(2)==added,"capacity rejection does not consume the rejected timeline");retained(active);

  ++cases;reset();seed();active=activeMotion;auto filler=timeline(0xff778899u);charge(filler,Budget-motionBytes);
  motionCache[2]=filler;motionBytes+=filler->bytes();replacement=timeline(0xffabcdefu,8);
  check(replacement->bytes()>active->bytes(),"replacement would exceed the exact resident limit");
  auto rejectedReplacement=bank(false,1,replacement);acceptPlaybackBank(std::move(rejectedReplacement));
  check(motionCache.at(1)==active&&motionCache.at(2)==filler&&motionBytes==Budget,"rejected replacement preserves both bank entries and bytes");retained(active);

  ++cases;reset();seed();active=activeMotion;added=timeline(0xffabcdefu);
  charge(active,Budget-added->bytes());motionBytes=active->bytes();auto exact=bank(false,2,added);acceptPlaybackBank(std::move(exact));
  check(motionCache.size()==2&&motionCache.at(2)==added&&motionBytes==Budget,"an addition exactly at the byte cap is admitted");retained(active);

  reset();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"failures\":0}"<<std::endl;return 0;
 }catch(const std::exception&error){std::cerr<<error.what()<<std::endl;return 1;}
}
