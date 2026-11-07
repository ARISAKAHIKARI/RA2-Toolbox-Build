#pragma once
namespace cursorpresentation {
struct Delivery {bool native=false,blank=false,overlay=false;};
// A Windows HCURSOR always follows the OS point. A transformed game point
// therefore needs an overlay, without changing the saved display preference.
constexpr Delivery delivery(bool nativePreference,bool noCursor,bool mapped,bool hide,bool suspended){
 if(suspended)return {};
 if(noCursor)return {false,hide,false};
 if(mapped)return {false,hide,true};
 if(nativePreference&&hide)return {true,false,false};
 return {false,false,true};
}
}
