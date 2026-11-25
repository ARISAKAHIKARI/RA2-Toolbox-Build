#include <cassert>
#include "../../shared/source/command_cursor_hint.h"
extern void publishFromTacticalTU();
int main(){publishFromTacticalTU();assert(commandcursor::read(42,1001).mode==commandcursor::Advanced);commandcursor::clear();assert(commandcursor::read(42,1001).mode==commandcursor::None);
 assert(commandcursor::animationTransition(53,5,53,54,450));
 assert(commandcursor::animationTransition(53,5,57,53,450));
 assert(commandcursor::animationTransition(89,10,97,98,517));
 assert(commandcursor::animationTransition(404,9,411,412,450));
 assert(commandcursor::animationTransition(404,9,412,404,450));
 assert(!commandcursor::animationTransition(53,5,52,53,450));
 assert(!commandcursor::animationTransition(53,5,53,55,450));
 assert(!commandcursor::animationTransition(53,5,57,58,450));
 assert(!commandcursor::animationTransition(404,9,404,405,412));
 assert(!commandcursor::animationTransition(53,1,53,53,450));}
