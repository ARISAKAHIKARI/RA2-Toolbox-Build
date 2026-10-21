// UI acknowledgement only, once per accepted user input. Never called by
// automatic combat/cooperative continuations. Native Voice* queues local audio.
static void inputFeedback(uint32_t source,uint32_t target,bool enter){
 if(!sh||!sh->foreground||get<bool>(0xA8BC60+25)||!worldObjects.contains(source)||!own(source,lastHouse))return;
 uint32_t table=get<uint32_t>(source);int k=kind(source);if(k<0||k>2)return;
 if(target&&(!worldObjects.contains(target)||!live(target)))return;
 uint32_t slot=enter?0x358:target?0x370:0x368;
 uint32_t expected=enter?0x709020:target?0x7090A0:0x708FC0;
 // A foreign override must not be called with an assumed native ABI.
 if(get<uint32_t>(table+slot)!=expected||get<uint32_t>(table+0x354)!=0x708D90)return;
 if(target&&!enter)call<void>(expected,source,(void*)(uintptr_t)target);
 else call<void>(expected,source);
}
