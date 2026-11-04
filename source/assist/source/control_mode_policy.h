#pragma once
#include <stdint.h>
namespace assist { namespace controlmode {
enum Mode : uint32_t { Off=0, Native=1, Master=2, Parameters=3 };
constexpr int implementation(int value){return value==1||value==2?value:0;}
constexpr bool native(uint32_t mode){return mode==Native;}
constexpr bool master(uint32_t mode){return mode==Master;}
constexpr bool parameters(uint32_t mode){return mode==Parameters;}
constexpr bool valid(uint32_t mode){return mode<=Parameters;}
// The basic Master patch group never admits foreign proxy orders or a native
// control adapter. Unknown mailbox values are not capabilities either.
constexpr bool proxyOrders(uint32_t mode){return native(mode);}
// Independent native sell/destruct retain their existing sender option when
// control is off. Selecting Master explicitly excludes that adaptation.
constexpr bool sender(uint32_t mode){return mode==Off||mode==Native;}
}}
