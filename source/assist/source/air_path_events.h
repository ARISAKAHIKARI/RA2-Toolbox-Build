#pragma once
#include <cstdint>
namespace assist { namespace airevents {
enum Status : int32_t { Idle=0, Recorded=1, Sending=2, Committed=3,
 Invalid=-4, Cancelled=-7, Full=-8, Partial=-16 };
struct Report {uint32_t match=0;int32_t status=0;uint32_t points=0,accepted=0,rejected=0;};
constexpr unsigned MaxActors=1024,MaxPoints=64;
}}
