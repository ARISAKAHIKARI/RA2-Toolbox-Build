static worldframe::DemandLeases runtimeDemandLeases;
static bool worldFrameBoundaryOnly=false;
static void rememberMissingWorldSpan(uint32_t,size_t){}
static void requestWorldRange(uint32_t at,size_t n){
 if(!worldframe::range(at,n))return;
 for(uint32_t page=at&~4095u;uint64_t(page)<uint64_t(at)+n;page+=4096){const uint32_t index=page>>12;const uint64_t mask=uint64_t(1)<<(index&63);
  if(!(runtimeDemandBits[index>>6]&mask)){runtimeDemandBits[index>>6]|=mask;runtimeWorldDemands.push_back(page);if(!++runtimeDemandRevision)++runtimeDemandRevision;
   runtimeDemandLeases.retain(page,worldFrame.key.tick);}}
}
static bool readWorldSample(uint32_t at,void*out,size_t n){
 requestWorldRange(at,n); // Keep successful consumer dependencies subscribed.
 const bool ok=worldFrameReady&&worldFrame.read(at,out,n);
 if(!ok){sampleReadComplete=false;rememberMissingWorldSpan(at,n);}return ok;
}
class SampleReadScope {
 unsigned previousDepth_;bool previousComplete_,ready_,finished_=false,result_=false;
public:
 SampleReadScope():previousDepth_(sampleReadDepth),previousComplete_(sampleReadComplete),ready_(worldFrameReady&&!worldFrameBoundaryOnly&&worldFrame.sealed()&&worldFrame.key.match==match&&worldFrame.key.frame==snap.frame){++sampleReadDepth;sampleReadComplete=ready_;}
 explicit operator bool()const{return ready_;}
 bool complete()const{return finished_?result_:ready_&&sampleReadComplete;}
 void finish(){if(finished_)return;result_=complete();sampleReadDepth=previousDepth_;sampleReadComplete=previousComplete_&&(!previousDepth_||result_);finished_=true;}
 ~SampleReadScope(){finish();}
 SampleReadScope(const SampleReadScope&)=delete;SampleReadScope&operator=(const SampleReadScope&)=delete;
};
