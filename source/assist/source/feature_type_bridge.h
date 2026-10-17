// A coherent host-owned filter snapshot, consumed only on the manager's game thread.
// No per-feature hook, rule write, secondary injection or game-thread wait.
static featuretypes::Config featureConfig{},featureConfigNext{};static long featureConfigRevision=-1;
static void resetFeatureTypes(){featureConfigRevision=-1;}
static bool refreshFeatureTypes(){
 long seq=sh->featureConfigSeq;if(seq&1)return false;if(seq==featureConfigRevision)return true;
 MemoryBarrier();memcpy(&featureConfigNext,&sh->featureConfig,sizeof featureConfigNext);MemoryBarrier();
 if(seq!=sh->featureConfigSeq||!featuretypes::valid(featureConfigNext))return false;
 featureConfig=featureConfigNext;featureConfigRevision=seq;return true;
}
static bool featureTypeAllowed(featuretypes::Feature f,uint32_t type){
 if(!refreshFeatureTypes())return false;
 if(!featureConfig.filters[f].custom)return true;
 char id[24]{};return type>=0x10000&&read(type+0x24,id,23)&&featuretypes::allowed(featureConfig,f,id);
}
static bool featureObjectAllowed(featuretypes::Feature f,uint32_t p){return p&&featureTypeAllowed(f,typeOf(p,kind(p)));}
