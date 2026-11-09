#pragma once
// Callers supply the platform headers. A failed acquisition is never released;
// repeated state updates do not add nested process-wide precision requests.
namespace timerresolution {
class Lease {
 bool held_=false;
public:
 Lease()=default;Lease(const Lease&)=delete;Lease&operator=(const Lease&)=delete;
 ~Lease(){state(false);}
 void state(bool active){
  if(active&&!held_)held_=timeBeginPeriod(1)==TIMERR_NOERROR;
  else if(!active&&held_){timeEndPeriod(1);held_=false;}
 }
};
}
