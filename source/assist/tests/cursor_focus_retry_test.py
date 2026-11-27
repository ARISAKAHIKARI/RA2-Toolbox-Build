"""Execute the production cursor startup backoff and check non-modal error routes."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'cursor/source/main.cpp').read_text()
a=source.index('static DWORD hideRetryPid=')
b=source.index('\n}',source.index('static DWORD deferHideRetry(',a))+2
policy=source[a:b]
cpp=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
using DWORD=uint32_t;
'''+policy+r'''
int main(){
 assert(deferHideRetry(100,7)==2000&&hideRetryAt==2100&&hideRetryPid==7);
 assert(deferHideRetry(2100,7)==4000&&hideRetryAt==6100);
 assert(deferHideRetry(6100,7)==8000&&hideRetryAt==14100);
 assert(deferHideRetry(14100,7)==8000&&hideRetryAt==22100);
 assert(deferHideRetry(100,8)==2000&&hideRetryPid==8&&hideRetryFailures==1);
 resetHideRetry();assert(!hideRetryPid&&!hideRetryAt&&!hideRetryFailures);
 assert(deferHideRetry(0xffffff00u,8)==2000&&int32_t(0xffffff00u-hideRetryAt)<0);
 assert(int32_t(DWORD(0xffffff00u+2000u)-hideRetryAt)==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'retry.cpp'; exe=Path(tmp)/'retry'; src.write_text(cpp)
    subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
assert 'try{draw();}catch(const std::exception& e){autoPaused=true;stop();error(e,false);}' in source
assert 'if(interactive)cursorDialog' in source
assert '!hideConfirmed&&(err==1||err==2)' in source
assert 'hideConfirmed=true;resetHideRetry();' in source
assert 'foregroundTransition(fg,fp);' in source
assert 'foregroundPid!=GetCurrentProcessId()||thread==GetCurrentThreadId()' in source
assert 'int32_t(now-hideRetryAt)<0' in source
print('PASS production startup backoff 2/4/8s, cap, target/reset and tick wrap; background modal and transition contracts (not a live game test)')
