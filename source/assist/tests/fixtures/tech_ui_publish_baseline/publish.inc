static bool healthy(){return service.state!=techbp::System&&service.state!=techbp::Foreign&&service.state!=techbp::Thread;}
static bool ready(uint32_t group){return healthy()&&(service.activeTags()&group)==group;}
static void publish(){
 if(!sh)return;const uint32_t active=service.activeTags();
 sh->techParameterStatus=failure?failure:desired?(ready(3)?techbp::Ready:(refusedGroups&3)?techbp::Slots:service.state<0?service.state:techbp::Waiting):(active&3)?techbp::Waiting:techbp::Off;
 sh->techParameterError=uint32_t(service.lastError);sh->techParameterSlots=__builtin_popcount(active&3);sh->techParameterExceptions=uint32_t(fullExceptions);
 sh->techEnhanceParameterStatus=enhancementFailure?enhancementFailure:enhancementDesired?(ready(12)?techbp::Ready:(refusedGroups&12)?techbp::Slots:service.state<0?service.state:techbp::Waiting):(active&12)?techbp::Waiting:techbp::Off;
 sh->techEnhanceParameterError=uint32_t(service.lastError);sh->techEnhanceParameterSlots=__builtin_popcount(active&12);sh->techEnhanceParameterExceptions=uint32_t(enhancementExceptions);
}