#pragma once
#include <cstdint>
namespace assist { namespace construction {
// Host authorizes a short-lived native-sidebar reservation context.
struct InputPolicy {uint32_t match=0,house=0,parent=0,parentID=0,tick=0;int tab=-1;uint32_t slots=0;int count=0,queued[5]{};};
// Runtime produces one immutable click; host owns consumed, not a command ACK.
struct InputClick {uint32_t tick=0,match=0,house=0,parent=0,parentID=0;int tab=-1,index=-1;uint32_t operation=0,all=0;};
}}
