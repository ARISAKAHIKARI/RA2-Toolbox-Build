#pragma once
#include "scene_bank.h"
#include "scene_poses.h"
#include "radar_publication.h"
#include <chrono>
#include <type_traits>

namespace external { namespace radar {
// The concrete Host reader implements this API. The fallback is for supplied
// in-memory readers used by portable renderer fixtures; a ProcessReader never
// falls back to live reads when its Runtime publication is unavailable.
template<class Reader,class=void>struct HasWorldFrame:std::false_type{};
template<class Reader>struct HasWorldFrame<Reader,std::void_t<
 decltype(std::declval<Reader&>().worldFrameToken()),
 decltype(std::declval<Reader&>().beginWorldFrame(uint32_t{},worldframe::Token{})),
 decltype(std::declval<Reader&>().endWorldFrame()),
 decltype(std::declval<Reader&>().worldFrameComplete())>>:std::true_type{};

template<class Reader>worldframe::Token sourceFrame(Reader&reader){
 if constexpr(HasWorldFrame<Reader>::value)return reader.worldFrameToken();
 else return {};
}
template<class Reader>bool completeFrame(Reader&reader){
 if constexpr(HasWorldFrame<Reader>::value)return reader.worldFrameComplete();
 else return true;
}
template<class Reader>bool liveRead(Reader&reader,uint32_t p,void*out,size_t n){
 if constexpr(HasWorldFrame<Reader>::value)return reader.readLive(p,out,n);
 else return reader.readFresh(p,out,n);
}
template<class Reader>class FrameScope {
 Reader&reader_;worldframe::Token previous_;uint32_t previousDemand_=0;bool ok_=true,entered_=false,previousComplete_=true;
public:
 FrameScope(Reader&r,uint32_t match,worldframe::Token token={},uint32_t demand=worldframe::Visual|worldframe::Passengers):reader_(r){
  if constexpr(HasWorldFrame<Reader>::value){
   previousDemand_=r.worldDemand();r.setWorldDemand(demand);
   previous_=r.worldFrameToken();previousComplete_=!previous_||r.worldFrameComplete();
   if(token){entered_=token!=previous_||r.worldDemand()!=previousDemand_;ok_=!entered_||r.beginWorldFrame(match,token);}
   else if(!previous_){entered_=true;ok_=r.beginWorldFrame(match);}
   else if(r.worldDemand()!=previousDemand_){entered_=true;ok_=r.beginWorldFrame(match,previous_);}
   if(previous_&&r.worldFrameToken()==previous_&&!previousComplete_)r.markWorldIncomplete();
  }
 }
 ~FrameScope(){if constexpr(HasWorldFrame<Reader>::value){
  if(entered_){
   const auto current=reader_.worldFrameToken();
   const bool childIncomplete=previous_&&current&&previous_->key.sameFrame(current->key)&&previous_->key.serial==current->key.serial&&!reader_.worldFrameComplete();
   if(previous_){reader_.setWorldDemand(previousDemand_);reader_.beginWorldFrame(previous_->key.match,previous_);if(!previousComplete_||childIncomplete)reader_.markWorldIncomplete();}
   else reader_.endWorldFrame();
  }
  reader_.setWorldDemand(previousDemand_);
 }}
 FrameScope(const FrameScope&)=delete;FrameScope&operator=(const FrameScope&)=delete;
 explicit operator bool()const{return ok_;}
};

enum class SceneConsumer:unsigned {Main,Minimap,Watch,Count};
// One owner performs visual decoding for a raw publication. Its reader points
// at the caller's pinned immutable bytes only while holding producerMutex_.
// Rendering and bank.get never take that mutex; asset work cannot stall capture.
template<class Reader>class SharedSceneProvider {
 struct Read {
  SharedSceneProvider*owner;
  bool operator()(uint32_t p,void*out,size_t n)const{return owner->reader_->read(p,out,n);}
  template<class R>static auto nullable(R&r,uint32_t p,void*out,size_t n,int)->decltype(r.readOptional(p,out,n)){return r.readOptional(p,out,n);}
  template<class R>static bool nullable(R&r,uint32_t p,void*out,size_t n,long){return r.read(p,out,n);}
  bool optional(uint32_t p,void*out,size_t n)const{return nullable(*owner->reader_,p,out,n,0);}
  template<class R>static auto batch(R&r,const std::vector<uint32_t>&p,size_t n,int)->decltype(r.prefetch(p,n),void()){r.prefetch(p,n);}
  template<class R>static void batch(R&,const std::vector<uint32_t>&,size_t,long){}
  void prefetch(const std::vector<uint32_t>&p,size_t n)const{batch(*owner->reader_,p,n,0);}
 };
 struct Key {
  uint32_t pid=0,match=0,reveal=0,serial=0;worldframe::Key source;bool managed=false;
  PublicationContext world;
  bool sameWorld(const Key&o)const{return pid==o.pid&&match==o.match&&managed==o.managed&&(!managed||source.sameWorld(o.source))&&
   world.house==o.world.house&&world.cells==o.world.cells&&world.bounds==o.world.bounds&&world.rawWidth==o.world.rawWidth&&world.rawHeight==o.world.rawHeight;}
  bool same(const Key&o)const{return sameWorld(o)&&reveal==o.reveal&&serial==o.serial&&world.frame==o.world.frame;}
 };
 struct Demand {uint32_t tick=0;uint8_t mask=0;bool present=false,real=false;};
 std::mutex producerMutex_;Reader*reader_=nullptr;Read read_{this};Sampler<Read>sampler_{read_};
 Key attempted_{};bool haveAttempt_=false;std::array<Demand,unsigned(SceneConsumer::Count)>demands_{};
 SceneBank&bank_;
public:
 struct Statistics {uint64_t attempts=0,captures=0,reused=0,deferred=0,failed=0;};
private:
 Statistics statistics_;
public:
 explicit SharedSceneProvider(SceneBank&bank=sceneBank()):bank_(bank){sampler_.wholeWorld(true);}
 Statistics statistics(){std::lock_guard<std::mutex>lock(producerMutex_);return statistics_;}
 std::shared_ptr<const Frame> acquire(Reader&reader,uint32_t pid,uint32_t match,uint32_t reveal,uint32_t now,
  SceneConsumer consumer,bool real,bool particles,bool lasers){
  const auto token=sourceFrame(reader);
  if constexpr(HasWorldFrame<Reader>::value)if(!token)return {};
  std::lock_guard<std::mutex>lock(producerMutex_);reader_=&reader;
  Key key;key.pid=pid;key.match=match;key.reveal=!!reveal;key.world=publicationContext(read_);
  if(token){key.serial=token->key.serial;key.source=token->key;key.managed=true;}
  if(!key.world.active||!completeFrame(reader)){++statistics_.failed;return {};}
  // A queued consumer can still hold an older token. Its completion time is
  // not a new observation or a genuine game rewind. Only a later serial may
  // reset the terrain owner to a smaller logic frame.
  if(token&&haveAttempt_&&attempted_.sameWorld(key)&&int32_t(key.serial-attempted_.serial)<0){++statistics_.deferred;return {};}
  if(haveAttempt_&&(!attempted_.sameWorld(key)||key.world.frame<attempted_.world.frame)){
   sampler_.reset();demands_={};haveAttempt_=false;
  }
  auto&demand=demands_[unsigned(consumer)];demand={now,uint8_t(uint8_t(particles)|(uint8_t(lasers)<<1)),true,real};
  auto bank=bank_.get(pid,match,reveal,now,real,token);
  const bool matches=bank&&sameWorld(*bank,key.world)&&bank->frame==key.world.frame&&
   (!token||(bank->sourceFrame&&bank->sourceFrame->key.sameFrame(token->key)&&bank->sourceFrame->key.serial==token->key.serial));
  if(matches&&supportsEffects(*bank,particles,lasers)){
   ++statistics_.reused;return bank;
  }
  // Config/view changes can reproject the sealed facts but may not silently
  // add a second capture when a late consumer requests more effect coverage.
  if(haveAttempt_&&attempted_.same(key)){++statistics_.deferred;return {};}
  attempted_=key;haveAttempt_=true;++statistics_.attempts;
  uint8_t mask=0;bool full=false;
  for(const auto&item:demands_)if(item.present&&uint32_t(now-item.tick)<=500){mask|=item.mask;full|=item.real;}
  // One canonical overview domain works for all cameras. The original Radar
  // backing remains in Geometry::source* for live world-identity guards.
  if(sampler_.hasTerrain()){
   auto old=sampler_.terrainFrame(now);
   if(old.terrain&&bool(!old.terrain->art.empty())!=full)sampler_.reset();
  }
  sampler_.wholeWorld(true);sampler_.effects(mask&1,mask&2);
  if(!sampler_.step(reveal,16384,full,true,false,true)||!completeFrame(reader)){
   sampler_.discardIncomplete();++statistics_.failed;return {};
  }
  if(!sampler_.hasTerrain()){++statistics_.deferred;return {};}
  auto frame=sampler_.snapshot(token?token->key.tick:now,key.world.frame);
  frame.sourceFrame=token;
  frame.observedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
  preparePoses(frame,read_);
  if(!frame.markerScanValid||!completeFrame(reader)||!sameWorld(frame,key.world)){
   ++statistics_.failed;return {};
  }
  bank=std::make_shared<const Frame>(std::move(frame));bank_.publish(pid,match,full,bank);++statistics_.captures;
  return supportsEffects(*bank,particles,lasers)&&(!real||!bank->terrain->art.empty())?bank:nullptr;
 }
};
template<class Reader>SharedSceneProvider<Reader>&sharedSceneProvider(){static SharedSceneProvider<Reader>provider;return provider;}
template<class Reader>std::shared_ptr<const Frame> sharedScene(Reader&reader,uint32_t pid,uint32_t match,uint32_t reveal,uint32_t now,
 SceneConsumer consumer,bool real,bool particles,bool lasers){
 return sharedSceneProvider<Reader>().acquire(reader,pid,match,reveal,now,consumer,real,particles,lasers);
}
}}
