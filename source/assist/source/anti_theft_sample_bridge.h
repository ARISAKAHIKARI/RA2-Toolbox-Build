#pragma once
// This cache proves only whether the current sealed source contains an eligible
// owned defense building. It never replaces the later World projection or the
// fresh execution guards. A positive scan needs its visited prefix; a negative
// scan needs the entire declared roster. Any failed read makes either unknown.
struct AntiTheftPresenceSample {
 worldframe::Key source{};uint32_t house=0;long revision=-1;
 bool valid=false,present=false;
};
static AntiTheftPresenceSample antiTheftPresenceSample{};
static bool sameAntiTheftSource(const worldframe::Key&a,const worldframe::Key&b){
 return a.sameFrame(b)&&a.serial==b.serial&&a.tick==b.tick&&a.demand==b.demand;
}
static bool antiTheftSourceCurrent(const worldframe::Key&source,long revision){
 MemoryBarrier();
 return worldFrameReady&&worldFrame.sealed()&&!worldFrame.overflow()&&
  source.match==match&&source.frame==snap.frame&&sameAntiTheftSource(source,worldFrame.key)&&
  !(revision&1)&&featureConfigRevision==revision&&sh->featureConfigSeq==revision;
}
static bool collectAntiTheftPresence(uint32_t house,bool&present,bool&changed,long&revision){
 present=changed=false;revision=-1;
 if(!sampleReadDepth||!sampleReadComplete||!worldFrameReady||!worldFrame.sealed()||worldFrame.overflow()||!refreshFeatureTypes())return false;
 const auto source=worldFrame.key;revision=featureConfigRevision;
 if(!antiTheftSourceCurrent(source,revision))return false;
 const auto&previous=antiTheftPresenceSample;
 if(previous.valid&&previous.house==house&&previous.revision==revision&&sameAntiTheftSource(previous.source,source)){
  present=previous.present;return true;
 }
 Vec buildings{};if(!sampleRoster(0xA8EB40,buildings))return false;
 for(int i=0;i<buildings.count;++i){uint32_t p=item(buildings,i);
  if(p&&own(p,house)&&get<int>(p+0x6C)>0&&!get<bool>(p+0x81)&&antiTheftBuilding(p)){present=true;break;}
 }
 if(!sampleReadComplete||!antiTheftSourceCurrent(source,revision))return false;
 // Reads on the miss keep their page demands subscribed. Those demands live
 // until the next capture; its new complete source key forces another scan.
 antiTheftPresenceSample={source,house,revision,true,present};changed=true;return true;
}
