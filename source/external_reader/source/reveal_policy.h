#pragma once
#include <cstdint>
namespace external { namespace reveal {
enum Option : uint8_t { Cloaked=1, Mirage=2, EnemySpy=4 };
// Presentation admission only. Callers still enforce alive/on-map/transport,
// current cell visibility, and immutable publication completeness.
struct Facts {
 int kind=-1,cloak=0;
 bool permanentInvisible=false,visible=true,disguised=false,enemy=false;
};
struct Decision {bool uncloak=false,trueForm=false;
 explicit operator bool()const{return uncloak||trueForm;}
};
inline Decision decide(uint8_t options,const Facts&f){
 Decision d;
 if(f.permanentInvisible||f.kind<0||f.kind>2)return d;
 d.uncloak=(options&Cloaked)&&!f.visible&&f.cloak>0;
 if(!f.visible&&!d.uncloak)return d;
 d.trueForm=f.disguised&&((f.kind==2&&(options&Mirage))||(f.kind==1&&f.enemy&&(options&EnemySpy)));
 return d;
}
}}
