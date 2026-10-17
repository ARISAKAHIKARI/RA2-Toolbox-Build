#include "../source/tab_control.h"
#include <cassert>
int main(){tabui::Controller tab;tab.mode(tabui::Mode::TapOrHold);tab.holdThreshold(300);
 assert(tab.event(true,true,false,false,100)&&tab.visible());assert(tab.event(false,true,false,false,200)&&tab.visible());
 tab.event(true,true,false,false,500);assert(tab.visible());tab.event(false,true,false,false,600);assert(!tab.visible());
 tab.event(true,true,false,false,900);for(unsigned t=1000;t<1500;t+=20)tab.event(true,true,false,false,t);assert(tab.visible());tab.event(false,true,false,false,1500);assert(!tab.visible());
 tab.event(true,true,false,false,2000);tab.event(false,true,false,false,2050);assert(tab.visible());tab.event(true,true,false,false,2100);tab.event(false,true,false,false,2600);assert(!tab.visible());
 assert(!tab.event(true,true,true,false,3000));assert(!tab.event(true,true,false,true,3100));assert(!tab.event(true,false,false,false,3200));
 tab.event(true,true,false,false,0xfffffff0u);tab.event(false,true,false,false,0x300u);assert(!tab.visible());
 tab.mode(tabui::Mode::Toggle);tab.event(true,true,false,false);assert(tab.visible());tab.event(false,true,false,false);assert(tab.visible());tab.mode(tabui::Mode::Hold);tab.event(true,true,false,false);assert(tab.visible());tab.event(false,true,false,false);assert(!tab.visible());
 for(auto mode:{tabui::Mode::Toggle,tabui::Mode::Hold,tabui::Mode::TapOrHold}){
  tab.mode(mode);bool hidden=false,manual=false;
  assert(!tabui::displayRequested(hidden,false,manual,tab.visible()));
  tabui::toggleDisplay(false,tab.visible(),hidden,manual);assert(tabui::displayRequested(hidden,false,manual,tab.visible()));
  tabui::toggleDisplay(false,tab.visible(),hidden,manual);assert(!tabui::displayRequested(hidden,false,manual,tab.visible()));
  hidden=false;manual=false;assert(tabui::displayRequested(hidden,true,manual,false));
  tabui::toggleDisplay(true,false,hidden,manual);assert(!tabui::displayRequested(hidden,true,manual,false));
  tabui::toggleDisplay(true,false,hidden,manual);assert(tabui::displayRequested(hidden,true,manual,false));
 }
}
