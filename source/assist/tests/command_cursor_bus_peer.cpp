#include <cassert>
#include "../../shared/source/command_cursor_hint.h"
extern void publishFromTacticalTU();
int main(){publishFromTacticalTU();assert(commandcursor::read(42,1001).mode==commandcursor::Advanced);commandcursor::clear();assert(commandcursor::read(42,1001).mode==commandcursor::None);}
