#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/native_badge_display.h"
#include "../source/world_presentation.h"
#include "../source/sidebar_presentation.h"
#include "../../external_reader/source/passenger_bubbles.h"
#include "../../shared/source/overlay_composition.h"
#include "../../shared/source/overlay_visibility.h"
#include "../../suite/source/display_settings.h"
#include <cassert>
#include <cstdio>
#include <cstring>
namespace {
int checks=0;
#define CHECK(value) do{++checks;assert(value);}while(0)
std::vector<uint8_t> memory(0x1000000);uint32_t failure=0;size_t failureSize=0;
template<class T>void put(uint32_t at,T value){std::memcpy(memory.data()+at,&value,sizeof value);}
bool read(uint32_t at,void*out,size_t n){
 if(uint64_t(at)+n>memory.size()||(failureSize&&at<uint64_t(failure)+failureSize&&uint64_t(at)+n>failure))return false;
 std::memcpy(out,memory.data()+at,n);return true;
}
constexpr uint32_t Carrier=0x100000,Passenger=0x110000,Type=0x200000,PassengerType=0x210000,Owner=0x300000,House=0x310000;
void passengerSetup(){
 put(0xA8EC78+4,0x500000u);put(0xA8EC78+8,1);put(0xA8EC78+16,1);put(0x500000,Carrier);
 put(Carrier,0x7F5C70u);put(Carrier+0x6C,100);put(Carrier+0x74,uint8_t(1));put(Carrier+0x90,uint8_t(1));put(Carrier+0x10,42u);
 put(Carrier+0x21C,Owner);put(Owner+0x30,1);put(Carrier+0x6C4,Type);put(Carrier+0x431,uint8_t(1));
 put(Carrier+0x9C,1024);put(Carrier+0xA0,512);put(Carrier+0x114,1);put(Carrier+0x118,Passenger);
 put(Passenger,0x7EB058u);put(Passenger+0x6C,100);put(Passenger+0x11C,Carrier);put(Passenger+0x6C0,PassengerType);
 put(PassengerType+0x60,0x400000u);put(0x400000,uint16_t('G'));
}
constexpr uint32_t Strip=0x87F7E8+0x1544,Gadgets=0xB07E80;
void sidebarSetup(){
 put(0x87F7E8+0x539C,0);put(0x8A00A4,800);put(0x8A00A8,600);put(0x886F94,0);put(0x886F9C,600);put(0xB0B4F8,100);put(0xA8B230,0x600000u);
 put(Strip+0x1C,uint8_t(1));put(Strip+0x38,0);put(Strip+0x44,0);put(Strip+0x54,1);put(Strip+88,9);put(Strip+92,7);
 put(Gadgets+0xC,650);put(Gadgets+0x10,150);put(Gadgets+0x14,60);put(Gadgets+0x18,48);
}
}
int main(){
 passengerSetup();external::Projection projection{0,0,640,480};
 auto good=external::passengers::sampleChecked(read,House,projection,true,false);
 CHECK(good.complete&&good.bubbles.size()==1&&good.bubbles[0].groups[0].count==1);
 displaylease::Sample<int,std::vector<external::passengers::Bubble>> passengers;
 CHECK(passengers.observe(7,good.complete,good.bubbles,100)->size()==1);
 failure=Carrier+0x431;failureSize=1;
 auto broken=external::passengers::sampleChecked(read,House,projection,true,false);
 CHECK(!broken.complete&&broken.bubbles.empty());
 CHECK(passengers.observe(7,broken.complete,broken.bubbles,120)->size()==1);
 CHECK(!passengers.observe(7,broken.complete,broken.bubbles,220)); // failures do not renew
 failureSize=0;good=external::passengers::sampleChecked(read,House,projection,true,false);
 CHECK(passengers.observe(7,good.complete,good.bubbles,230)->size()==1);
 put(Carrier+0x431,uint8_t(0));auto absent=external::passengers::sampleChecked(read,House,projection,true,false);
 CHECK(absent.complete&&absent.bubbles.empty());CHECK(passengers.observe(7,true,absent.bubbles,240)->empty());
 put(Carrier+0x431,uint8_t(1));put(Carrier+0x114,0);put(Carrier+0x118,0u);
 absent=external::passengers::sampleChecked(read,House,projection,true,false);CHECK(absent.complete&&absent.bubbles.empty());
 put(Carrier+0x118,Passenger);broken=external::passengers::sampleChecked(read,House,projection,true,false);CHECK(!broken.complete);
 put(Carrier+0x114,1);put(Carrier+0x118,Passenger);put(Passenger+0x11C,0u);
 broken=external::passengers::sampleChecked(read,House,projection,true,false);CHECK(!broken.complete);
 put(Passenger+0x11C,Carrier);put(0xA8EC78+8,0);
 broken=external::passengers::sampleChecked(read,House,projection,true,false);CHECK(!broken.complete);
 put(0xA8EC78+8,1);CHECK(!passengers.observe(8,false,{},250)); // different world

 sidebarSetup();using namespace assist::construction;
 NativeDisplay display;NativeDisplayKey key{1,2,House,800,600};
 auto frame=display.sample(read,key,100);CHECK(frame&&frame->cells.size()==1&&frame->cells[0].index==9);
 failure=Gadgets;failureSize=1;frame=display.sample(read,key,116);CHECK(frame&&frame->cells.size()==1);
 NativeView input;CHECK(!readNativeView(read,input)&&input.count==0); // no input lease
 CHECK(!display.sample(read,key,220));failureSize=0;
 frame=display.sample(read,key,236);CHECK(frame&&frame->cells.size()==1);
 put(Strip+0x54,0);frame=display.sample(read,key,252);CHECK(frame&&frame->cells.empty());
 put(Strip+0x54,1);frame=display.sample(read,key,268);CHECK(frame&&frame->cells.size()==1);
 put(0x87F7E8+0x539C,2);CHECK(!display.sample(read,key,284));
 put(0x87F7E8+0x539C,0);CHECK(display.sample(read,key,300));
 failure=Gadgets;failureSize=1;auto next=key;next.match++;CHECK(!display.sample(read,next,316));
 failureSize=0;CHECK(display.sample(read,key,332));put(0x8A00A4,1024);CHECK(!display.sample(read,key,348));
 put(0x8A00A4,800);CHECK(display.sample(read,key,364));put(Strip+0x1C,uint8_t(0));CHECK(!display.sample(read,key,380));
 put(Strip+0x1C,uint8_t(1));CHECK(display.sample(read,key,396));display.clear();failure=Gadgets;failureSize=1;CHECK(!display.sample(read,key,412));

 worldpresentation::Submission submission;CHECK(submission.required(false));submission.submitted();CHECK(!submission.required(false));
 CHECK(submission.invalidate());CHECK(submission.required(false));submission.submitted();CHECK(submission.required(true));
 // Exercise the actual composition contract: a camera commit removes world
 // pixels but cannot remove fixed sidebar numbers while the next job waits.
 using namespace overlaycomposition;Context a{1,1,1,2,800,600,0,0},b=a;b.cameraX=64;
 auto pixels=std::make_shared<const std::vector<uint32_t>>(std::vector<uint32_t>(4,0xffffffff));
 Surface world{1,10,a,{10,10,2,2},true,255,pixels};auto badge=sidebarpresentation::surface(a,{650,182,2,2},pixels);
 CHECK(valid(world,a)&&valid(badge,a));CHECK(!valid(world,b)&&valid(badge,b));
 std::vector<uint32_t> composed;CHECK(compose(b,{world,badge},composed));CHECK(composed[182*800+650]==0xffffffff&&composed[10*800+10]==0);
 b.match++;CHECK(!valid(badge,b));
 sidebarpresentation::Identity identity;
 CHECK(identity.changed(1,0,800,600));CHECK(!identity.changed(1,0,800,600));
 CHECK(identity.changed(1,1,800,600));CHECK(!identity.changed(1,1,800,600));CHECK(identity.changed(1,1,1024,600));

 // Actual shared input policy: edge only, chat/focus disarm, rebinding while
 // held, and two module callers observing the same physical press.
 overlayvisibility::Key keyPolicy;
 CHECK(!keyPolicy.sample(0xC0,false,true,true));CHECK(keyPolicy.sample(0xC0,true,true,true));
 CHECK(!keyPolicy.sample(0xC0,true,true,true));CHECK(!keyPolicy.sample(0xC0,false,true,true));
 CHECK(!keyPolicy.sample(0xC0,true,true,false));CHECK(!keyPolicy.sample(0xC0,true,true,true));
 CHECK(!keyPolicy.sample(0xC0,true,false,true));CHECK(!keyPolicy.sample(0xC0,true,true,true));
 CHECK(!keyPolicy.sample(0xC0,false,true,true));CHECK(keyPolicy.sample(0xC0,true,true,true));
 CHECK(!keyPolicy.sample(0x240,true,true,true));CHECK(!keyPolicy.sample(0x240,false,true,true));CHECK(keyPolicy.sample(0x240,true,true,true));
 CHECK(!keyPolicy.sample(0,false,true,true));CHECK(!keyPolicy.sample(0,true,true,true));
 overlayvisibility::Dispatcher dispatcher;overlayvisibility::Context context{123,456};
 CHECK(!dispatcher.sample(1,context,0xC0,false,true,true,100));
 CHECK(!dispatcher.sample(2,context,0xC0,false,false,true,101)); // inactive peer cannot reset
 CHECK(dispatcher.sample(1,context,0xC0,true,true,true,102));
 CHECK(!dispatcher.sample(2,context,0xC0,true,true,true,103)); // one global toggle
 CHECK(!dispatcher.sample(1,context,0xC0,true,false,true,104));
 CHECK(!dispatcher.sample(2,context,0xC0,true,true,true,105)); // handoff while held
 CHECK(!dispatcher.sample(2,context,0xC0,false,true,true,106));
 CHECK(dispatcher.sample(2,context,0xC0,true,true,true,107));
 CHECK(!dispatcher.sample(1,context,0xC0,true,true,true,250)); // owner expired: still require release
 CHECK(!dispatcher.sample(1,context,0xC0,false,true,true,251));CHECK(dispatcher.sample(1,context,0xC0,true,true,true,252));
 context.window++;CHECK(!dispatcher.sample(1,context,0xC0,true,true,true,253));
 CHECK(!dispatcher.sample(1,context,0xC0,false,true,true,254));CHECK(dispatcher.sample(1,context,0xC0,true,true,true,255));
 CHECK(suite::displaySpecs[suite::OverlayToggleKey].id==9420&&suite::displaySpecs[suite::OverlayToggleKey].control==suite::DisplayControl::Hotkey);
 CHECK(suite::validDisplayEdit(suite::OverlayToggleKey,0xC0)&&suite::validDisplayEdit(suite::OverlayToggleKey,0)&&!suite::validDisplayEdit(suite::OverlayToggleKey,65536));
 auto revision=overlayvisibility::revision.load();bool shown=overlayvisibility::visible();overlayvisibility::toggle();
 CHECK(overlayvisibility::visible()!=shown&&overlayvisibility::revision.load()==revision+1);overlayvisibility::toggle();CHECK(overlayvisibility::visible()==shown);
 std::printf("overlay display continuity: %d checks passed\n",checks);
}
