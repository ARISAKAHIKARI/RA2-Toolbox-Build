#pragma once
#include "command_intent.h"
// Read/plan/entry boundary: a read-only snapshot enters here; no engine
// addresses or mutation allowed. The runtime must additionally check session,
// object ownership, signature, queue budget, hook state and game thread.
namespace assist { namespace entrygate {
struct Snapshot {
 uint32_t match=0, intentMatch=0;
 int32_t session=-1;
 bool active=false,chat=false,gameThread=false,queueRoom=false;
};
enum class Outcome:uint8_t { Pass, Stale, UnsupportedSession, NotReady, NoRoom };
inline Outcome admit(const Snapshot&s){
 if(!s.match||s.intentMatch!=s.match)return Outcome::Stale;
 if(s.session!=0&&s.session!=5&&s.session!=3&&s.session!=4)return Outcome::UnsupportedSession;
 if(!s.active||s.chat||!s.gameThread)return Outcome::NotReady;
 if(!s.queueRoom)return Outcome::NoRoom;
 return Outcome::Pass;
}
}} // assist::entrygate
