#include "../source/air_path_policy.h"
#include "../source/shroud_policy.h"
#include "../source/settings_dependencies.h"
#include "../../suite/source/display_settings.h"
#include "../../suite/source/settings_categories.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 using namespace assist;
 int checks=0;
 for(int session:{0,5,3,4}){
  // Compatibility was native in SP. It must now retain usable event routing
  // without either UI-gate or arrival replacement in every supported mode.
  assert(airpathpolicy::events(session,false,true,0));++checks;
  assert(!airpathpolicy::native(session,true,0));++checks;
  assert(!airpathpolicy::arrival(session,true,0));++checks;
  assert(airpathpolicy::events(session,true,false,0));++checks;
  for(unsigned mode:{1u,2u}){
   assert(airpathpolicy::native(session,true,mode));++checks;
   assert(!airpathpolicy::events(session,true,true,mode));++checks;
   assert(airpathpolicy::arrival(session,true,mode)==(mode==2));++checks;
  }
 }
 for(int session:{-1,1,2,6}){
  assert(!airpathpolicy::events(session,true,true,0));++checks;
  assert(!airpathpolicy::native(session,true,2));++checks;
 }
 for(int strategy:{1,2,7,8}){
  assert(!shroudpolicy::materialMode(shroudpolicy::runtimeMode(strategy)));++checks;
  assert(shroudpolicy::fullPreview(strategy));++checks;
 }
 assert(shroudpolicy::runtimeMode(1)==shroudpolicy::runtimeMode(7));++checks;
 assert(shroudpolicy::runtimeMode(2)==shroudpolicy::runtimeMode(8));++checks;
 assert(shroudpolicy::runtimeMode(7)==2);++checks;
 assert(shroudpolicy::runtimeMode(8)==1);++checks;
 assert(shroudpolicy::runtimeMode(0)==0);++checks;
 assert(shroudpolicy::runtimeMode(3)==0);++checks;
 assert(shroudpolicy::runtimeMode(4)==3);++checks;
 for(int strategy:{5,6,-1,9}){
  assert(shroudpolicy::savedStrategy(strategy)==0);++checks;
  assert(shroudpolicy::runtimeMode(strategy)==0);++checks;
  assert(shroudpolicy::uiIndex(strategy)==0);++checks;
  assert(!shroudpolicy::fullPreview(strategy));++checks;
 }
 for(int ui=0;ui<=4;++ui){assert(shroudpolicy::uiIndex(shroudpolicy::strategyFromUI(ui))==ui);++checks;}
 for(int ui:{-1,5,6,7,8}){assert(shroudpolicy::strategyFromUI(ui)==-1);++checks;}
 assert(shroudpolicy::uiIndex(7)==1&&shroudpolicy::uiIndex(8)==2);++checks;
 assert(shroudpolicy::legacyStrategy(1)==2&&shroudpolicy::legacyStrategy(2)==1&&shroudpolicy::legacyStrategy(3)==4);++checks;
 assert(shroudpolicy::legacyStrategy(4)==0&&shroudpolicy::legacyStrategy(5)==0);++checks;
 assert(!suite::validDisplayEdit(suite::ShroudStrategy,5)&&!suite::validDisplayEdit(suite::ShroudStrategy,8));++checks;
 settingsdeps::State all;all.available=~uint64_t(0);
 for(int id:{445,446,447,448,466,810,811,812,9180}){assert(!settingsdeps::decision(id,all).enabled());++checks;}
 for(int id:{473,474,702}){assert(settingsdeps::decision(id,all).enabled());++checks;}
 settingsdeps::State off;
 assert(!settingsdeps::decision(108,off).enabled()&&!settingsdeps::decision(208,off).enabled());++checks;
 off.set(settingsdeps::Need::ManualReveal,true);
 assert(settingsdeps::decision(108,off).enabled()&&settingsdeps::decision(208,off).enabled());++checks;
 for(int id:{108,208,9199}){assert(suite::categories::row(3,&id,1)==suite::categories::Rules);++checks;}
 for(int strategy=0;strategy<=shroudpolicy::MaxStrategy;++strategy){
  assert(shroudpolicy::revealEnabled(true,strategy));++checks;
  assert(shroudpolicy::revealEnabled(false,strategy)==(strategy==3));++checks;
  assert(shroudpolicy::automaticReveal(strategy)==(strategy==3));++checks;
 }
 std::printf("PASS: %d feature routes, restored shroud choices and unavailable feature controls\n",checks);
}
