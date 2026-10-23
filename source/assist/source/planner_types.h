#pragma once
#include <stdint.h>
namespace assist { namespace planner {
constexpr int MaxActors=8192,MaxIntents=2048,MaxRefineries=128,MaxOrders=24,MaxWaitingCells=40,MaxCombatProbes=8192;
enum ActorFlags { Own=1,Enemy=2,Selected=4,Disabled=8,Harvester=16,Unloading=32,Transported=64,Cloaked=128,Limbo=256,OffMap=512,Airborne=1024,Manipulated=2048,Organic=4096,Protected=8192,PsionicImmune=16384,PsychicImmune=32768,FixedWeapon=65536,StagedWeapon=131072,Controlled=262144,TemporalTarget=524288,TemporalImmune=1048576,ParasiteImmune=2097152,Infested=4194304,OutOfAmmo=8388608,BurstFiring=16777216,SustainedFire=33554432,OrePaused=67108864, PointDefense=134217728, Saboteur=268435456, DropHazard=536870912 };
enum WeaponFlags { GroundWeapon=1,AirWeapon=2,MagneticWeapon=4,LocomotorWeapon=8,MindControlWeapon=16,PsychicWeapon=32,TemporalWeapon=64,ParasiteWeapon=128,HealingWeapon=256,EffectivenessKnown=512 };
struct Weapon {int range,minimum;uint32_t flags,armorMask,selectMask;int slot;float effectiveness[11]{};};
struct Actor {
 uint32_t id,type,flags,manualVersion,target,destination,destinationCell;
 int kind,x,y,z,mission,queued,cost,storageLimit,range;
 float veterancy,storage[4];uint32_t dockTypes[8],radioIDs[4];
 int armor,rearmStart,rearmDuration,airRangeBonus;uint32_t liftTarget;Weapon weapons[2];int combatFirst=-1,combatCount=0;
};
enum IntentMode { OrdinaryMove=0,MagnetronMove=1 };
struct Intent {uint32_t id,cell,token,mode;};
struct WaitingCell {uint32_t cell,occupation;int land;};
struct Refinery {uint32_t id;int cellX,cellY,dockStyleA,dockStyleB,count;WaitingCell waiting[MaxWaitingCells];};
enum CombatProbeFlags { InWeaponRange=1, InAcquisitionRange=2 };
struct CombatProbe {uint32_t target,flags;int slot;float effectiveness;};
struct World {
 uint32_t match,tick;int frame,count,intentCount,refineryCount;uint32_t inputSerial;
 Actor actors[MaxActors];Intent intents[MaxIntents];Refinery refineries[MaxRefineries];int combatCount=0;CombatProbe combat[MaxCombatProbes];
};
inline bool validWorldCounts(int actors,int intents,int refineries){return actors>=0&&actors<=MaxActors&&intents>=0&&intents<=MaxIntents&&refineries>=0&&refineries<=MaxRefineries;}
inline bool validWorld(const World&w){if(w.combatCount<0||w.combatCount>MaxCombatProbes||w.frame<0||!validWorldCounts(w.count,w.intentCount,w.refineryCount))return false;for(int i=0;i<w.refineryCount;++i)if(w.refineries[i].count<0||w.refineries[i].count>MaxWaitingCells)return false;return true;}
// OreYield is a guarded native Move through the existing plan mailbox.
enum OrderPurpose { OreOrder=0, CombatOrder=1, OreRelease=2, MagnetronOrder=3, OreYield=4, DefenseOrder=5, MagnetronDropOrder=6 };
struct Order {
 uint32_t unit,token,target,destination;int mission,destinationTag,purpose;uint32_t refinery;uint32_t pursuit=0;
};
}}
