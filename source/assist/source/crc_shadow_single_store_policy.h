#pragma once
#include <cstdint>

namespace assist { namespace crcshadowsingle {

enum class Result : uint8_t {
 Idle, Armed, Busy, NativeStore, SuppressStore, IdentityChanged
};

struct Scope {
 uint32_t epoch=0,house=0,houseID=0;
 bool active=false,faulted=false;
 unsigned suppressed=0;
};

inline Result begin(Scope&scope,uint32_t epoch,uint32_t house,uint32_t houseID){
 if(scope.active)return Result::Busy;
 if(!epoch||house<0x10000||!houseID)return Result::Idle;
 scope={epoch,house,houseID,true,false,0};
 return Result::Armed;
}

inline Result beforeStore(Scope&scope,uint32_t epoch,uint32_t currentHouse,uint32_t currentHouseID){
 if(!scope.active)return Result::NativeStore;
 if(currentHouse!=scope.house)return Result::NativeStore;
 if(epoch!=scope.epoch||currentHouseID!=scope.houseID){
  scope.faulted=true;return Result::IdentityChanged;
 }
 ++scope.suppressed;
 return Result::SuppressStore;
}

inline bool abort(Scope&scope,uint32_t currentHouse){
 if(!scope.active||currentHouse!=scope.house)return false;
 scope.faulted=true;return true;
}

inline bool consumeSkip(bool&pending){
 const bool skip=pending;pending=false;return skip;
}

inline void end(Scope&scope){
 scope.active=false;
 scope.epoch=scope.house=scope.houseID=0;
 scope.faulted=false;scope.suppressed=0;
}

}}
