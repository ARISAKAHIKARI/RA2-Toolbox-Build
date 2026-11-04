#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdio>
#include "../source/additional_settings.h"
#include "../source/settings_dependencies.h"
#include "../../suite/source/settings_categories.h"
int main(){
 using namespace assist;
 AdditionalSettings settings;
 int checks=0;auto check=[&](bool ok){assert(ok);++checks;};
 auto ui=[&]{settingsdeps::State state;
  state.set(settingsdeps::Need::ControlAll,settings.controlAll);
  state.set(settingsdeps::Need::NativeControl,settings.nativeControlRequested());
  state.set(settingsdeps::Need::NetworkControlAllowed,controlmode::sender(settings.controlRequest()));return state;
 };
 check(!settings.techEnhanceCompareLevel);settingsdeps::State exact;check(!settingsdeps::decision(9472,exact).enabled());exact.set(settingsdeps::Need::ExactTechnology,true);check(settingsdeps::decision(9472,exact).enabled());
 check(settings.controlImplementation==0&&settings.controlRequest()==controlmode::Off);
 settings.controlCommandOwner=settings.networkHousePreserve=true;
 check(!settings.commandOwnerRequested()&&settings.networkHouseRequested());
 check(!settingsdeps::decision(9193,ui()).enabled()&&settingsdeps::decision(9187,ui()).enabled());
 settings.controlAll=true;
 check(settings.controlRequest()==controlmode::Parameters&&settings.parameterControlRequested());
 check(!settings.nativeControlRequested()&&!settings.commandOwnerRequested()&&!settings.networkHouseRequested());
 check(!settingsdeps::decision(9193,ui()).enabled()&&!settingsdeps::decision(9187,ui()).enabled());
 check(settings.controlCommandOwner&&settings.networkHousePreserve); // saved choices survive grey state
 settings.nativeSellCursor=settings.sellEnemy=true;
 check(!settings.networkHouseRequested()); // no hidden sender Hook exception in Master mode
 check(settings.nativeSellCursor&&settings.sellEnemy); // independent preferences are not erased
 settings.controlImplementation=1;
 check(settings.controlRequest()==controlmode::Native&&settings.nativeControlRequested());
 check(settings.commandOwnerRequested()&&settings.networkHouseRequested());
 check(settingsdeps::decision(9193,ui()).enabled()&&settingsdeps::decision(9187,ui()).enabled());
 settings.controlImplementation=0;
 check(!settings.commandOwnerRequested()&&!settings.networkHouseRequested());
 settings.controlAll=false;
 check(settings.controlRequest()==controlmode::Off&&!settings.commandOwnerRequested()&&settings.networkHouseRequested());
 check(settings.controlCommandOwner&&settings.networkHousePreserve);
 check(!settingsdeps::decision(9193,ui()).enabled()&&settingsdeps::decision(9187,ui()).enabled());
 settings.controlAll=true;settings.controlImplementation=99;
 check(settings.controlRequest()==controlmode::Parameters&&!settings.commandOwnerRequested()&&!settings.networkHouseRequested());
 settings.controlImplementation=2;check(settings.controlRequest()==controlmode::Master&&settings.masterControlRequested());
 for(int value:{-1,0,1,2,3,99})check(controlmode::implementation(value)==(value==1||value==2?value:0));
 settingsdeps::State blocked;blocked.set(settingsdeps::Need::ExactTechnology,true);blocked.hardwareBlocked=15;
 for(int id:{860,420,9097,9472})check(settingsdeps::decision(id,blocked).missing==settingsdeps::Need::HardwareBudget);
 blocked.hardwareBlocked=0;for(int id:{860,420,9097,9472})check(settingsdeps::decision(id,blocked).enabled());
 for(int id:{860,865,9250,9193,9194,9472,9473})check(suite::categories::row(4,&id,1)==suite::categories::Rules);
 std::printf("PASS: %d host control mode, retained-preference and UI availability checks\n",checks);
}
