"""Test the shipped fixed compute policy, removed settings route and legacy INI behavior.

This is a source contract and portable C++ policy test; it does not replace
the native Windows build or the game-level command/ownership regression tests.
"""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]/"source"
policy=(root/"compute_policy.h").read_text(encoding="utf-8")
controls=(root/"tactical_controls.h").read_text(encoding="utf-8-sig")
settings=(root/"extra_settings_io.h").read_text(encoding="utf-8-sig")
handler=(root/"auto_settings_ui.h").read_text(encoding="utf-8")
host=(root/"main.cpp").read_text(encoding="utf-8-sig")
help_text=(root/"hover_help.h").read_text(encoding="utf-8-sig")
for token in ('combo(9000+i)', 'L"工具箱计算",L"游戏内计算"', 'activeFoldGroup=9609;', 'id==833'):
    assert token not in controls, token
assert 'if(id>=9000&&id<9000+compute::Count&&select)' not in handler
assert 'id>=9000&&id<9000+compute::Count' not in help_text
assert '{9609,L"计算模式"}' not in host
assert 'GetDlgItem(ui,833)' not in host
assert 'computeRequested=compute::Fixed;' in host
assert 'computeRequested=compute::Fixed;' in settings
assert 'shared->computeRequested=compute::Fixed;' in settings
assert 'value(L"Compute"' not in settings
assert 'write(L"Compute"' not in settings
assert 'constexpr uint32_t Fixed=All;' in policy

program=r"""
#include "compute_policy.h"
#include <cassert>
using namespace assist;
static_assert(compute::Fixed == compute::All);
static_assert((compute::Fixed & ~compute::All) == 0);
static_assert(compute::Count == 15);
int main(){
 for(int i=0;i<compute::Count;++i){
   auto kind=compute::Feature(i);
   assert(compute::inside(compute::Fixed,kind));
 }
 assert(compute::inside(compute::Fixed,compute::Load));
 assert(compute::inside(compute::Fixed,compute::Repair));
 assert(compute::inside(compute::Fixed,compute::Unload));
 assert(compute::inside(compute::Fixed,compute::Dodge));
 assert(compute::inside(compute::Fixed,compute::Advanced));
 assert(compute::inside(compute::Fixed,compute::Magnetron));
 assert(compute::inside(compute::Fixed,compute::Ore));
 assert(compute::inside(compute::Fixed,compute::Aircraft));
 // The old inner/outer pure planners remain available for regression tests.
 assert(compute::plannerFlags(8192u,compute::Fixed,false)==0u);
 assert(compute::plannerFlags(8192u,compute::Fixed,true)==8192u);
}
"""
with tempfile.TemporaryDirectory(prefix="compute-policy-") as d:
    source=Path(d)/"fixed.cpp"
    binary=Path(d)/"fixed"
    source.write_text(program,encoding="utf-8")
    subprocess.run(["g++","-std=c++17","-O2","-UNDEBUG","-I",str(root),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print("PASS fixed compute policy and removed settings source contract")
