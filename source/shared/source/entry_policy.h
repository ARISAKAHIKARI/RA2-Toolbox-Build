#pragma once
#include "game_entry_api.h"
namespace gameentry {
// Enter is non-reentrant scheduling. Capture is an explicit same-module
// nested callback lease; every level has a unique, LIFO-only release token.
struct Policy {
 static constexpr uint32_t MaxDepth=128;
 uint32_t thread=0,module=0,client=0,token=0,serial=0,boundClient=0,depth=0;
 uint32_t tokens[MaxDepth]{};
 uint32_t nextToken(){
  for(;;){uint32_t n=++serial;if(!n)continue;bool used=false;
   for(uint32_t i=0;i<depth;++i)if(tokens[i]==n)used=true;
   if(!used)return n;
  }
 }
 Result enter(uint32_t tid,Request&r,bool priorThreadDead=false){
  if(!tid||!r.module||uint32_t(r.client)<1||uint32_t(r.client)>3)return Result::Invalid;
  const bool capture=r.operation==Operation::Capture;
  if(capture&&r.bindThread)return Result::Invalid;
  if(thread&&(thread!=tid||priorThreadDead)){
   // No module may promote itself over another live thread binding.
   // A reused numeric TID is a new identity too: callbacks cannot revive it
   // without the explicit message-boundary bind used for a different TID.
   const bool canTakeOver=!module&&r.bindThread&&priorThreadDead;
   if(!canTakeOver)return Result::WrongThread;
  }
  if(!thread&&!r.bindThread)return Result::WrongThread;
  if(module){
   if(!capture)return Result::Busy;
   if(!owns(tid,r))return Result::WrongOwner;
   if(depth>=MaxDepth)return Result::Busy;
   token=nextToken();tokens[depth++]=token;r.token=token;return Result::Ok;
  }
  if(capture&&r.token)return Result::WrongOwner;
  if(r.bindThread&&(thread!=tid||r.client==Client::Assist||!boundClient))boundClient=uint32_t(r.client);
  thread=tid;module=r.module;client=uint32_t(r.client);
  token=nextToken();tokens[0]=token;depth=1;r.token=token;return Result::Ok;
 }
 bool owns(uint32_t tid,const Request&r)const{
  return module&&tid==thread&&r.module==module&&uint32_t(r.client)==client&&r.token==token;
 }
 bool sameModule(uint32_t tid,const Request&r)const{
  return module&&tid==thread&&r.module==module&&uint32_t(r.client)==client;
 }
 Result leave(uint32_t tid,Request&r){
  if(!owns(tid,r))return Result::WrongOwner;
  tokens[--depth]=0;
  if(depth){token=tokens[depth-1];r.token=token;return Result::Ok;}
  module=client=token=0;r.token=0;return Result::Ok;
 }
};
}
