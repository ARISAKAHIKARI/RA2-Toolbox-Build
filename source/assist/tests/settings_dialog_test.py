"""Exercise the actual dialog policy and cursor call sites with Win32 stubs.

The Windows pane fixture separately creates and dismisses a real MessageBox
while its settings page is cloaked; this portable test makes no DWM claim.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = (root / 'shared/source/settings_dialog.h').read_text()
policy = header.split('inline int message(', 1)[1].split('\n}', 1)[0] + '\n}'
main = (root / 'cursor/source/main.cpp').read_text()
wrapper = main.split('static int cursorDialog(', 1)[1].split('\n}', 1)[0] + '\n}'
assert 'MessageBoxW(mainw,' not in main
assert main.count('cursorDialog(') == 5  # wrapper plus restore/error/close/result
fixture = r'''
#include <cassert>
#include <cstdio>
using HWND=void*;using UINT=unsigned;
constexpr unsigned MB_TASKMODAL=0x2000,MB_SETFOREGROUND=0x10000,MB_TOPMOST=0x40000;
constexpr unsigned QUESTION=0x24,WARNING=0x34;
static HWND mainw=reinterpret_cast<HWND>(101),lastOwner=nullptr;
static unsigned lastFlags=0,calls=0;static int result=6;
static int MessageBoxW(HWND owner,const wchar_t*text,const wchar_t*title,UINT flags){
 assert(text&&title);lastOwner=owner;lastFlags=flags;++calls;return result;
}
namespace settingsdialog {inline int message(POLICY
}
static int cursorDialog(WRAPPER
int main(){
 for(auto flags:{QUESTION,WARNING}){
  auto expected=result;assert(cursorDialog(L"Text",L"Title",flags)==expected);
#ifdef RA2_UNIFIED
  assert(lastOwner==nullptr&&lastFlags==(flags|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST));
#else
  assert(lastOwner==mainw&&lastFlags==flags);
#endif
  ++result;
 }
 assert(calls==2);
 std::puts("PASS: cursor prompts preserve result/flags, managed dialogs avoid cloaked/cross-thread owners and remain foreground/task-modal; standalone owner preserved");
}
'''.replace('POLICY', policy).replace('WRAPPER', wrapper)
with tempfile.TemporaryDirectory(prefix='settings-dialog-') as temp:
    cpp = Path(temp) / 'test.cpp'
    cpp.write_text(fixture)
    for managed in (False, True):
        exe = Path(temp) / ('managed' if managed else 'standalone')
        subprocess.run(['g++', '-std=c++17', '-O2', '-UNDEBUG', '-include', 'initializer_list',
                        *(['-DRA2_UNIFIED'] if managed else []), str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
