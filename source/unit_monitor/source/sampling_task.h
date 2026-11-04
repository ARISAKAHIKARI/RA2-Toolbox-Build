#pragma once
#include "sampler.h"
#include "../../external_reader/source/shared_reader.h"
#include "../../external_reader/source/game_asset_files.h"
#include "../../external_reader/source/cameo_catalog.h"
#include <memory>

namespace um {
struct SampleRequest {uint64_t generation=0;uint32_t pid=0;uint64_t connection=0;};
struct SampleResult {
 SampleRequest request;std::shared_ptr<const Snapshot> snapshot;
 external::layout::Result probe{};DWORD readError=0,openError=0;
};
// Owned exclusively by the sampling worker. Callbacks are synchronous and
// scoped to this task; neither the UI reader nor its page cache is shared.
class SamplingTask {
 external::ReadSession reader_;external::assets::Files files_;
 uint64_t generation_=0,connection_=0;uint32_t pid_=0,match_=0;IconCache* cache_;
 inline static thread_local SamplingTask* current_=nullptr;
 static bool memory(uint32_t p,void*out,size_t n){return current_&&current_->reader_.readFresh(p,out,n);}
 external::assets::RuntimeCameos<decltype(&memory)> cameos_{&memory};
 std::unique_ptr<Sampler<external::SharedReadView,external::NoResolve>> sampler_;
 static bool load(const char*name,std::vector<uint8_t>&out){auto&c=*current_;c.files_.update(c.reader_,c.pid_,c.match_);return c.files_.load(name,out);}
 static void resolve(Type&t,uint32_t match){
  auto&c=*current_;c.match_=match;c.files_.update(c.reader_,c.pid_,match);
  char image[26]{};c.reader_.readFresh(t.ptr+0x1F8,image,25);
  auto file=c.cameos_.resolve(load,c.files_.revision(),c.files_.revision()^(GetTickCount64()/5000),t.id,image,t.cameo);
  memset(t.cameo,0,sizeof t.cameo);if(file.size()<sizeof t.cameo)memcpy(t.cameo,file.data(),file.size());
 }
 void reset(){sampler_.reset();files_.clear();cameos_.clear();reader_.close();match_=0;}
public:
 explicit SamplingTask(IconCache*cache):cache_(cache){}
 std::shared_ptr<const SampleResult> run(const SampleRequest&request){
  if(request.generation!=generation_||request.pid!=pid_||request.connection!=connection_){reset();generation_=request.generation;pid_=request.pid;connection_=request.connection;}
  if(!pid_)return {};
  auto result=std::make_shared<SampleResult>();result->request=request;
  if(!reader_.open(pid_)){result->openError=reader_.lastOpenError;result->snapshot=std::make_shared<const Snapshot>();return result;}
  // PID reuse may open a different process before the UI detects its old
  // handle's exit. Never publish that new process under the old identity.
  if(reader_.generation()!=request.connection)return {};
  struct Binding {SamplingTask*previous;Binding(SamplingTask*p):previous(current_){current_=p;}~Binding(){current_=previous;}} binding(this);
  if(!sampler_)sampler_=std::make_unique<Sampler<external::SharedReadView,external::NoResolve>>(external::SharedReadView{&reader_},external::NoResolve{},cache_,load,resolve);
  reader_.begin();result->snapshot=std::make_shared<const Snapshot>(sampler_->sample(GetTickCount(),true));
  result->probe=sampler_->probe();result->readError=reader_.lastReadError;return result;
 }
};
}
