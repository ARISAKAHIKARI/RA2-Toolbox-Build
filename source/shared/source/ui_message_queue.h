#pragma once
#include <windows.h>
namespace uimessages {
// Unfiltered GetMessage prefers posted work to hardware input and paint. A
// high-rate producer can keep one message pending forever without growing its
// queue, yet still starve real controls. Give input/paint/timers bounded turns;
// also service ordinary messages after at most four priority turns.
class Queue {
 unsigned priority_=0,next_=0;
public:
 bool peek(MSG&message){
  if(priority_<4){
   const UINT masks[]={PM_QS_INPUT,PM_QS_PAINT,UINT(QS_TIMER)<<16};
   for(unsigned n=0;n<3;++n){const unsigned group=(next_+n)%3;
    if(PeekMessageW(&message,nullptr,0,0,PM_REMOVE|masks[group])){next_=(group+1)%3;++priority_;return true;}
   }
  }
  priority_=0;return PeekMessageW(&message,nullptr,0,0,PM_REMOVE)!=FALSE;
 }
 int get(MSG&message){return peek(message)?(message.message==WM_QUIT?0:1):int(GetMessageW(&message,nullptr,0,0));}
};
}
