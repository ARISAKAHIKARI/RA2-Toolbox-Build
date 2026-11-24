#pragma once
#include <cstdint>
#include "../shared/source/runtime_seh_scope.h"

namespace assist { namespace crcshadowboundary {

enum class Result : uint8_t {
 Idle, Armed, Busy, IdentityUnavailable, ReadFailed, InvalidOriginal,
 IdentityChanged, AlreadyOriginal, Restored, Conflict, WriteFailed, VerifyFailed
};

template<class Identity>
struct Transaction {
 uint32_t house=0;
 Identity identity{};
 uint8_t original=0;
 bool armed=false;
 Result last=Result::Idle;
};

template<class Identity,class GetIdentity,class Read>
Result begin(Transaction<Identity>&tx,uint32_t house,GetIdentity getIdentity,Read read){
 if(tx.armed)return Result::Busy;
 Identity id{};
 if(!house||!getIdentity(house,&id))return Result::IdentityUnavailable;
 uint8_t before=0;
 if(!read(house+0x241,&before))return Result::ReadFailed;
 if(before>1)return Result::InvalidOriginal;
 tx.house=house;tx.identity=id;tx.original=before;tx.armed=true;tx.last=Result::Armed;
 return Result::Armed;
}

template<class Identity,class GetIdentity,class Read,class Write>
Result finish(Transaction<Identity>&tx,GetIdentity getIdentity,Read read,Write write){
 if(!tx.armed)return Result::Idle;
 Identity now{};
 if(!getIdentity(tx.house,&now)||!(now==tx.identity)){
  tx.armed=false;tx.last=Result::IdentityChanged;return tx.last;
 }
 uint8_t live=0;
 if(!read(tx.house+0x241,&live)){tx.last=Result::ReadFailed;return tx.last;}
 if(live==tx.original){
  tx.armed=false;tx.last=Result::AlreadyOriginal;return tx.last;
 }
 // Reveal is expected to set this flag to one. Any other value may be a
 // nested/natural write or an unknown mutation: never overwrite it.
 if(live!=1){tx.last=Result::Conflict;return tx.last;}
 if(!write(tx.house+0x241,tx.original)){tx.last=Result::WriteFailed;return tx.last;}
 uint8_t verify=0;
 if(!read(tx.house+0x241,&verify)||verify!=tx.original){
  tx.last=Result::VerifyFailed;return tx.last;
 }
 tx.armed=false;tx.last=Result::Restored;return tx.last;
}

template<class Identity,class GetIdentity,class Read,class Write,class Invoke>
Result run(Transaction<Identity>&tx,uint32_t house,GetIdentity getIdentity,
           Read read,Write write,Invoke&&invoke){
 const Result started=begin(tx,house,getIdentity,read);
 if(started!=Result::Armed)return started;
 // The Runtime implementation maps this to MSVC __try/__finally. Cleanup
 // therefore runs on normal return and when the native Reveal unwinds.
 runtimeunwind::run([&]{invoke();},[&]{finish(tx,getIdentity,read,write);});
 return tx.last;
}

}}
