#include "../source/offline_tool_policy.h"
#include "../../shared/source/command_cursor_hint.h"
#include <cassert>
#include <iostream>
using namespace assist::offlinetools;
int main(){
 assert(offline(0)&&offline(5)&&!offline(3)&&!offline(4)&&!offline(-1));
 uint8_t r[5]={1,0,0,0,0},s[5]={0,1,0,0,0},bad[5]={1,1,0,0,0},plan[5]={1,0,0,1,0};
 assert(mode(r)==Repair&&mode(s)==Sell&&mode(bad)==None&&mode(plan)==None);
 assert(repairTarget(50,100,false,false,true)&&repairTarget(50,100,false,true,false));
 assert(!repairTarget(50,100,false,true,true)&&!repairTarget(100,100,false,false,false));
 assert(repairTarget(50,100,true,false,false)&&!repairTarget(0,100,true,false,false));
 assert(sellTarget(true,true,false,true)&&!sellTarget(true,true,false,false)&&!sellTarget(false,true,false,true)&&!sellTarget(true,true,true,true));
 Point p{1,100,0x50000,7,Repair,8,10,20,800,600,40,50};Hint h{p,0x70000,12};
 assert(usable(h,p,150));p.x+=3;assert(!usable(h,p,150));p.x-=3;p.cameraX++;assert(!usable(h,p,150));p.cameraX--;assert(!usable(h,p,201));
 Tickets tickets;auto*t=tickets.admit(1,12,0x70000,Repair,false,8,100);assert(t);*t={12,0x70000,100,8,false,Repair};
 assert(!tickets.admit(1,12,0x70000,Repair,false,9,120));assert(tickets.admit(1,12,0x70000,Repair,true,9,120));
 t=tickets.admit(1,12,0x70000,Sell,false,9,120);assert(t);*t={12,0x70000,120,9,false,Sell};
 assert(!tickets.admit(1,12,0x70000,Sell,false,10,130));assert(tickets.admit(2,12,0x70000,Sell,false,1,140));
 commandcursor::Hint c{1,1,100,0,commandcursor::OfflineRepair,10,20};
 assert(commandcursor::definition(c.mode)==33&&commandcursor::matchesTool(c,35,10,20)&&!commandcursor::matchesTool(c,33,10,20)&&!commandcursor::matchesTool(c,35,15,20));
 c.mode=commandcursor::OfflineSell;assert(commandcursor::definition(c.mode)==31&&commandcursor::matchesTool(c,32,10,20));
 std::cout<<"PASS offline tool session, native mode, target, preview freshness, duplicate and cursor gates\n";
}
