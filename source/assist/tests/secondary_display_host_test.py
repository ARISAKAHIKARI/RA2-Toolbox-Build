#!/usr/bin/env python3
"""Exercise production assist display-only scheduling and pin-key dispatch.

Only Win32/UI/reader boundaries are faked. These checks validate lifecycle and
command isolation, not physical monitor composition or a live game session.
"""
from pathlib import Path
import subprocess
import tempfile
import json

root=Path(__file__).resolve().parents[2]
source=(root/'assist/source/main.cpp').read_text()
def function(signature):
    start=source.index(signature)
    while source.find(';',start)<source.index('{',start):
        start=source.index(signature,start+len(signature))
    end=source.index('{',start)+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

prefix=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include @ROUTES@
using namespace assist;
using DWORD=uint32_t;using WORD=uint16_t;using HWND=void*;using WPARAM=uintptr_t;using LPARAM=intptr_t;
#define HIWORD(v) ((uint16_t)((uintptr_t(v)>>16)&0xffff))
#define LOWORD(v) ((uint16_t)(uintptr_t(v)&0xffff))
#define MAKEWORD(a,b) (WORD((uint8_t(a))|(uint16_t(uint8_t(b))<<8)))
constexpr int MOD_SHIFT=4,MOD_CONTROL=2,MOD_ALT=1,HOTKEYF_SHIFT=1,HOTKEYF_CONTROL=2,HOTKEYF_ALT=4,SW_HIDE=0;
static bool enabled=true,minimapSecondary=false,focused=true,monitorPresent=false,readerOpen=true;
static HWND game=(void*)1,ui=(void*)2,overlay=(void*)3;
static struct {bool watchSecondary=false,experimentalReveal=false;uint32_t watchKeys[3]={0x657,0x657,0x657};}extra;
static struct Snapshot {int renderStatus=1,status=1,readStatus=1;bool chat=false;uint32_t match=99;char types[1]{};int typeCount=0;}snap,raw;
static const Snapshot*externalData=&raw;
static DWORD now=1000;static DWORD GetTickCount(){return now;}
static bool modifierDown[3]{},keyDown[256]{};
static short GetAsyncKeyState(int key){assert(key>=0&&key<256);return ((key>=16&&key<=18)?modifierDown[key-16]:keyDown[key])?short(0x8000):0;}
static bool foregroundGame(){return focused;}
static unsigned monitorQueries=0;
namespace secondarydisplay {struct Target{bool available()const{return monitorPresent;}};struct Monitor{Target get(HWND,HWND,DWORD){++monitorQueries;return {};}};}
static secondarydisplay::Monitor secondaryDisplayMonitor;
static unsigned samples=0,detaches=0,releases=0,clockCalls=0,registered=0,cleared=0,primaryHidden=0,allHidden=0,pins=0;
static bool clockActive=false;static int clockRate=0;static bool shared=true;
static DWORD sentAt=99,inFlightMatch=4,inFlightRetries=2;
struct Intent{int command=0,arg=0,arg2=0;uint32_t pid=0,match=0,tick=0;int retries=0;uint32_t due=0,token=0;uint64_t hash=0;};
static struct Queue{int size=0;void clear(){++cleared;size=0;}bool add(Intent){++size;return true;}}commandQueue,hotkeyPending,pointerMatches;
static uint32_t pid=42;static DWORD lastData=0;static Snapshot incoming;
static bool IsWindow(HWND w){return w!=nullptr;}
static uint64_t typeListHash(const char*,int){return 123;}
static void diag(const std::wstring&){}static void wakeGame(){}
constexpr int RevealMap=1,SelectType=2;
static uint32_t pendingSelectionObject=0;static uint8_t pendingSelectionTypes[8]{};static Intent pendingSelection;static bool hasSelection=false;
static unsigned bridgeAttempts=0,selectionServices=0,notifications=0;
static bool ensureBridge(){++bridgeAttempts;return false;}
static bool commandFresh(DWORD){return false;}
static void notify(const std::wstring&){++notifications;}
static void serviceSelection(){++selectionServices;}
namespace delivery {constexpr uint32_t UnknownMatch=0;}

