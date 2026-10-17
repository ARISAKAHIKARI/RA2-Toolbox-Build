#include "../source/ore_type_config.h"
#include <cassert>
#include <iostream>
using namespace assist;
int main(){oreconfig::Config c;assert(oreconfig::valid(c)&&oreconfig::allowed(c,true,"ANY"));
 assert(oreconfig::parse(L"harv, CMIN；harv",c.miners,c.minerCount));assert(c.minerCount==2);c.customMiners=1;
 assert(oreconfig::allowed(c,true,"HARV")&&oreconfig::allowed(c,true,"cmin")&&!oreconfig::allowed(c,true,"HTNK"));assert(oreconfig::allowed(c,false,"MODREF"));
 auto prior=c;assert(!oreconfig::parse(L"HARV,invalid!",c.miners,c.minerCount));assert(!memcmp(&c,&prior,sizeof c));
 assert(!oreconfig::parse(std::wstring(24,L'A'),c.miners,c.minerCount));
 assert(oreconfig::parse(L"",c.miners,c.minerCount)&&!oreconfig::allowed(c,true,"HARV"));
 c.customRefineries=1;assert(oreconfig::parse(L"GAREFN,NAREFN",c.refineries,c.refineryCount));assert(oreconfig::allowed(c,false,"GAREFN")&&!oreconfig::allowed(c,false,"GAPOWR"));
 c.refineryCount=513;assert(!oreconfig::valid(c));std::cout<<"PASS: defaults, independent miner/refinery lists, empty whitelist, normalization, malformed/over-limit rejection\n";
}
