#pragma once
#include <cstdint>
namespace external { namespace display {
struct State {
 bool readable=false;uint8_t tactical=0;int32_t specialDialog=0,dialogCount=0;
 bool visible()const{return readable&&tactical==1&&specialDialog==0&&dialogCount==0;}
};
// YRpp Unsorted: TacticalActive, SpecialDialog, WSDialogCount. Confirmed
// against original dialog entry/exit and drawing paths. No frame-stall guess.
template<class Read> State inspect(Read read){
 State s;s.readable=read(0xA8E378,&s.tactical,sizeof s.tactical);
 s.readable=read(0xA8EDA0,&s.specialDialog,sizeof s.specialDialog)&&s.readable;
 s.readable=read(0xA8ED8C,&s.dialogCount,sizeof s.dialogCount)&&s.readable;return s;
}
}}
