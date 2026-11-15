#include "../../external_reader/source/unit_info_panel_model.h"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace external::unitpanel;
int main(){
 Model m; m.reset(19,100);
 assert(m.match==19&&m.frame==100&&m.selected==0&&m.visualSignature());
 auto*g=m.selectedType(0x00100000,2);
 assert(g&&g->count==1);g->name[0]='T';g->name[1]='K';
 assert(!m.selectedType(0x00100000,2));
 assert(m.selected==2&&m.groupCount==1&&m.groups[0].count==2);
 auto stamp=m.visualSignature();m.frame=101;
 assert(m.visualSignature()==stamp); // a new game frame alone cannot force paint
 m.groups[0].count=3;assert(m.visualSignature()!=stamp);
 for(unsigned i=0;i<MaxGroups+7;++i)m.selectedType(0x00200000+i,1);
 assert(m.groupCount==int(MaxGroups)&&m.groupsTruncated&&m.selected==int(MaxGroups+9));
 assert(pageCount(0,8)==1&&pageCount(1,8)==1&&pageCount(9,8)==2);
 assert(clampPage(100,9,8)==1&&clampPage(-3,9,8)==0);
 assert(validCargoCount(0,0,true)&&validCargoCount(3,3,true));
 assert(!validCargoCount(4,3,true)&&!validCargoCount(3,3,false));
 assert(!validCargoCount(-1,0,true)&&!validCargoCount(3,4,true));
 m.reset(20,1);assert(m.selected==0&&m.groupCount==0&&m.passengersComplete);
 Unit passenger{};passenger.id=82;passenger.type=0xC0000;passenger.kind=1;
 for(unsigned i=0;i<MaxPassengers;++i)m.appendPassenger(passenger);
 assert(m.passengerCount==int(MaxPassengers)&&m.passengersComplete);
 m.appendPassenger(passenger);assert(m.passengerCount==int(MaxPassengers)&&!m.passengersComplete);
 auto stamp2=m.visualSignature();m.passenger[0].health=20;
 // A change in the selected single unit's cargo state changes the picture.
 m.selectedType(0x00BAD000,2);m.single.id=4;m.single.passengers=49;
 assert(m.visualSignature()!=stamp2);
 std::cout<<"unit-panel: grouping, bounded cargo, visual signature, pagination PASS\n";
}
