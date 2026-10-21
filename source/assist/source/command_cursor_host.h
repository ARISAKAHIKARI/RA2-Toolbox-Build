// A mode preview, not a promise that the hovered target accepts an order.
// It uses the same key resolver, feature switches and type filters as clicks.
static void serviceCommandCursorHint(){
 using namespace commandcursor;
 if(!enabled||!shared||!commandFresh(GetTickCount())||!foregroundGame()||incoming.chat){clear();return;}
 auto freshRead=[](uint32_t at,void*p,size_t n){return externalReader.readFresh(at,p,n);};
 if(!external::display::inspect(freshRead).visible()){clear();return;}
 uint8_t keys[256]{};for(int n=0;n<256;++n)keys[n]=(GetAsyncKeyState(n)&0x8000)!=0;
 const uint32_t featureFlags=flags();uint32_t allowed=0;
 if(featureFlags&AdvancedInput)allowed|=pointerinput::Advanced;
 if(featureFlags&MagnetronInput)allowed|=pointerinput::Magnetron;
 if(featureFlags&CustomCS)allowed|=pointerinput::CS;
 if(featureFlags&AdvancedEnter)allowed|=pointerinput::Enter;
 if(featureFlags&AircraftSpread)allowed|=pointerinput::AirFirst|pointerinput::AirSecond;
 if(featureFlags&DiskPrimary)allowed|=256;
 const uint32_t holds[]={advancedKey,magnetronKey,csHoldKey,extra.enterKey,extra.airKey1,extra.airKey2,extra.diskKey};
 const uint32_t bits[]={pointerinput::Advanced,pointerinput::Magnetron,pointerinput::CS,pointerinput::Enter,pointerinput::AirFirst,pointerinput::AirSecond,256};
 uint32_t held=0;for(int n=0;n<7;++n){
  if(!extra.mouseButtons[n]||!(holds[n]&255))allowed&=~bits[n];
  if((allowed&bits[n])&&pointerinput::matchesHold(keys,holds[n],pointerinput::Enhanced))held|=bits[n];
 }
 // Mouse-only configurations do not leave a permanent special cursor.
 if(!held||keys[27]||((featureFlags&NativeAirWaypoints)&&airway::held(keys))){clear();return;}
 allowed&=held;
 static external::ReadSession previewReader;
 static uint32_t scanAt=0,scanPid=0,scanMatch=0,scanKeys=0,scanRevision=0;
 const uint32_t now=GetTickCount();
 if(scanAt&&scanPid==pid&&scanMatch==commandMatch()&&scanKeys==held&&scanRevision==settingsRevision&&uint32_t(now-scanAt)<32)return;
 scanAt=now;scanPid=pid;scanMatch=commandMatch();scanKeys=held;scanRevision=settingsRevision;
 if((previewReader.pid()!=pid||previewReader.generation()!=externalReader.generation())&&!previewReader.open(pid)){clear();return;}
 previewReader.begin();
 auto read=[&](uint32_t at,void*p,size_t n){return previewReader.read(at,p,n);};
 external::GameView<decltype(read)> view(read);
 uint32_t house=view.template get<uint32_t>(0xA83D4C);int frame=view.template get<int>(0xA8ED84,-1);
 // Native placement and native UI modes always retain their own cursor.
 uint8_t tools[5]{};
 if(house<0x10000||frame<0||!read(0x87F7E8+0x11B0,tools,sizeof tools)||tools[0]||tools[1]||tools[2]||tools[3]||tools[4]){clear();return;}
 uint32_t eligible=0;
 for(uint32_t p:view.vec(0xA8ECB8,pointerinput::MaxSources)){
  if(!view.live(p)||view.template get<uint32_t>(p+0x21C)!=house||view.template get<uint8_t>(p+0x1C8)||view.template get<uint32_t>(p+0x11C))continue;
  int k=view.kind(p);uint32_t t=view.type(p,k);char id[24]{};
  if(!t||!read(t+0x24,id,23))continue;
  auto accepts=[&](int f){return featuretypes::allowed(featureTypes,f,id);};
  if(k==0||(k==2&&(view.template get<uint8_t>(p+0x134)||view.template get<int>(p+0x94)==3||view.template get<uint8_t>(p+0x84)||view.template get<uint8_t>(p+0x8D)))){
   if(accepts(featuretypes::Aircraft))eligible|=pointerinput::AirFirst|pointerinput::AirSecond;
  }
  if((eligible&allowed)==allowed)break;
  if(k!=1&&k!=2)continue;
  planner::Actor combat{};
  if(allowed&(pointerinput::Advanced|pointerinput::CS|pointerinput::Magnetron))combat=view.combat(p,frame,true);
  bool armed=combat.weapons[0].range>0||combat.weapons[1].range>0;
  if(armed&&accepts(featuretypes::AdvancedAttack))eligible|=pointerinput::Advanced;
  if(armed)eligible|=pointerinput::CS;
  if(armed&&accepts(featuretypes::Magnetron)&&((combat.weapons[0].flags|combat.weapons[1].flags)&planner::LocomotorWeapon))eligible|=pointerinput::Magnetron;
  if(accepts(featuretypes::EnterSource))eligible|=pointerinput::Enter;
  if(k==2&&accepts(featuretypes::Disk)&&oreconfig::equal(id,"DISK"))eligible|=256;
  if((eligible&allowed)==allowed)break;
 }
 pointerinput::Bindings c{advancedKey,magnetronKey,csHoldKey,extra.enterKey,extra.airKey1,extra.airKey2,extra.diskKey,extra.sellKey,allowed&eligible};
 auto resolved=pointerinput::resolveBindings(keys,pointerinput::Enhanced,c);
 Mode mode=commandcursor::None;uint32_t binding=0;
 if(resolved&pointerinput::AirSecond){mode=AirSecond;binding=extra.airKey2;}
 else if(resolved&pointerinput::AirFirst){mode=AirFirst;binding=extra.airKey1;}
 else if(resolved&pointerinput::Magnetron){mode=Magnetron;binding=magnetronKey;}
 else if(resolved&pointerinput::Advanced){mode=Advanced;binding=advancedKey;}
 else if(resolved&pointerinput::CS){mode=CS;binding=csHoldKey;}
 else if(resolved&pointerinput::Enter){mode=Enter;binding=extra.enterKey;}
 else if(resolved&pointerinput::Control){mode=Disk;binding=extra.diskKey;}
 if(view.template get<uint32_t>(0xA83D4C)!=house||view.template get<int>(0xA8ED84,-1)<frame){clear();return;}
 publish({pid,commandMatch(),GetTickCount(),binding,mode});
}
