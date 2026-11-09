#include <cstdint>
#include <cstdio>
#include <cstdlib>
using DWORD=uint32_t;
struct Display {int status=1;DWORD sampledTick=0;int renderStatus=1;};
struct State {int readStatus=0,renderStatus=0,status=1,chat=0;};
struct Result {int readStatus,renderStatus,mode;};
namespace v143 {
static State snap;static bool enabled=true;struct Extra {bool watchSecondary=false;} extra;static bool foregroundGame(){return true;}
static int desiredHotkeyMode(){
 if(!foregroundGame()||snap.renderStatus!=1||snap.chat)return 0;
 if(enabled&&snap.status==1)return 2;
 return !enabled&&extra.watchSecondary&&snap.readStatus==1?1:0;
}
Result probe(const Display&d,DWORD displayNow,bool externalSourceConfirmed){snap={};
snap.readStatus=d.status==1&&d.sampledTick&&DWORD(displayNow-d.sampledTick)>=2000?0:d.status;
const bool externalValid=d.status==1&&d.sampledTick&&
                           DWORD(displayNow-d.sampledTick)<2000;
 snap.renderStatus=externalValid&&d.renderStatus==1?1:0;
return {snap.readStatus,snap.renderStatus,desiredHotkeyMode()};}
}
namespace v1464 {
static State snap;static bool enabled=true;struct Extra {bool watchSecondary=false;} extra;static bool foregroundGame(){return true;}
static int desiredHotkeyMode(){
 if(!foregroundGame()||snap.renderStatus!=1||snap.chat)return 0;
 if(enabled&&snap.status==1)return 2;
 return !enabled&&extra.watchSecondary&&snap.readStatus==1?1:0;
}
Result probe(const Display&d,DWORD displayNow,bool externalSourceConfirmed){snap={};
snap.readStatus=d.status==1&&d.sampledTick&&DWORD(displayNow-d.sampledTick)>=2000&&!externalSourceConfirmed?0:d.status;
const bool externalValid=d.status==1&&d.sampledTick&&
                           (DWORD(displayNow-d.sampledTick)<2000||externalSourceConfirmed);
 snap.renderStatus=externalValid&&d.renderStatus==1?1:0;
return {snap.readStatus,snap.renderStatus,desiredHotkeyMode()};}
}
int main(){
{const Display d{1,1000u,1};const DWORD now=1000u;const auto r=v143::probe(d,now,false);
if(r.readStatus!=1||r.renderStatus!=1||r.mode!=2){std::fprintf(stderr,"FAIL old_host_clock_same_time\n");return 1;}
std::printf("{\"case\":\"old_host_clock_same_time\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1008u,1};const DWORD now=1000u;const auto r=v1464::probe(d,now,false);
if(r.readStatus!=0||r.renderStatus!=0||r.mode!=0){std::fprintf(stderr,"FAIL new_producer_ahead_8ms_unconfirmed\n");return 1;}
std::printf("{\"case\":\"new_producer_ahead_8ms_unconfirmed\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1008u,1};const DWORD now=1000u;const auto r=v1464::probe(d,now,true);
if(r.readStatus!=1||r.renderStatus!=1||r.mode!=2){std::fprintf(stderr,"FAIL new_producer_ahead_8ms_confirmed\n");return 1;}
std::printf("{\"case\":\"new_producer_ahead_8ms_confirmed\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1008u,1};const DWORD now=1007u;const auto r=v1464::probe(d,now,false);
if(r.readStatus!=0||r.renderStatus!=0||r.mode!=0){std::fprintf(stderr,"FAIL new_producer_ahead_1ms_unconfirmed\n");return 1;}
std::printf("{\"case\":\"new_producer_ahead_1ms_unconfirmed\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1008u,1};const DWORD now=1008u;const auto r=v1464::probe(d,now,false);
if(r.readStatus!=1||r.renderStatus!=1||r.mode!=2){std::fprintf(stderr,"FAIL new_clock_catches_up\n");return 1;}
std::printf("{\"case\":\"new_clock_catches_up\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1008u,1};const DWORD now=1024u;const auto r=v1464::probe(d,now,false);
if(r.readStatus!=1||r.renderStatus!=1||r.mode!=2){std::fprintf(stderr,"FAIL new_fresh_past_sample\n");return 1;}
std::printf("{\"case\":\"new_fresh_past_sample\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1000u,1};const DWORD now=3000u;const auto r=v1464::probe(d,now,false);
if(r.readStatus!=0||r.renderStatus!=0||r.mode!=0){std::fprintf(stderr,"FAIL new_true_expiry_2000ms\n");return 1;}
std::printf("{\"case\":\"new_true_expiry_2000ms\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1000u,1};const DWORD now=3000u;const auto r=v1464::probe(d,now,true);
if(r.readStatus!=1||r.renderStatus!=1||r.mode!=2){std::fprintf(stderr,"FAIL new_independently_confirmed_old_frame\n");return 1;}
std::printf("{\"case\":\"new_independently_confirmed_old_frame\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{0,1000u,1};const DWORD now=1008u;const auto r=v1464::probe(d,now,true);
if(r.readStatus!=0||r.renderStatus!=0||r.mode!=0){std::fprintf(stderr,"FAIL new_confirmed_invalid_display_sample\n");return 1;}
std::printf("{\"case\":\"new_confirmed_invalid_display_sample\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,1000u,0};const DWORD now=1008u;const auto r=v1464::probe(d,now,true);
if(r.readStatus!=1||r.renderStatus!=0||r.mode!=0){std::fprintf(stderr,"FAIL new_confirmed_hidden_display\n");return 1;}
std::printf("{\"case\":\"new_confirmed_hidden_display\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
{const Display d{1,4294967280u,1};const DWORD now=16u;const auto r=v1464::probe(d,now,false);
if(r.readStatus!=1||r.renderStatus!=1||r.mode!=2){std::fprintf(stderr,"FAIL new_uint32_wrap_fresh\n");return 1;}
std::printf("{\"case\":\"new_uint32_wrap_fresh\",\"unsigned_age\":%u,\"read_status\":%d,\"render_status\":%d,\"desired_hotkey_mode\":%d}\n",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}
return 0;}
