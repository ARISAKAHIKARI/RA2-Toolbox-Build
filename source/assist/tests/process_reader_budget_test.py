"""Production read-page growth/budget policy, with bounded Win32 read doubles."""
from pathlib import Path
import ast
import json
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
tree=ast.parse((Path(__file__).with_name('process_reader_pages_test.py')).read_text())
windows=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
             and any(isinstance(t,ast.Name) and t.id=='windows' for t in n.targets))
code=r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>
#include <atomic>
static std::atomic<size_t> allocationCalls{0};
static int allocationsBeforeFailure=-1;
void*operator new(size_t n){++allocationCalls;if(allocationsBeforeFailure==0)throw std::bad_alloc();if(allocationsBeforeFailure>0)--allocationsBeforeFailure;if(auto*p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
#include @READER@
DWORD fakeLastError=0;
static uint8_t version=0;
static uint32_t partialPage=0;
constexpr uint32_t Base=0x100000;
uint8_t byteAt(uintptr_t p){return uint8_t(p^(p>>12)^version);}
BOOL ReadProcessMemory(HANDLE,const void*at,void*out,SIZE_T n,SIZE_T*got){
 const auto p=uintptr_t(at);
 if(p<0x10000||p+n>0x80000000ull||(p==partialPage&&n==4096)){*got=0;fakeLastError=ERROR_PARTIAL_COPY;return FALSE;}
 auto*b=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i)b[i]=byteAt(p+i);*got=n;return TRUE;
}
void readField(external::ProcessReader&r,size_t page,size_t field=0){
 uint8_t value[4]{};const uint32_t at=Base+uint32_t(page*4096+field*4);
 assert(r.read(at,value,4));for(unsigned i=0;i<4;++i)assert(value[i]==byteAt(at+i));
}
int main(){
 using namespace external;
 static_assert(readpages::PageBytes==4096);
 static_assert(readpages::SessionPages==(sizeof(void*)>=8?65536:16384));
 static_assert(readpages::ProcessPages==(sizeof(void*)>=8?262144:65536));
 assert(readpages::sharedBudget().used()==0);
 for(size_t count:{8193u,16384u}){
  ProcessReader reader;assert(reader.open(7));
  assert(reader.cachedPageStorage()==0&&readpages::sharedBudget().used()==0);
  for(size_t f=0;f<12;++f)for(size_t p=0;p<count;++p)readField(reader,p,f);
  assert(reader.reads==count&&reader.pageHits==11*count);
  assert(reader.pageEvictions==0&&reader.cachedPageStorage()==count&&readpages::sharedBudget().used()==count);
  const size_t buffers=reader.pageBufferAllocations(),allocs=allocationCalls.load();
  for(size_t f=0;f<12;++f)for(size_t p=0;p<count;++p)readField(reader,p,f);
  assert(reader.reads==count&&reader.pageBufferAllocations()==buffers&&allocationCalls==allocs);
  ++version;reader.begin();for(size_t p=0;p<count;++p)readField(reader,p);
  assert(reader.reads==count&&reader.pageBufferAllocations()==buffers&&allocationCalls==allocs);
  std::printf("{\"scenario\":\"field_major\",\"pages\":%zu,\"fields\":12,\"RPM_calls\":%zu,\"warm_allocations\":0,\"epoch_fresh\":true}\n",count,count);
  reader.close();assert(readpages::sharedBudget().used()==0);
 }
 // A small injected pool exercises the same process-wide claims without a
 // 1 GiB test allocation. The budget must outlive its sessions.
 {
  readpages::Budget budget(6);
  {
   ProcessReader a(budget,4),b(budget,4),c(budget,4);assert(a.open(7)&&b.open(7)&&c.open(7));
   assert(budget.used()==0);
   for(size_t p=0;p<4;++p)readField(a,p);
   for(size_t p=4;p<7;++p)readField(b,p);
   assert(budget.used()==6&&a.cachedPageStorage()==4&&b.cachedPageStorage()==2&&b.pageBudgetDenials==1);
   for(size_t p=7;p<32;++p)readField(b,p);
   assert(budget.used()==6&&b.pageBudgetDenials==1);
   // With no first heap page available, fixed 4 KiB read scratch still makes
   // adjacent fields one complete-page read, with no allocation retries.
   const auto allocs=allocationCalls.load();readField(c,100,0);readField(c,100,1);
   assert(c.cachedPageStorage()==0&&c.reads==1&&c.emergencyPageReads==1&&c.pageHits==1&&c.pageBudgetDenials==1&&allocationCalls==allocs);
   uint8_t crossing[8]{};const uint32_t at=Base+200*4096+4093;
   assert(c.read(at,crossing,sizeof crossing));for(unsigned i=0;i<sizeof crossing;++i)assert(crossing[i]==byteAt(at+i));
   assert(c.reads==3&&budget.used()==6);
   ++version;c.begin();readField(c,100);assert(c.reads==1&&c.pageBudgetDenials==1);
   a.close();assert(budget.used()==2);
   c.begin();readField(c,100);readField(c,101);assert(c.cachedPageStorage()==2&&c.emergencyPageReads==0&&budget.used()==4);
   b.begin();readField(b,32);assert(b.cachedPageStorage()==3&&budget.used()==5);
   b.close();assert(budget.used()==2);
  }
  assert(budget.used()==0);
  std::puts("{\"scenario\":\"shared_budget\",\"cap_pages\":6,\"peak_heap_pages\":6,\"release_and_recovery\":true,\"empty_session_scratch\":true}");
 }
 {
  readpages::Budget budget(8);ProcessReader old(budget,8),fresh(budget,8);assert(old.open(7)&&fresh.open(7));
  for(unsigned p=0;p<8;++p)readField(old,p);
  old.begin();readField(old,0);assert(old.cachedPageStorage()==8&&old.pagePressureRetired==0);
  const auto buffers=old.pageBufferAllocations();
  readField(fresh,100);assert(fresh.cachedPageStorage()==0&&budget.used()==8&&budget.pressure()==1);
  old.begin();assert(old.pagePressureRetired==6&&old.cachedPageStorage()==2&&budget.used()==2);
  // Last observation's working page survives. Only its bytes, not its node,
  // refresh on this new epoch; the second kept slot is the small test floor.
  readField(old,0);assert(old.pageBufferAllocations()==buffers);
  fresh.begin();readField(fresh,100);readField(fresh,101);
  assert(fresh.cachedPageStorage()==2&&fresh.emergencyPageReads==0&&budget.used()==4);
  old.begin();assert(old.pagePressureRetired==0&&old.cachedPageStorage()==2); // no new pressure, no trim
  old.close();fresh.close();assert(budget.used()==0);
  std::puts("{\"scenario\":\"pressure_trim\",\"before_heap_pages\":8,\"retired_stale_pages\":6,\"last_working_preserved\":true,\"new_session_recovered\":true}");
 }
 {
  readpages::Budget none(0);ProcessReader reader(none);assert(reader.open(7));
  std::vector<uint32_t> pages;for(unsigned p=0;p<4096;++p)pages.push_back(Base+p*4096);
  reader.prefetch(pages,4096);
  assert(reader.reads==1&&reader.pageBudgetDenials==1&&reader.prefetchedPages==0&&none.used()==0);
  reader.prefetch(pages,4096);assert(reader.reads==1); // no repeated speculative work without a slot
  readField(reader,0);readField(reader,0,1);
  assert(reader.reads==2&&reader.emergencyPageReads==1&&reader.pageHits==1);
 }
 // Failure at list allocation, map-node allocation, or map rehash must return
 // its claim and keep reads functional; next epoch may grow normally.
 for(int failAfter:{0,1,2}){
  readpages::Budget budget(8);ProcessReader reader(budget,8);assert(reader.open(7));
  allocationsBeforeFailure=failAfter;readField(reader,30);allocationsBeforeFailure=-1;
  assert(reader.pageAllocationFailures==1&&reader.cachedPageStorage()==0&&budget.used()==0&&reader.emergencyPageReads==1);
  const auto allocs=allocationCalls.load();readField(reader,30,1);readField(reader,31);
  assert(reader.pageAllocationFailures==1&&allocationCalls==allocs&&budget.used()==0);
  ++version;reader.begin();readField(reader,30);assert(reader.cachedPageStorage()==1&&budget.used()==1);
  reader.close();assert(budget.used()==0);
 }
 {
  readpages::Budget budget(8);ProcessReader reader(budget,8);assert(reader.open(7));
  readField(reader,0);readField(reader,1);reader.begin();
  allocationsBeforeFailure=0;readField(reader,2);allocationsBeforeFailure=-1;
  assert(reader.cachedPageStorage()==2&&budget.used()==2&&reader.pageAllocationFailures==1&&reader.pageEvictions==1&&reader.pageCurrentEvictions==0);
  const auto allocs=allocationCalls.load();
  for(size_t p=3;p<103;++p)readField(reader,p);
  assert(allocationCalls==allocs&&reader.pageAllocationFailures==1&&reader.cachedPageStorage()==2&&budget.used()==2);
  // Negative page cache preserves direct-range recovery without repeated
  // failed full-page attempts; a new observation retries the full page.
  partialPage=Base+300*4096;readField(reader,300);const auto calls=reader.reads;readField(reader,300,1);
  assert(reader.reads==calls+1&&reader.pageFallbacks==2&&reader.errors==0);
  partialPage=0;reader.begin();readField(reader,300);assert(reader.reads==1);
 }
 // Claims are shared atomically, without serializing normal cache hits.
 {
  readpages::Budget budget(3);std::atomic<size_t>peak{0};std::vector<std::thread>threads;
  for(unsigned t=0;t<8;++t)threads.emplace_back([&](){for(unsigned n=0;n<20000;++n)if(budget.acquire()){
   auto now=budget.used(),old=peak.load();while(old<now&&!peak.compare_exchange_weak(old,now)){}
   assert(now<=3);budget.release(1);
  }});
  for(auto&t:threads)t.join();assert(budget.used()==0&&peak.load()<=3);
 }
 {
  readpages::Budget none(0);std::vector<std::thread>threads;
  for(unsigned t=0;t<8;++t)threads.emplace_back([&](){for(unsigned n=0;n<20000;++n)assert(!none.acquire());});
  for(auto&t:threads)t.join();assert(none.pressure()==8*20000&&none.used()==0);
 }
 assert(readpages::sharedBudget().used()==0);
 std::puts("PASS ProcessReader budget: lazy growth, 8193/16384 fields, shared cap/release, scratch, bad_alloc rollback/reuse, fresh epochs and concurrent claims");
}
'''.replace('@READER@',json.dumps(str(root/'external_reader/source/process_reader.h')))
with tempfile.TemporaryDirectory(prefix='ra2-reader-budget-') as tmp:
    directory=Path(tmp)
    (directory/'windows.h').write_text(windows)
    (directory/'test.cpp').write_text(code)
    subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-pthread','-I'+str(directory),
                    str(directory/'test.cpp'),'-o',str(directory/'test')],check=True)
    subprocess.run([str(directory/'test')],check=True,timeout=30)
