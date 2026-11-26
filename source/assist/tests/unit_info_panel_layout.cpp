#include "../source/unit_info_panel_layout.h"
#include <cassert>
#include <iostream>
using namespace external::unitpanel;
namespace p=unitpanelhost::panelmetrics;
int main(){
 Model m;m.reset(100,20);
 auto*g=m.selectedType(0x40000,2);assert(g);m.single.id=11;
 assert(!p::expandable(m)&&!p::cargoVisible(m,false));
 assert(p::entries(m,false)==0&&p::slots(m,false,0)==2&&p::pages(m,false)==1);
 assert(p::cols(p::slots(m,false,0))==2&&p::rows(p::slots(m,false,0))==1);
 Unit cargo{};cargo.type=0x50000;cargo.id=22;
 for(int i=0;i<10;++i){cargo.id=22+i;m.appendPassenger(cargo);}
 assert(p::expandable(m)&&!p::cargoVisible(m,false));
 // The folded panel shows only the vehicle Cameo and HP tile; the expanded panel includes cargo automatically.
 assert(p::entries(m,false)==0&&p::slots(m,false,0)==2&&p::pages(m,false)==1);
 assert(p::cargoVisible(m,true)&&p::entries(m,true)==10&&p::pages(m,true)==2);
 assert(p::slots(m,true,0)==9&&p::cols(p::slots(m,true,0))==3&&p::rows(p::slots(m,true,0))==3);
 assert(p::slots(m,true,1)==5&&p::cols(p::slots(m,true,1))==3&&p::rows(p::slots(m,true,1))==2);
 assert(p::PassengerCameoW==44&&p::PassengerCameoH==36);
 assert(p::PassengerCameoW<p::CellW&&p::PassengerCameoH<p::CellH);
 // Unit selection count >1 is a group list, never mistaken for a cargo list.
 m.selectedType(0x50000,1);
 assert(!p::expandable(m)&&p::entries(m,true)==m.groupCount);
 assert(p::slots(m,true,0)==m.groupCount&&p::pages(m,true)==1);
 m.reset(200,20);
 assert(!p::expandable(m)&&p::slots(m,false,0)==0);
 std::cout<<"PASS folded unit+HP only; expanded passenger pagination; 44x36 passenger Cameo; group list unaffected\n";
}
