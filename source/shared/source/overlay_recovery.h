#pragma once
#include <cstdint>
namespace overlayui {
// Windows can retain WS_VISIBLE after a display-mode switch while losing the
// layered surface or its position in the topmost band. Re-submit periodically;
// never manufacture a user's request to show a hidden panel.
struct Context {
 uintptr_t window=0;uint32_t process=0,match=0;int width=0,height=0;
 bool operator==(const Context&o)const{return window==o.window&&process==o.process&&match==o.match&&width==o.width&&height==o.height;}
};
class Recovery {
 Context context_{};uint32_t attempted_=0;bool active_=false,failed_=false;
public:
 void suspend(){active_=false;failed_=false;}
 bool due(uint32_t now,bool requested,const Context&context,bool visible){
  if(!requested){suspend();return false;}
  bool changed=!active_||!(context==context_);
  uint32_t interval=failed_||!visible?250u:1000u;
  if(!changed&&uint32_t(now-attempted_)<interval)return false;
  active_=true;context_=context;attempted_=now;return true;
 }
 void submitted(bool success){failed_=!success;}
};
}