static hotkeyroute::Registry hotkeyRegistry;
static void sampleExternal(DWORD){++samples;assert(readerOpen);}
static void clockState(bool active,int rate){++clockCalls;clockActive=active;clockRate=rate;}
static int informationFPS(){return 144;}
static void releaseBridge(){++releases;shared=false;}
static void detach(){++detaches;shared=false;readerOpen=false;snap.renderStatus=0;}
static void registerKeys(){++registered;}
static void setStatus(const std::wstring&){}
static void resetConstruction(){}static void resetAirPath(){}static void clearPassengerDisplay(){}
static void hideAircraftButton(){}static void hideConstruction(){}
static void hideWorldOverlay(){++primaryHidden;}
static void ShowWindow(HWND,int){++primaryHidden;}
static struct Recovery{void suspend(){}}overlayRecovery;
namespace worldrender {static void invalidate(){}}
namespace minimap {static void hidePrimary(){++primaryHidden;}static void hide(){++allHidden;}}
namespace watchwindows {static void hidePrimary(){++primaryHidden;}static void hide(){++allHidden;}}
namespace watchhost {static std::vector<uint32_t> pinned;static void pin(uint32_t key){++pins;pinned.push_back(key);}}
'''
functions='\n'.join(function(s) for s in (
 'static bool secondaryDisplayRequested()', 'static bool secondaryDisplayAvailable()',
 'static bool displayFrameRequested()', 'static int desiredHotkeyMode()',
 'static uint32_t displayMatch()', 'static void dispatchReadOnlyWatchKeys(',
 'static bool readOnlyWatchHotkey(', 'static void serviceReadOnlyWatchKeys()',
 'static void pauseAssistControls()', 'static bool serviceReadOnlyDisplays(',
 'static bool queueCommand(', 'static void sendSelection('))
scenario=r'''
static LPARAM chord(unsigned key,unsigned modifiers){return LPARAM((key<<16)|modifiers);}
static void bind(int id,uint16_t key){assert(hotkeyRegistry.add(id,key,true,[](int,uint16_t){return true;}));}
int main(){
 raw.match=7;
 // No second output avoids monitor queries and preserves primary behavior.
 assert(!secondaryDisplayRequested()&&displayFrameRequested()&&monitorQueries==0);
 enabled=false;focused=false;assert(!displayFrameRequested()&&desiredHotkeyMode()==0);
 minimapSecondary=true;assert(!displayFrameRequested());monitorPresent=true;assert(displayFrameRequested());
 // Pause removes commands, leaves read session/pins and secondary cadence alive.
 unsigned sampled=samples;pauseAssistControls();
 assert(!shared&&readerOpen&&detaches==0&&releases==1&&samples==sampled+1);
 assert(clockActive&&clockRate==144&&cleared==3&&primaryHidden==3&&allHidden==0);
 assert(sentAt==0&&inFlightMatch==0&&inFlightRetries==0);
 assert(displayMatch()==7);snap.match=1234;assert(displayMatch()==7);
 // UI command entry points cannot accumulate future intents while paused.
 assert(!queueCommand(42,1,2,7)&&commandQueue.size==0);
 uint8_t selectedMask[8]={1};sendSelection(3,true,selectedMask,55);
 assert(!hasSelection&&pendingSelectionObject==0&&selectionServices==0&&bridgeAttempts==0&&notifications==1);
 // Display-only controls never create a bridge. Invalid worlds hide both;
 // disappearing monitors stop high-rate presentation without dropping settings.
 assert(serviceReadOnlyDisplays(now)&&!shared&&readerOpen&&clockActive);
 snap.renderStatus=0;assert(serviceReadOnlyDisplays(now)&&!clockActive&&allHidden==2);
 snap.renderStatus=1;monitorPresent=false;assert(serviceReadOnlyDisplays(now)&&!clockActive);
 monitorPresent=true;assert(serviceReadOnlyDisplays(now)&&clockActive);
 enabled=true;sampled=samples;assert(!serviceReadOnlyDisplays(now)&&samples==sampled);
 // Only read-only watch bindings remain available while command master is off.
 enabled=false;minimapSecondary=false;extra.watchSecondary=true;focused=true;
 assert(desiredHotkeyMode()==1);snap.chat=true;assert(desiredHotkeyMode()==0);snap.chat=false;
 bind(1730,0x657);assert(readOnlyWatchHotkey(1730,chord('W',MOD_CONTROL|MOD_ALT))&&pins==1);
 assert(readOnlyWatchHotkey(1000,chord('W',MOD_CONTROL|MOD_ALT))&&pins==1);
 assert(readOnlyWatchHotkey(1730,chord('X',MOD_CONTROL|MOD_ALT))&&pins==1);
 focused=false;readOnlyWatchHotkey(1730,chord('W',MOD_CONTROL|MOD_ALT));assert(pins==1);focused=true;
 // Same physical key single/double actions use registry routing, not wp slot.
 hotkeyRegistry.clear();extra.watchKeys[0]=0x657;extra.watchKeys[1]=0x8657;bind(1730,0x657);bind(1731,0x8657);
 unsigned before=pins;readOnlyWatchHotkey(1730,chord('W',3));assert(pins==before);
 now+=100;readOnlyWatchHotkey(1730,chord('W',3));assert(pins==before+1&&watchhost::pinned.back()==0x8657);
 now+=400;readOnlyWatchHotkey(1730,chord('W',3));modifierDown[1]=modifierDown[2]=true;now+=351;serviceReadOnlyWatchKeys();assert(pins==before+2&&watchhost::pinned.back()==0x657);modifierDown[1]=modifierDown[2]=false;
 // Bare Shift is sampled only on a fresh edge in the active game; holding it
 // across foreground/round changes never adds another watch panel.
 hotkeyRegistry.clear();extra.watchKeys[0]=16;bind(1730,16);serviceReadOnlyWatchKeys();before=pins;
 modifierDown[0]=true;serviceReadOnlyWatchKeys();assert(pins==before+1);
 serviceReadOnlyWatchKeys();assert(pins==before+1);focused=false;serviceReadOnlyWatchKeys();focused=true;serviceReadOnlyWatchKeys();assert(pins==before+1);
 ++raw.match;serviceReadOnlyWatchKeys();assert(pins==before+1);modifierDown[0]=false;serviceReadOnlyWatchKeys();modifierDown[0]=true;serviceReadOnlyWatchKeys();assert(pins==before+2);
 // Explicit release waits for the main key to rise while Ctrl remains held.
 modifierDown[0]=false;hotkeyRegistry.clear();extra.watchKeys[0]=0x2000|0x0257;bind(1730,uint16_t(extra.watchKeys[0]));serviceReadOnlyWatchKeys();before=pins;
 modifierDown[1]=true;keyDown['W']=true;readOnlyWatchHotkey(1730,chord('W',MOD_CONTROL));serviceReadOnlyWatchKeys();assert(pins==before);
 keyDown['W']=false;serviceReadOnlyWatchKeys();assert(pins==before+1);
 keyDown['W']=true;readOnlyWatchHotkey(1730,chord('W',MOD_CONTROL));modifierDown[1]=false;serviceReadOnlyWatchKeys();keyDown['W']=false;serviceReadOnlyWatchKeys();assert(pins==before+1);
 hotkeyRegistry.clear();extra.watchKeys[0]=0xa257;bind(1730,uint16_t(extra.watchKeys[0]));serviceReadOnlyWatchKeys();before=pins;
 modifierDown[1]=true;keyDown['W']=true;readOnlyWatchHotkey(1730,chord('W',MOD_CONTROL));serviceReadOnlyWatchKeys();keyDown['W']=false;serviceReadOnlyWatchKeys();assert(pins==before);
 now+=100;keyDown['W']=true;readOnlyWatchHotkey(1730,chord('W',MOD_CONTROL));serviceReadOnlyWatchKeys();assert(pins==before);keyDown['W']=false;serviceReadOnlyWatchKeys();assert(pins==before+1);
 enabled=true;assert(!readOnlyWatchHotkey(1730,chord(16,0))&&desiredHotkeyMode()==2);
 // Turning off the last requested output performs the normal full detach.
 enabled=false;extra.watchSecondary=false;pauseAssistControls();assert(detaches==1&&!readerOpen&&!clockActive);
 std::cout<<"PASS production secondary-display scheduling, read-only pause/bridge isolation, invalid-world/monitor gates, stable display identity and single/double/modifier watch keys\n";
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory);cpp=tmp/'test.cpp';binary=tmp/'test'
    cpp.write_text(prefix.replace('@ROUTES@',json.dumps(str(root/'assist/source/hotkey_routes.h')))+functions+scenario)
    subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
