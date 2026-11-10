// Included by the runner after the existing live-guard fixture, before main.
// Unknown callers are synthetic; these tests do not model JJ's own code.
static void productionCompatibility(){using namespace subject;
 auto source=[&](unsigned tag,uint32_t caller,unsigned include){
  if(!tag){fullStack[0]=caller;fullStack[3]=include;}
  else{put<uint32_t>(Stack+0x4C,caller);put<uint32_t>(Stack+0x58,include);}
 };
 auto passes=[&](unsigned tag){return tag?storage.techProductionEnhancePasses:storage.techProductionFullPasses;};
 for(unsigned rule=0;rule<techenhancement::Count;++rule)for(unsigned tag:{0u,2u,3u})
 for(unsigned include:{0u,1u})for(uint32_t caller:{0x4457A2u,0x650000u,0x71000000u}){
  seed(rule);source(tag,caller,include);rejects(tag,"compatibility disabled preserves original caller scope");
  check(passes(tag)==0,"decline is not a permission pass");storage.techProductionCompatibility=1;
  succeeds(tag,"compatibility admits an additional local-house caller");check(passes(tag)==1,"only successful extension increments its count");
  check((tag?storage.techProductionFullPasses:storage.techProductionEnhancePasses)==0,"unrelated count stays unchanged");
  storage.techProductionCompatibility=0;rejects(tag,"turning compatibility off restores original scope immediately");check(passes(tag)==1,"disable cannot increment counters");
 }
 for(unsigned tag:{0u,2u,3u})for(unsigned mutation=0;mutation<28;++mutation){
  seed();source(tag,0x71000000,1);storage.techProductionCompatibility=1;succeeds(tag,"extended candidate initially valid");
  switch(mutation){
   case 0:storage.stop=1;break;case 1:fault=true;break;case 2:sh=nullptr;break;
   case 3:put<uint32_t>(0xA83D4C,House+4);break;case 4:put<uint32_t>(House+0x10,78);break;
   case 5:put<uint32_t>(0xA8B230,Scenario+4);break;case 6:put<int>(0xA8B238,4);break;
   case 7:put<int>(0xA8ED84,99);break;case 8:put<bool>(0xA8E9A0,false);break;
   case 9:put<uint32_t>(0xAC1198,House);break;case 10:put<bool>(House+0x1F5,true);break;
   case 11:put<bool>(House+0x1F6,true);break;case 12:put<bool>(House+0x1F8,true);break;
   case 13:put<uint32_t>(Type,0x7EB610);break;case 14:put<uint32_t>(Type+0x10,101);break;
   case 15:if(tag)enhancementPolicyRevision=0;else nativePolicyRevision=0;break;
   case 16:if(tag)policy.enhanced=0;else policy.enabled=0;break;
   case 17:if(tag)storage.techEnhance=0;else storage.flags=0;break;
   case 18:source(tag,0xFFFF,1);break;case 19:source(tag,0x71000000,2);break;
   case 20:if(tag)put<uint32_t>(Stack+0x54,1);else fullStack[2]=1;break;
   case 21:if(tag)put<uint32_t>(Stack+0x10,House+4);else techImplementation=techmode::Data;break;
   case 22:if(tag)put<int>(Type+0x634,1);else fullStack[1]=Unknown;break;
   case 23:if(tag)put<uint32_t>(techenhancement::Rules[10].array+4,Items+4);else policy.types[hash(Type)]={};break;
   case 24:if(tag)policy.enhancement[10].index=-1;else put<uint8_t>(0x6A97C3,0);break;
   case 25:if(tag)put<int>(techenhancement::Rules[10].array+16,0);else put<uint8_t>(0x6AA777,0);break;
   case 26:if(tag)put<uint32_t>(Items,Unknown);else put<uint8_t>(0x4F7870,0);break;
   case 27:if(tag)put<uint8_t>(0x4F794A,0);else unreadable=0xA83D4C;break;
  }
  rejects(tag,"compatibility never bypasses live admission guards");check(passes(tag)==1,"failed revalidation leaves count unchanged");
 }
 for(unsigned tag:{0u,2u,3u}){
  seed();storage.techProductionCompatibility=1;succeeds(tag,"original sidebar caller still accepted");check(passes(tag)==0,"sidebar is not an extra caller");
  for(uint32_t sp:{0u,0xFFFFu,UINT32_MAX-8}){seed();storage.techProductionCompatibility=1;source(tag,0x71000000,1);auto c=context(tag);c.Esp=sp;const auto before=c;check(!dispatch(c,tag),"invalid stack bounds reject before reads");check(!std::memcmp(&before,&c,sizeof c),"invalid stack leaves context intact");}
  seed();storage.techProductionCompatibility=1;source(tag,0x71000000,1);auto c=context(tag);if(tag)c.Ebp=House+4;else c.Ecx=House+4;const auto before=c;check(!dispatch(c,tag),"other House never receives compatibility permission");check(!std::memcmp(&before,&c,sizeof c),"other House context unchanged");
 }
 for(unsigned tag:{2u,3u}){
  seed();storage.techProductionCompatibility=1;source(tag,techEnhanceProbeReturn,1);rejects(tag,"compatibility cannot authorize inactive private probe");
  probeDepth=1;probeToken=99;gameentry::activeToken=98;rejects(tag,"compatibility cannot authorize stale probe token");
  gameentry::activeToken=99;succeeds(tag,"valid probe remains accepted");check(passes(tag)==0,"valid private probe is not an extra caller");
  seed();storage.techProductionCompatibility=1;source(tag,0x71000000,1);auto c=context(tag);if(tag==2)c.Edi=Unknown;else c.Ecx=Unknown;check(!dispatch(c,tag),"extra caller still validates type register");
 }
 seed();storage.techProductionCompatibility=1;source(3,0x71000000,1);policy.compareLevel=0;rejects(3,"optional level comparison still requires its published permission");succeeds(2,"skip minus-one and level comparison stay independent");
}
