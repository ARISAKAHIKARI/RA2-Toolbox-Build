#pragma once
namespace assist { namespace unloadpolicy {
// An ejected passenger may briefly inherit an engine Move/Attack mission.
// For automatic ejection only, allow a short grace after landing, but never
// override explicit user Stop (13) or Enter (7).
constexpr int ScatterFrames=180;
// YRpp Sequence::Deploy=27; Deployed/DeployedFire/DeployedIdle=28..30.
inline bool completedDeploy(int sequence){return sequence>=28&&sequence<=30;}
inline bool deployObserved(int sequence){return sequence>=27&&sequence<=30;}
inline bool initialMayContinue(int mission,int mode,int landedAge,bool enteringOldCarrier=false,bool ejectDeployer=false){
 if(mission==2&&(mode==0||ejectDeployer&&mode!=0))return landedAge>=0&&landedAge<=ScatterFrames;
 if(mission==0||mission==5||mission==6||mission==11)return true;
 if(mission==7&&enteringOldCarrier)return true;
 if(mission==13||mission==7||mission==16)return false;
 return mode!=0&&landedAge>=0&&landedAge<=8;
}
inline bool shouldKeepWatching(bool feature,bool carrierAlive,bool alreadyTracked){
 return feature&&(carrierAlive||alreadyTracked);
}
} }
