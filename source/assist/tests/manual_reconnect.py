"""Exercise the actual reconnect action; retirement/Win32 are controlled seams."""
import argparse, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',required=True,type=Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
src=Path(__file__).resolve().parents[1]/'source';main=(src/'main.cpp').read_text();start=main.index('static void requestGameReconnect(){');end=main.index('// Allowlist/search edits',start)
code=r'''#include <cstdint>
#include <string>
#include <cassert>
#include <cstdio>
using DWORD=uint32_t;
static struct Shared {unsigned request=9,ack=8,stop=0;}storage,*shared=&storage;
static struct {int status=1;}incoming;
static struct Queue {unsigned size=3;void clear(){size=0;}}commandQueue,hotkeyPending,pointerMatches;
static bool enabled=true,bridgeBlocked=true,fresh=true;
static DWORD lastFind=0,lastProcessCheck=0,sentAt=1,inFlightMatch=1,bridgeAttempts=3,bridgeRetryAt=9999,pid=42,hook=1;
static std::wstring bridgeFailure=L"old error",status;
static unsigned registered=0,retired=0,repairs=0;
static DWORD GetTickCount(){return 10000;}
static bool commandFresh(DWORD){return fresh;}
static void resetConstruction(){}static void resetAirPath(){}
static void registerKeys(){++registered;}
static void releaseBridge(){++retired;shared->stop=1;shared=nullptr;}
static void critical(const std::wstring&){}static void setStatus(const std::wstring&s){status=s;}
namespace gameinput {static void repair(DWORD){++repairs;}}
'''+main[start:end]+r'''
int main(){
 requestGameReconnect();assert(registered==1&&retired==0&&repairs==0&&shared==&storage&&storage.request==9&&storage.ack==8);
 fresh=false;requestGameReconnect();assert(registered==2&&repairs==1&&retired==0&&storage.request==9&&storage.ack==8);
 incoming.status=-2;requestGameReconnect();assert(registered==3&&retired==1&&!shared&&storage.stop==1&&storage.request==9&&storage.ack==8);
 assert(!commandQueue.size&&!hotkeyPending.size&&!pointerMatches.size&&!sentAt&&!inFlightMatch&&!bridgeBlocked&&!bridgeAttempts&&!bridgeRetryAt&&bridgeFailure.empty());
 requestGameReconnect();assert(registered==4&&retired==1&&repairs==1);
 puts("PASS reconnect: healthy binding reset, stalled transport repair, fault retirement with unchanged request/ACK, and clean retry");
}
'''
cpp=a.out/'production.cpp';exe=a.out/'manual-reconnect';cpp.write_text(code)
subprocess.run(['g++','-std=c++17','-O1',str(cpp),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True);print(r.stdout,end='');(a.out/'result.json').write_text(json.dumps({'passed':r.returncode==0,'production_action':True,'windows_game_tested':False,'stdout':r.stdout,'stderr':r.stderr},indent=2)+'\n');raise SystemExit(r.returncode)
