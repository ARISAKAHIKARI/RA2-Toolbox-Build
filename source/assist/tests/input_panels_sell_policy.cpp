#include "panel_placement.h"
#include "trigger_input_policy.h"
#include "sell_packet.h"
#include "../../shared/source/hardware_slot_budget.h"
#include <cassert>
#include <iostream>
#include <iomanip>
#include <string>
using namespace assist;
int main(int argc,char**argv){
 for(int action=0;action<180;++action){
  assert(triggerinput::blocked(action,true,false)==(action==46));
  assert(triggerinput::blocked(action,false,true)==(action==48||action==49||action==112||action==145));
  assert(!triggerinput::blocked(action,false,false));
 }
 assert(slotbudget::allows(slotbudget::pack(1,3,0,slotbudget::Full|slotbudget::Map|slotbudget::FPS),slotbudget::Trigger));
 assert(!slotbudget::allows(slotbudget::pack(0,3,1,slotbudget::Full|slotbudget::Map|slotbudget::FPS),slotbudget::Trigger));
 assert(slotbudget::tags(slotbudget::pack(3,1,0,slotbudget::Trigger))==slotbudget::Trigger);
 using namespace panelplacement;
 for(int count=1;count<=256;++count)for(int limit=1;limit<=20;++limit)for(int available=1;available<=12;++available){
  int r=rows(count,limit,available);assert(r<=limit&&r<=available&&r<=(count+1)/2&&r>=1);
 }
 for(int scale:{75,100,150,200})for(auto dims:{Rect{0,0,1280,720},Rect{0,0,1920,1080},Rect{0,0,2560,1440}}){
  const int w=140*scale/100,h=rows(2,6,20)*65*scale/100+32*scale/100;
  Rect occupied{dims.w-400,dims.h/3,200,dims.h*2/3};
  Rect pref{occupied.x+20,occupied.y+10,w,h};auto placed=avoid(pref,occupied,dims.w,dims.h,4);
  assert(!overlaps(placed,occupied,4)&&placed.x>=0&&placed.y>=0&&placed.x+w<=dims.w&&placed.y+h<=dims.h);
  auto stable=avoid(placed,occupied,dims.w,dims.h,4);assert(stable.x==placed.x&&stable.y==placed.y);
 }
 sellpacket::Wire wire;assert(!sellpacket::encode(wire,3,0,1,100,15,5,1,0));
 assert(!sellpacket::encode(wire,2,0,1,101,15,5,1,0));assert(!sellpacket::encode(wire,1,32,1,100,15,5,1,0));
 assert(!sellpacket::encode(wire,1,0,0,100,15,5,1,0));assert(!sellpacket::encode(wire,1,0,1,INT32_MAX,15,5,1,0));
 for(int protocol:{0,1,2})for(int house:{0,1,7,31}){
  assert(sellpacket::encode(wire,protocol,house,0x1234,1230,15,5,0x12345678,65535));
  assert(wire.local.frame==1245&&wire.local.house==house&&wire.local.kind==0x16&&wire.local.payload[4]==0x34);
  assert(wire.size==(protocol?20:222));
  if(argc>1){std::cout<<std::dec<<protocol<<' '<<house<<' ';for(unsigned i=0;i<wire.size;++i)std::cout<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(wire.bytes[i]);std::cout<<'\n';}
 }
 if(argc==1)std::cout<<"PASS panel geometry (61440 cases), trigger filters, shared DR budget and Sell packet boundaries\n";
}
