#include "../../shared/source/overlay_recovery.h"
#include "../source/tab_control.h"
#include "../../external_reader/source/display_state.h"
#include <cassert>
#include <iostream>
int main(){
 overlayui::Recovery repair;overlayui::Context context{5,9,1,800,600};
 tabui::Controller tab;tab.mode(tabui::Mode::TapOrHold);
 tab.event(true,true,false,false,100);tab.event(false,true,false,false,150);
 bool hidden=false,manual=false;
 auto visible=[&]{return tabui::displayRequested(hidden,false,manual,tab.visible());};
 assert(visible()&&repair.due(150,true,context,false));repair.submitted(true);
 assert(!repair.due(1149,visible(),context,true));
 // WS_VISIBLE may remain true even though the DWM surface/z-order was lost.
 assert(repair.due(1150,visible(),context,true));repair.submitted(false);
 assert(!repair.due(1399,visible(),context,true));assert(repair.due(1400,visible(),context,true));repair.submitted(true);
 for(auto state:{external::display::State{false,1,0,0},external::display::State{true,0,0,0},external::display::State{true,1,1,0},external::display::State{true,1,0,1}}){assert(!state.visible());assert(!repair.due(2000,visible()&&state.visible(),context,true));}
 // Loading, minimize, menu, focus loss: preserve intent, repair on return.
 for(int phase=0;phase<4;++phase){tab.loseFocus();assert(visible());assert(!repair.due(1500,false,context,false));assert(repair.due(1501,visible(),context,false));}
 // A new round and a replaced game window rebuild immediately.
 ++context.match;assert(repair.due(1502,visible(),context,true));
 ++context.window;assert(repair.due(1503,visible(),context,true));
 context.width=1024;assert(repair.due(1504,visible(),context,true));
 // Manual hiding and a second short Tab stay hidden during periodic repair.
 tabui::toggleDisplay(false,tab.visible(),hidden,manual);assert(!visible());
 for(unsigned now=2000;now<7000;now+=1000)assert(!repair.due(now,visible(),context,false));
 tabui::toggleDisplay(false,tab.visible(),hidden,manual);assert(visible()&&repair.due(7000,visible(),context,false));
 hidden=manual=false;tab.event(true,true,false,false,8000);tab.event(false,true,false,false,8050);assert(!visible()&&!repair.due(8050,visible(),context,false));
 // A held key is not restored after losing focus; tick wrap is harmless.
 tab.mode(tabui::Mode::Hold);tab.event(true,true,false,false,9000);tab.loseFocus();assert(!visible());
 repair.suspend();assert(repair.due(0xffffff00,true,context,true));assert(!repair.due(0x100,true,context,true));assert(repair.due(0x300,true,context,true));
 std::cout<<"PASS overlay recovery through loading/focus/round/window changes, periodic reupload, bounded retry, manual hiding and clock wrap\n";
}
