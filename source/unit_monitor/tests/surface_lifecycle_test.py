"""Exercise the shipped statistics GDI surface with allocation failure doubles."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'source/main.cpp').read_text()
begin = source.index('struct UnitSurface {')
end = source.index('} surface,secondarySurface;', begin)
surface = source[begin:end + 1] + ';'

fixture = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using DWORD=unsigned;using UINT=unsigned;using HWND=void*;
struct Bitmap {std::vector<uint32_t> storage;bool live=false;};
struct DC {void* selected=nullptr;bool live=false;};
using HBITMAP=Bitmap*;using HDC=DC*;using HGDIOBJ=void*;
static HGDIOBJ const HGDI_ERROR=reinterpret_cast<HGDIOBJ>(-1);
constexpr unsigned DIB_RGB_COLORS=0;
struct BITMAPINFOHEADER {unsigned biSize=0;int biWidth=0,biHeight=0;unsigned short biPlanes=0,biBitCount=0;};
struct BITMAPINFO {BITMAPINFOHEADER bmiHeader{};};
enum class Failure {None,Screen,MemoryDC,Bitmap,Bits,SelectNull,SelectError};
static Failure failure=Failure::None;
static Bitmap stock{};static DC screen{};
static std::vector<Bitmap*> bitmaps;static std::vector<DC*> dcs;
static int liveBitmaps=0,liveDCs=0,screenLeases=0,selectCalls=0,invalidSelections=0,invalidDeletes=0,createdBitmaps=0;
static HDC GetDC(HWND){if(failure==Failure::Screen)return nullptr;++screenLeases;return &screen;}
static int ReleaseDC(HWND,HDC dc){assert(dc==&screen);--screenLeases;return 1;}
static HDC CreateCompatibleDC(HDC dc){assert(dc==&screen);if(failure==Failure::MemoryDC)return nullptr;
 auto*next=new DC{&stock,true};dcs.push_back(next);++liveDCs;return next;}
static HBITMAP CreateDIBSection(HDC dc,const BITMAPINFO*info,UINT,void**bits,void*,DWORD){
 assert(dc==&screen);if(failure==Failure::Bitmap)return nullptr;
 auto*next=new Bitmap{{},true};next->storage.resize(size_t(info->bmiHeader.biWidth)*-info->bmiHeader.biHeight);
 *bits=failure==Failure::Bits?nullptr:next->storage.data();bitmaps.push_back(next);++liveBitmaps;++createdBitmaps;return next;
}
static HGDIOBJ SelectObject(HDC dc,HGDIOBJ item){
 assert(dc&&dc->live);++selectCalls;
 if(!item||item==HGDI_ERROR){++invalidSelections;return nullptr;}
 if(failure==Failure::SelectNull)return nullptr;
 if(failure==Failure::SelectError)return HGDI_ERROR;
 auto previous=dc->selected;dc->selected=item;return previous;
}
static int DeleteDC(HDC dc){assert(dc&&dc->live);dc->live=false;--liveDCs;return 1;}
static int DeleteObject(HGDIOBJ item){
 auto*bitmap=static_cast<Bitmap*>(item);assert(bitmap&&bitmap->live);
 for(auto*dc:dcs)if(dc->live&&dc->selected==item){++invalidDeletes;return 0;}
 bitmap->live=false;--liveBitmaps;return 1;
}
@SURFACE@
static void clean(){assert(liveBitmaps==0&&liveDCs==0&&screenLeases==0&&invalidSelections==0&&invalidDeletes==0);
 for(auto*p:bitmaps)delete p;bitmaps.clear();for(auto*p:dcs)delete p;dcs.clear();}
int main(){
 {
  UnitSurface s;
  assert(s.ensure(64,32)&&s.dc&&s.bitmap&&s.bits&&s.w==64&&s.h==32);
  const int created=createdBitmaps;assert(s.ensure(64,32)&&createdBitmaps==created);
  assert(s.ensure(80,48)&&createdBitmaps==created+1&&liveBitmaps==1&&liveDCs==1);
  s.clear();s.clear();assert(!s.dc&&!s.bitmap&&!s.old&&!s.bits&&!s.w&&!s.h);clean();
 }
 for(auto mode:{Failure::Screen,Failure::MemoryDC,Failure::Bitmap,Failure::Bits,Failure::SelectNull,Failure::SelectError}){
  UnitSurface s;failure=mode;
  assert(!s.ensure(64,32)&&!s.dc&&!s.bitmap&&!s.old&&!s.bits&&!s.w&&!s.h);
  failure=Failure::None;clean();
  assert(s.ensure(64,32));s.clear();clean();
 }
 {UnitSurface s;assert(!s.ensure(0,32)&&!s.ensure(32,-1)&&!s.ensure(8192,8192));clean();}
 std::cout<<"PASS production statistics GDI surface: reuse, resize, partial allocation, invalid selection, bounds and cleanup\n";
}
'''

with tempfile.TemporaryDirectory(prefix='stats-surface-') as temp:
    cpp = Path(temp) / 'test.cpp'
    exe = Path(temp) / 'test'
    cpp.write_text(fixture.replace('@SURFACE@', surface))
    subprocess.run(['g++', '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
