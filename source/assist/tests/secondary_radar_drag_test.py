"""Exercise shipped secondary-map drag/resize calculations with a Win32 boundary double."""
from pathlib import Path
import json
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=(root/'assist/source/minimap_overlay.h').read_text()

def function(signature):
    start=source.index(signature);opened=source.index('{',start);depth=1;end=opened+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

code=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include @LAYOUT@
using HWND=void*;using DWORD=uint32_t;using BOOL=int;using UINT=unsigned;
struct POINT{int x=0,y=0;};struct RECT{int left=0,top=0,right=0,bottom=0;};
constexpr UINT SWP_NOACTIVATE=0x10,SWP_NOZORDER=4,SWP_NOSIZE=1;
static HWND secondaryWindow=reinterpret_cast<HWND>(1),game=reinterpret_cast<HWND>(2),ui=reinterpret_cast<HWND>(3);
static RECT actual{};static POINT cursor{};static bool captured=true;static int saves=0,syncs=0;
static DWORD GetTickCount(){return 1000;}
static HWND GetCapture(){return captured?secondaryWindow:nullptr;}
static BOOL ReleaseCapture(){captured=false;return 1;}
static BOOL GetWindowRect(HWND,RECT*r){*r=actual;return 1;}
static BOOL GetClientRect(HWND,RECT*r){*r={0,0,actual.right-actual.left,actual.bottom-actual.top};return 1;}
static BOOL GetCursorPos(POINT*p){*p=cursor;return 1;}
static BOOL SetWindowPos(HWND,HWND,int x,int y,int w,int h,UINT flags){
 int width=(flags&SWP_NOSIZE)?actual.right-actual.left:w,height=(flags&SWP_NOSIZE)?actual.bottom-actual.top:h;
 actual={x,y,x+width,y+height};return 1;
}
namespace secondarydisplay{
struct Target{RECT work{-1920,0,0,1080};bool available()const{return true;}};
struct Monitor{Target get(HWND,HWND,DWORD){return {};}};
}
static secondarydisplay::Monitor secondaryMonitor;
static int minimapSecondaryX=10000,minimapSecondaryY=10000,minimapSecondaryWidth=320;
static int minimapMode=0;static double secondaryAspect(){return 1;}
static void changedSetting(){++saves;}static void syncSecondaryLayoutControls(){++syncs;}
struct PaintGate{int resets=0;void invalidate(){++resets;}};static PaintGate secondaryPaintGate;
@ENUM@
static SecondaryEdit secondaryEdit=SecondaryEdit::None;
static POINT secondaryStartCursor{};static RECT secondaryStartRect{};
static int secondaryRequestedWidth=320;
@FINISH@
@MOVE@
@HIT@
int main(){
 auto box=secondaryradar::place(-1920,0,0,1080,10000,10000,320,1);
 actual={box.x,box.y,box.x+box.w,box.y+box.h};
 assert(secondaryHit(secondaryWindow,box.w/2,box.h/2)==SecondaryEdit::None);
 assert(secondaryHit(secondaryWindow,box.w/2,5)==SecondaryEdit::Move);
 assert(secondaryHit(secondaryWindow,5,5)==SecondaryEdit::ResizeUpperLeft);
 assert(secondaryHit(secondaryWindow,box.w-5,box.h-5)==SecondaryEdit::Resize);
 assert(secondaryHit(reinterpret_cast<HWND>(8),5,5)==SecondaryEdit::None);

 secondaryEdit=SecondaryEdit::ResizeUpperLeft;secondaryStartRect=actual;
 secondaryStartCursor={actual.left+5,actual.top+5};cursor={secondaryStartCursor.x-200,secondaryStartCursor.y-200};
 moveSecondaryEdit();assert(actual.right==-12&&actual.bottom==1068&&actual.right-actual.left==520&&actual.bottom-actual.top==520);
 finishSecondaryEdit();assert(saves==1&&syncs==1&&minimapSecondaryWidth==520);
 auto restored=secondaryradar::place(-1920,0,0,1080,minimapSecondaryX,minimapSecondaryY,minimapSecondaryWidth,1);
 assert(std::abs(restored.x-actual.left)<=1&&std::abs(restored.y-actual.top)<=1&&restored.w==520);

 captured=true;secondaryEdit=SecondaryEdit::Move;secondaryStartRect=actual;
 secondaryStartCursor={actual.left+40,actual.top+8};cursor={secondaryStartCursor.x-350,secondaryStartCursor.y-150};
 moveSecondaryEdit();assert(actual.left<secondaryStartRect.left&&actual.top<secondaryStartRect.top&&actual.right-actual.left==520);
 finishSecondaryEdit();assert(saves==2&&syncs==2&&minimapSecondaryWidth==520);
 restored=secondaryradar::place(-1920,0,0,1080,minimapSecondaryX,minimapSecondaryY,minimapSecondaryWidth,1);
 assert(std::abs(restored.x-actual.left)<=1&&std::abs(restored.y-actual.top)<=1);

 captured=true;secondaryEdit=SecondaryEdit::Resize;secondaryStartRect=actual;
 secondaryStartCursor={actual.right-5,actual.bottom-5};cursor={secondaryStartCursor.x-120,secondaryStartCursor.y};
 moveSecondaryEdit();assert(actual.right-actual.left==400&&actual.bottom-actual.top==400);
 finishSecondaryEdit();assert(saves==3&&syncs==3&&minimapSecondaryWidth==400);
 restored=secondaryradar::place(-1920,0,0,1080,minimapSecondaryX,minimapSecondaryY,minimapSecondaryWidth,1);
 assert(std::abs(restored.x-actual.left)<=1&&std::abs(restored.y-actual.top)<=1);
 std::cout<<"PASS production secondary map: move and two aspect-preserving resize grips, capture to persistent controls, no post-release snap\n";
}
'''
start=source.index('enum class SecondaryEdit ')
enum=source[start:source.index(';',start)+1]
code=code.replace('@LAYOUT@',json.dumps(str((root/'assist/source/secondary_radar_layout.h').resolve())))
for key,value in {'@ENUM@':enum,'@FINISH@':function('static void finishSecondaryEdit()'),
                  '@MOVE@':function('static void moveSecondaryEdit()'),
                  '@HIT@':function('static SecondaryEdit secondaryHit(')}.items():code=code.replace(key,value)
with tempfile.TemporaryDirectory(prefix='secondary-map-drag-') as tmp:
    p=Path(tmp);(p/'test.cpp').write_text(code)
    subprocess.run(['g++','-std=c++17','-O2',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,timeout=20)
