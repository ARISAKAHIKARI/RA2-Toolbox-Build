#pragma once
#include "game_entry_api.h"
namespace gameentry {
// Internal Runtime call, not an exported ingress or a second permission owner.
// It checks the manager's current thread/module/client/LIFO token every time.
bool validateCurrent(Client client,uint32_t module,uint32_t token);
}
