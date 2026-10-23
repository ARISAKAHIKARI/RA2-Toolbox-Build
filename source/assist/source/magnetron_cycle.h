#pragma once
#include <algorithm>
namespace assist { namespace magnetcycle {
// Local planning policy only.  Never modifies a weapon, timer or game mission.
inline bool shotObserved(int frame, int sentFrame, int readyRemaining,
                         int rearmStart, int shotStart, bool liftedOrManipulated) {
 if(frame<=sentFrame)return false;
 // A nonzero cooldown may have preceded the order. It is NOT evidence that
 // this order fired. Require a fresh rearm timestamp or a lift/manipulation.
 (void)readyRemaining;
 return (rearmStart!=shotStart && rearmStart>=sentFrame && rearmStart<=frame) || liftedOrManipulated;
}
inline int avoidUntil(int frame,int rearmDuration){
 // At least one full cycle, bounded so an idle target can eventually be retried.
 return frame+std::clamp(rearmDuration,30,120);
}
// Fall back if a native command was consumed without visible fire.  This is
// a retry bound, not an invented shot: it must not mark a target as fired.
inline bool shotStalled(int frame,int sentFrame,int timeout=45){return frame>=sentFrame&&frame-sentFrame>=timeout;}
} }
