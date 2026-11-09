#include "../../external_reader/source/idle_aircraft.h"
#include "../source/protocol.h"
#include <algorithm>
#include <cassert>
#include <iostream>
static uint32_t now=1000,pid=1,match=3;static bool enabled=true,focused=true,fresh=true;
static assist::Snapshot snap{};static const assist::Snapshot*externalData=&snap;
static struct {uint32_t idleAircraftTick=1000;int idleAircraftCount=2;uint32_t idleAircraftIDs[8192]{7,9};}externalSampler;
static unsigned sent=0;static uint32_t selected=0;
static bool foregroundGame(){return focused;}static bool commandFresh(uint32_t){return fresh;}
static uint32_t GetTickCount(){return now;}static uint32_t sampleRenderClock(void*){return now;}static uint32_t commandMatch(){return match;}
static void sendSelection(int kind,bool,const void*,uint32_t id){assert(kind==-4);++sent;selected=id;}
#include "../source/idle_aircraft_selection.h"
int main(){
 snap.renderStatus=1;external::aircraft::Sample display{3,1000,10,100,2,2};
 selectNextIdleAircraft();assert(sent==1&&selected==7);now=1500;selectNextIdleAircraft();assert(sent==2&&selected==9);
 now=1501;assert(display.current(3,100,10,now,true));selectNextIdleAircraft();assert(sent==2);
 now=60000;assert(display.current(3,100,10,now,true));selectNextIdleAircraft();assert(sent==2);
 externalSampler.idleAircraftTick=now;selectNextIdleAircraft();assert(sent==3&&selected==7);
 fresh=false;selectNextIdleAircraft();assert(sent==3);fresh=true;focused=false;selectNextIdleAircraft();assert(sent==3);
 std::cout<<"PASS exact production aircraft selection: 500ms boundary unchanged; paused display lease never authorizes expired command; connection/focus gates retained\n";
}
