#include "hook_registry.h"
#include "../../shared/source/game_entry_client.h"
// Compatibility view only; the registry itself lives in the common DLL.
struct ProcessHookRegistry {
 bool active(uint32_t at)const{return gameentry::inspect(at).active!=0;}
 bool conflict(uint32_t at)const{return gameentry::inspect(at).conflict!=0;}
 unsigned conflicts()const{return gameentry::inspect().conflicts;}
 unsigned installed()const{return gameentry::inspect().installed;}
 void markConflict(uint32_t at){gameentry::markConflict(at);}
};
static ProcessHookRegistry codeHookRegistry;
