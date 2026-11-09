#include "../source/protocol.h"
#include <cstring>
#include <iostream>
#include <memory>

using namespace assist;
static unsigned checks=0,cases=0,failures=0,barriers=0,cancels=0,retires=0;
static void check(bool value,const char*name){++checks;if(!value){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
static void barrier(){++barriers;}
static auto state=std::make_unique<Shared>();
static readonly::RuntimeState runtime;
static void fresh(){state=std::make_unique<Shared>();runtime={};cancels=retires=barriers=0;++cases;}
static auto service(bool retired){return runtime.service(*state,[]{++cancels;readonly::cancelMailboxes(*state,barrier);},[&]{++retires;return retired;},barrier);}
static void submit(){
 auto&s=*state;s.request=11;s.selectRequest=12;s.planRequest=13;s.cooperativePlan=14;
 s.cooperativeCount=3;s.cooperativeAccepted=2;s.cooperativeProcessed=2;s.cooperativeAcceptedBits[0]=3;
 s.cooperativeRequest.serial=31;s.cooperativeRequest.match=9;s.cooperativeRequest.count=2;
 s.pointer.request=15;s.pointer.intent.count=4;s.productionRequest=16;s.uiSoundRequest=17;
 s.formationKey=18;s.formationConsumedUntil=12345;s.reserveInput=19;s.cameraSeq=20;
}
static void cancelled(){
 const auto&s=*state;
 check(s.ack==11&&s.result==-7&&s.count==0,"command is terminal cancelled");
 check(s.selectAck==12&&s.selectResult==-7&&s.selectCount==0,"selection is terminal cancelled");
 check(s.planAck==13&&s.planResult==-7&&s.planAccepted==0,"planner is terminal cancelled");
 check(s.cooperativeAck==14&&s.cooperativeResult==-7&&s.cooperativeAccepted==0&&s.cooperativeProcessed==0&&s.cooperativeAcceptedBits[0]==0,"cooperative is terminal cancelled");
 check(s.cooperativeRejected==3,"unconsumed cooperative count rejected");
 check(!s.cooperativeRequest.serial&&!s.cooperativeRequest.match&&!s.cooperativeRequest.count&&!(s.cooperativeSeq&1)&&s.runtimeCoopDone==31,"shared cooperative intent retired under publication fence");
 check(s.pointer.ack==15&&s.pointer.result==-7&&s.pointer.accepted==0&&s.pointer.rejected==4,"pointer is terminal cancelled");
 check(s.productionAck==16&&s.uiSoundAck==17,"production and sound observations drained");
 check(s.formationKeyAck==18&&s.formationConsumedUntil==0,"formation input ownership released");
 check(s.reserveConsumed==19&&s.cameraAck==20,"reservation and stable camera requests drained");
}
int main(){
 fresh();check(readonly::commands(state.get()),"normal default remains command capable");
 check(service(true)==readonly::Admission::Commands&&cancels==0&&retires==0,"normal mode does not reset features");

 fresh();readonly::request(*state,barrier);
 check(state->readOnly==1&&state->readOnlyRequest==1&&state->readOnlyAck==0,"pause publishes an unacknowledged generation");
 check(!readonly::commands(state.get())&&!readonly::settled(*state),"pause blocks immediately before Runtime callback");
 readonly::request(*state,barrier);check(state->readOnlyRequest==1,"maintenance does not allocate repeated requests");
 submit();check(service(false)==readonly::Admission::Retiring,"incomplete retirement does not admit observation or commands");
 check(runtime.active&&runtime.pending&&state->readOnlyAck==0,"retirement remains pending without ACK");cancelled();
 check(service(false)==readonly::Admission::Retiring&&retires==2,"deferred restoration retries");
 check(!readonly::settled(*state),"Host cannot resume before restoration ACK");
 check(service(true)==readonly::Admission::Observe,"completed retirement keeps read-only mode");
 check(state->readOnlyAck==1&&!runtime.pending&&!readonly::commands(state.get()),"ACK only grants observation");

 fresh();readonly::request(*state,barrier);check(service(true)==readonly::Admission::Observe,"cold readonly starts after cleanup");
 submit();check(service(true)==readonly::Admission::Observe&&retires==1,"late requests cancelled without resetting world producer");cancelled();
 state->readOnly=0;check(service(true)==readonly::Admission::Commands&&!runtime.active,"resume restores ordinary admission after ACK");
 check(readonly::commands(state.get()),"resume opens Host command gate");cancelled();

 fresh();readonly::request(*state,barrier);state->readOnly=0;
 check(!readonly::commands(state.get()),"clearing boolean cannot erase unacknowledged pause");
 check(service(false)==readonly::Admission::Retiring&&state->readOnlyAck==0,"quick off/on still retires");
 submit();check(service(true)==readonly::Admission::Commands&&state->readOnlyAck==1,"quick off/on resumes only after cleanup");cancelled();

 fresh();readonly::request(*state,barrier);service(true);state->readOnly=0;
 readonly::request(*state,barrier);
 check(state->readOnlyRequest==2&&state->readOnlyAck==1,"second pause cannot reuse previous ACK");
 check(service(false)==readonly::Admission::Retiring&&state->readOnlyAck==1,"new generation forces a fresh cleanup");
 check(service(true)==readonly::Admission::Observe&&state->readOnlyAck==2,"second generation ACK advances after retirement");

 fresh();state->readOnlyRequest=state->readOnlyAck=0xffffffffu;
 readonly::request(*state,barrier);check(state->readOnlyRequest==1,"request wrap skips zero sentinel");
 check(service(true)==readonly::Admission::Observe&&state->readOnlyAck==1,"wrapped request still observes before resume");

 fresh();readonly::request(*state,barrier);
 const auto raced=runtime.service(*state,[]{readonly::cancelMailboxes(*state,barrier);},[]{state->readOnlyRequest=2;return true;},barrier);
 check(raced==readonly::Admission::Observe&&state->readOnlyAck==1&&!readonly::settled(*state),"new request during retirement cannot receive old ACK");
 check(service(false)==readonly::Admission::Retiring&&state->readOnlyAck==1,"next callback retires new generation");

 fresh();state->cameraSeq=21;state->cameraAck=20;
 readonly::cancelMailboxes(*state,barrier);check(state->cameraAck==20,"incomplete camera publication is not acknowledged");
 state->cameraSeq=22;readonly::cancelMailboxes(*state,barrier);check(state->cameraAck==22,"completed camera publication is drained");
 check(!readonly::commands<Shared>(nullptr),"missing transport never admits commands");

 std::cout<<"{\"suite\":\"read_only_lifecycle\",\"passed\":"<<(failures?"false":"true")
 <<",\"scenarios\":"<<cases<<",\"checks\":"<<checks<<",\"failures\":"<<failures
 <<",\"scope\":\"production read-only admission and mailbox cancellation with actual Shared layout; no Windows/game execution\"}\n";
 return failures?1:0;
}
