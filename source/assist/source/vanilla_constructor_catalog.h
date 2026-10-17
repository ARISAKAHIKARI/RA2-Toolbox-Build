#pragma once
#include <stdint.h>
// Source: YRpp NetworkEvents.h, plus the two locally compared gamemd-spawn builds.
// The functions below FILL an EventClass; NONE append to OutList/DoList.
// Do not replace them with simulation/effect calls (Factory::StartProduction, SW::Launch).
namespace assist { namespace vanilla {
enum class Domain : uint8_t {
 Object, MegaMission, Production, SellCell, NoOptions, PlayerBased,
 Waypoints, Place, SuperWeaponPlace
};
enum class Exposure : uint8_t { SupportedSender, ProbeOnly };
// Canonical YRpp NetworkEvents (EventClass::Type byte in 111-byte packet).
namespace EventType {
 constexpr int32_t Empty = 0x00;
 constexpr int32_t PowerOn = 0x01;
 constexpr int32_t PowerOff = 0x02;
 constexpr int32_t Ally = 0x03;
 constexpr int32_t MegaMission = 0x04;
 constexpr int32_t MegaMissionF = 0x05;
 constexpr int32_t Idle = 0x06;
 constexpr int32_t Scatter = 0x07;
 constexpr int32_t Destruct = 0x08;
 constexpr int32_t Deploy = 0x09;
 constexpr int32_t Detonate = 0x0A;
 constexpr int32_t Place = 0x0B;
 constexpr int32_t Options = 0x0C;
 constexpr int32_t GameSpeed = 0x0D;
 constexpr int32_t Produce = 0x0E;
 constexpr int32_t Suspend = 0x0F;
 constexpr int32_t Abandon = 0x10;
 constexpr int32_t Primary = 0x11;
 constexpr int32_t SpecialPlace = 0x12;
 constexpr int32_t Exit = 0x13;
 constexpr int32_t Animation = 0x14;
 constexpr int32_t Repair = 0x15;
 constexpr int32_t Sell = 0x16;
 constexpr int32_t SellCell = 0x17;
 constexpr int32_t Special = 0x18;
 constexpr int32_t FrameSync = 0x19;
 constexpr int32_t Message = 0x1A;
 constexpr int32_t ResponseTime = 0x1B;
 constexpr int32_t FrameInfo = 0x1C;
 constexpr int32_t SaveGame = 0x1D;
 constexpr int32_t Archive = 0x1E;
 constexpr int32_t AddPlayer = 0x1F;
 constexpr int32_t Timing = 0x20;
 constexpr int32_t ProcessTime = 0x21;
 constexpr int32_t PageUser = 0x22;
 constexpr int32_t RemovePlayer = 0x23;
 constexpr int32_t LatencyFudge = 0x24;
 constexpr int32_t MegaFrameInfo = 0x25;
 constexpr int32_t PacketTiming = 0x26;
 constexpr int32_t AboutToExit = 0x27;
 constexpr int32_t FallbackHost = 0x28;
 constexpr int32_t AddressChange = 0x29;
 constexpr int32_t PlanConnect = 0x2A;
 constexpr int32_t PlanCommit = 0x2B;
 constexpr int32_t PlanNodeDelete = 0x2C;
 constexpr int32_t AllCheer = 0x2D;
 constexpr int32_t AbandonAll = 0x2E;
}
// YRpp TargetClass / NetID 1-byte RTTI tag values used in network events.
namespace TargetTag {
 constexpr uint8_t None = 0x00;
 constexpr uint8_t Cell = 0x0B;   // AbstractType::Cell (11)
 constexpr uint8_t Techno = 0x34; // Object/Techno network ID reference (52)
}
constexpr bool validTargetRef(int32_t id, uint8_t tag){
 if(tag==TargetTag::None)return id==0;
 if(tag==TargetTag::Cell||tag==TargetTag::Techno)return id>0;
 return false;
}
constexpr uint32_t address(Domain d){
 switch(d){
 case Domain::Object: return 0x4C65E0;
 case Domain::MegaMission: return 0x4C6860;
 case Domain::Production: return 0x4C6970;
 case Domain::SellCell: return 0x4C6650;
 case Domain::NoOptions: return 0x4C66C0;
 case Domain::PlayerBased: return 0x4C6720;
 case Domain::Waypoints: return 0x4C6780;
 case Domain::Place: return 0x4C6AE0;
 case Domain::SuperWeaponPlace: return 0x4C6B60;
 }return 0;
}
constexpr Exposure exposure(Domain d){
 return d==Domain::Object || d==Domain::MegaMission || d==Domain::Production ||
        d==Domain::SuperWeaponPlace ? Exposure::SupportedSender : Exposure::ProbeOnly;
}
// 111-byte vanilla event, same as YRpp NetworkEvent; avoid relying on packing
// defaults (NetworkEvents is a byte, frame is offset 3).
#pragma pack(push,1)
struct Packet { uint8_t kind,executed;char house;uint32_t frame;uint8_t payload[104]; };
#pragma pack(pop)
static_assert(sizeof(Packet)==111,"NetworkEvent ABI must be 111 bytes");
struct Cell {int16_t x=0,y=0;};
static_assert(sizeof(Cell)==4,"YRpp CellStruct uses two signed shorts");
constexpr bool validCell(const Cell& c){return c.x>=0&&c.x<512&&c.y>=0&&c.y<512;}
}} // assist::vanilla
