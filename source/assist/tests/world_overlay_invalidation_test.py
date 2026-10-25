"""Exercise shipped world-overlay hide/submit handlers with the real async worker."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'assist/source/main.cpp').read_text().split('namespace worldrender {', 1)[1]
assert 'static bool active=false;' in source

def production_function(signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

cpp = r'''
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include "@WORKER@"

struct Job {int serial=0;};
struct Frame {int serial=0;};
static std::mutex gate;
static std::condition_variable changed;
static bool entered=false,released=false;
static bool await(bool&value){
 std::unique_lock<std::mutex> lock(gate);
 return changed.wait_for(lock,std::chrono::seconds(3),[&]{return value;});
}
namespace worldrender {
struct CountingWorker {
 int invalidations=0,submissions=0;
 asyncwork::Latest<Job,Frame> core{[](const Job&job)->std::shared_ptr<const Frame>{
  if(job.serial==1){std::unique_lock<std::mutex> lock(gate);entered=true;changed.notify_all();changed.wait(lock,[]{return released;});}
  return std::make_shared<Frame>(Frame{job.serial});
 }};
 void submit(Job job){++submissions;core.submit(std::move(job));}
 void invalidate(){++invalidations;core.invalidate();}
 std::shared_ptr<const Frame> get(){return core.get();}
}worker;
static std::shared_ptr<const Frame> displayed;
static bool active=false;
@SUBMIT@
@INVALIDATE@
}
int main(){
 using namespace worldrender;
 invalidate();assert(!active&&worker.invalidations==0);
 submit({1});assert(active&&worker.submissions==1&&await(entered));
 invalidate();for(int i=0;i<1000;++i)invalidate();
 assert(!active&&worker.invalidations==1&&"hidden display must cancel one pending generation only");
 {std::lock_guard<std::mutex> lock(gate);released=true;}changed.notify_all();
 std::this_thread::sleep_for(std::chrono::milliseconds(25));
 assert(!worker.get()&&"an in-flight old-world frame must not reappear after hide");
 submit({2});assert(active&&worker.submissions==2);
 for(int n=0;n<300&&!worker.get();++n)std::this_thread::sleep_for(std::chrono::milliseconds(2));
 assert(worker.get()&&worker.get()->serial==2&&"resuming after hide must render again");
 displayed=worker.get();invalidate();assert(worker.invalidations==2&&!worker.get()&&!displayed);
 submit({3});for(int n=0;n<300&&(!worker.get()||worker.get()->serial!=3);++n)std::this_thread::sleep_for(std::chrono::milliseconds(2));
 assert(worker.get()&&worker.get()->serial==3);
 std::cout<<"PASS world overlay: single hide cancellation, no repeated epoch churn, stale completion rejected, resume publishes fresh frame\n";
}
'''.replace('@WORKER@', str((root / 'shared/source/latest_worker.h').resolve()))
cpp = cpp.replace('@SUBMIT@', production_function('static void submit(Job input)'))
cpp = cpp.replace('@INVALIDATE@', production_function('static void invalidate()'))
with tempfile.TemporaryDirectory(prefix='world-overlay-') as temp:
    file = Path(temp) / 'test.cpp'
    exe = Path(temp) / 'test'
    file.write_text(cpp)
    subprocess.run(['g++', '-std=c++17', '-O2', '-pthread', str(file), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=20)
