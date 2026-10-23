#pragma once
#include <stdint.h>
// Conservative, game-thread command preflight shared by all original EventClass
// writers. Input validation is not evidence of game acceptance or execution.
namespace assist { namespace orderpolicy {
enum class Reject : uint8_t { None, NoActor, ForeignActor, DeadActor, BadTarget, BadDestination, InvalidEvent, UnauthorizedOwner, UnsupportedOrder };
inline Reject mission(bool actorFound, bool actorOwned, bool actorAlive, int mission,
                      bool targetSpecified, bool targetFound, bool destIsObject, bool destFound) {
 if(!actorFound)return Reject::NoActor;
 if(!actorOwned)return Reject::ForeignActor;
 if(!actorAlive)return Reject::DeadActor;
 // 1=Attack, 7=Enter, 9=Capture / special Enter; other supported game
 // missions can be validated by feature-specific planners upstream.
 if(mission==1 && !targetSpecified)return Reject::BadTarget;
 if(targetSpecified&&!targetFound)return Reject::BadTarget;
 if(destIsObject&&!destFound)return Reject::BadDestination;
 return Reject::None;
}
// Unified mission preflight supporting both object targets (tag 0x34) and
// ground-cell targets (tag 11 = TargetTag::Cell, e.g. DiskPrimary force-fire).
inline Reject missionWithTargetTag(bool actorFound, bool actorOwned, bool actorAlive, int missionId,
                                   uint8_t targetTag, uint32_t target, bool targetObjectFound,
                                   bool targetCellValid, bool destIsObject, bool destFound) {
 const bool targetSpecified = (target != 0 && targetTag != 0);
 const bool targetFound = !targetSpecified ? true :
                          (targetTag == 11 ? targetCellValid :
                           targetTag == 0x34 ? targetObjectFound : false);
 return orderpolicy::mission(actorFound, actorOwned, actorAlive, missionId, targetSpecified, targetFound, destIsObject, destFound);
}
inline Reject objectEvent(int eventType, bool targetFound, bool targetAlive,
                          bool senderIsLocal, bool targetOwnerIsSender, bool multiplayer) {
 if(eventType!=0x06&&eventType!=0x07&&eventType!=0x15&&eventType!=0x16)return Reject::InvalidEvent;
 if(!targetFound||!targetAlive)return Reject::NoActor;
 if(eventType==0x06||eventType==0x07||eventType==0x15)return (senderIsLocal&&targetOwnerIsSender)?Reject::None:Reject::UnauthorizedOwner;
 // Selling another player's object has no validated multiplayer semantics.
 if(multiplayer && (!senderIsLocal || !targetOwnerIsSender))return Reject::UnauthorizedOwner;
 if(!targetOwnerIsSender)return Reject::UnauthorizedOwner;
 return Reject::None;
}
inline Reject productionEvent(bool validPlan, bool multiplayer, bool unsafeTechAllowed,bool nativeCameo=false) {
 if(multiplayer && !unsafeTechAllowed&&!nativeCameo)return Reject::UnauthorizedOwner;
 if(!validPlan)return Reject::UnsupportedOrder;
 return Reject::None;
}
inline Reject superWeaponEvent(bool swFound, bool swOwned, bool swReady, int actionType) {
 if(!swFound)return Reject::NoActor;
 if(!swOwned)return Reject::ForeignActor;
 if(!swReady)return Reject::InvalidEvent;
 if(actionType!=0)return Reject::UnsupportedOrder;
 return Reject::None;
}
} }
