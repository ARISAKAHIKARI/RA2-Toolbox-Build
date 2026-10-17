"""Compile the shipped snapshot consumer; race the writer during its copy."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'assist/source/main.cpp').read_text()
read='static void readSnapshot(){'+s.split('static void readSnapshot(){',1)[1].split('struct TextRaster',1)[0]
program=r'''
#include "protocol.h"
#include <cassert>
#include <cstring>
#include <string>
#include <memory>
using namespace assist;using DWORD=uint32_t;
static auto block=std::make_unique<Shared>();static Shared*shared=block.get();
static Snapshot incoming{},snap{};static uint32_t snapshotSeq=~0u;
static DWORD lastData=0,clockNow=100;static int barriers=0,receipts=0,merges=0;
static bool collide=false,bridgeBlocked=false,hasSelection=false;static int bridgeAttempts=2;
struct {int size=0;void clear(){size=0;}}commandQueue;
static DWORD GetTickCount(){return clockNow;}
static void MemoryBarrier(){if(collide&&++barriers==2){++shared->seq;shared->data.status=-2;shared->data.match=999;}}
static void logCommandReceipts(const Snapshot&){++receipts;}
static void diag(const std::wstring&){}
static void mergeExternal(){++merges;}
'''+read+r'''
int main(){
 readSnapshot();assert(!lastData&&!receipts&&bridgeAttempts==2); // no Runtime publication
 shared->seq=2;shared->data.status=1;shared->data.match=7;shared->data.commandStatus=1;
 readSnapshot();assert(lastData==100&&receipts==1&&incoming.match==7&&!bridgeBlocked);
 clockNow=200;shared->seq=4;shared->data.status=-2;shared->data.match=99;collide=true;barriers=0;
 readSnapshot();assert(lastData==100&&receipts==1&&incoming.match==7&&incoming.status==1&&!bridgeBlocked);
 collide=false;shared->seq=6;shared->data.status=1;shared->data.match=7;readSnapshot();assert(lastData==200&&receipts==2);
 shared->seq=0;clockNow=210;readSnapshot();assert(lastData==210&&receipts==3); // sequence wraps after authentication
 shared->seq=2;shared->data.status=-2;commandQueue.size=3;hasSelection=true;readSnapshot();
 assert(bridgeBlocked&&!hasSelection&&!commandQueue.size&&incoming.status==-2);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'assist/source'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS unpublished seq=0, torn-copy isolation, fresh stable publish, sequence wrap, fault cancels unsent backlog')
