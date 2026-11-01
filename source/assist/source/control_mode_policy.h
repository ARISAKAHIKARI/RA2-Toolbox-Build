#pragma once
#include <stdint.h>
namespace assist { namespace controlmode {
enum Mode : uint32_t { Off=0, Native=1, HouseFlag=2 };
constexpr bool native(uint32_t mode){return mode==Native;}
constexpr bool data(uint32_t mode){return mode==HouseFlag;}
constexpr bool valid(uint32_t mode){return mode<=HouseFlag;}
// The data-only experiment never admits foreign proxy orders or a native
// control adapter. Unknown mailbox values are not capabilities either.
constexpr bool proxyOrders(uint32_t mode){return native(mode);}
// Independent native sell/destruct retain their existing sender option when
// control is off. Selecting HouseFlag explicitly excludes that adaptation.
constexpr bool sender(uint32_t mode){return mode==Off||mode==Native;}
}}
