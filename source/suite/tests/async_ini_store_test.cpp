#include "../../shared/source/async_ini_store.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <set>

using asyncini::core::Store;
using asyncini::core::Operation;
using asyncini::core::Result;
using asyncini::core::Kind;
using asyncini::core::Key;
using namespace std::chrono_literals;

template<class F>void eventually(F f){
 const auto end=std::chrono::steady_clock::now()+5s;
 while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}
}
struct Gate {
 std::mutex mutex;std::condition_variable cv;bool entered=false,released=false;
 void wait(){std::unique_lock<std::mutex>l(mutex);entered=true;cv.notify_all();cv.wait(l,[&]{return released;});}
 void await(){std::unique_lock<std::mutex>l(mutex);assert(cv.wait_for(l,5s,[&]{return entered;}));}
 void release(){std::lock_guard<std::mutex>l(mutex);released=true;cv.notify_all();}
};
struct Disk {
 std::mutex mutex;std::map<Key,std::wstring> values;std::vector<Operation> calls;
 void apply(const Operation&o){
  std::lock_guard<std::mutex>l(mutex);calls.push_back(o);const auto k=asyncini::core::keyOf(o);
  if(o.kind==Kind::Set)values[k]=o.value.value;
  if(o.kind==Kind::DeleteKey)values.erase(k);
  if(o.kind==Kind::DeleteSection)for(auto it=values.begin();it!=values.end();){if(it->first.path==k.path&&it->first.section==k.section)it=values.erase(it);else ++it;}
 }
 std::wstring get(const wchar_t*s,const wchar_t*k,const wchar_t*p){std::lock_guard<std::mutex>l(mutex);auto it=values.find(asyncini::core::keyOf(Operation(s,k,L"",p)));return it==values.end()?L"<missing>":it->second;}
};

void coalescing_and_barriers(){
 Gate gate;Disk disk;disk.apply(Operation(L"Other",L"Unknown",L"preserved",L"C:\\Config.ini"));disk.apply(Operation(L"Section",L"Unknown",L"to-delete",L"C:\\Config.ini"));
 std::atomic<unsigned>count{0};Store store([&](const Operation&o){if(count++==0)gate.wait();disk.apply(o);return Result{};},32);
 assert(store.write(L"Gate",L"Hold",L"1",L"C:\\Config.ini"));gate.await();
 assert(store.write(L"SECTION",L"Key",L"1",L"C:/Config.ini"));
 assert(store.write(L"section",L"key",L"2",L"c:\\config.ini"));
 auto pending=store.lookup(L"Section",L"KEY",L"C:\\CONFIG.INI");assert(pending.known&&pending.present&&pending.value==L"2");
 assert(store.write(L"Section",nullptr,nullptr,L"C:\\Config.ini"));
 assert(!store.lookup(L"Section",L"Unknown",L"c:/config.ini").present);
 assert(store.write(L"Section",L"Key",L"3",L"C:\\Config.ini"));
 assert(store.write(L"Section",L"Key",L"4",L"C:\\Config.ini"));
 assert(store.write(L"Section",L"Key",nullptr,L"C:\\Config.ini"));
 assert(store.write(L"Section",L"Key",L"5",L"C:\\Config.ini"));
 assert(store.requestFlush(L"C:\\Config.ini"));
 assert(store.status().pending==6); // 2, delete section, 4, delete key, 5, flush.
 gate.release();eventually([&]{return store.idle();});
 assert(disk.get(L"Section",L"Key",L"c:\\config.ini")==L"5");
 assert(disk.get(L"Other",L"Unknown",L"c:\\config.ini")==L"preserved");
 assert(disk.get(L"Section",L"Unknown",L"c:\\config.ini")==L"<missing>");
 assert(!store.lookup(L"Section",L"Key",L"C:\\Config.ini").known);
 const auto&calls=disk.calls;assert(calls[calls.size()-1].kind==Kind::Flush);
 std::vector<std::wstring> writes;for(const auto&o:calls)if(o.kind==Kind::Set&&asyncini::core::keyOf(o).section==L"section"&&asyncini::core::keyOf(o).key==L"key")writes.push_back(o.value.value);
 assert((writes==std::vector<std::wstring>{L"2",L"4",L"5"}));
 assert(store.status().completedRevision==store.status().submittedRevision);
}

void owned_values_and_bounded_admission(){
 Gate gate;Disk disk;std::atomic<unsigned>count{0};Store store([&](const Operation&o){if(count++==0)gate.wait();disk.apply(o);return Result{};},2);
 assert(store.write(L"A",L"Hold",L"hold",L"file.ini"));gate.await();
 std::wstring value=L"original";assert(store.write(L"A",L"One",value.c_str(),L"file.ini"));value.assign(L"changed");
 assert(store.write(L"A",L"Two",L"second",L"file.ini"));
 assert(!store.write(L"A",L"Three",L"rejected",L"file.ini"));
 assert(store.status().pending==2&&store.status().failed&&store.status().errorCode==asyncini::core::QueueFull);
 // A full queue still admits a replacement of an already queued key.
 assert(store.write(L"A",L"TWO",L"latest",L"FILE.INI"));
 gate.release();eventually([&]{return store.idle();});
 assert(disk.get(L"A",L"One",L"file.ini")==L"original");
 assert(disk.get(L"A",L"Two",L"file.ini")==L"latest");
 assert(disk.get(L"A",L"Three",L"file.ini")==L"<missing>");
 assert(store.status().failed);store.acknowledgeError();assert(!store.status().failed);
 store.beginShutdown();eventually([&]{return store.status().stopped;});
 assert(!store.write(L"A",L"After",L"stop",L"file.ini"));
}

void failure_and_ordered_retry(){
 Disk disk;disk.apply(Operation(L"Section",L"Unknown",L"old",L"file.ini"));
 std::atomic<bool>failDelete{true};Store store([&](const Operation&o){
  if(o.kind==Kind::DeleteSection&&failDelete.load())return Result{false,5};
  disk.apply(o);return Result{};
 });
 assert(store.write(L"Section",nullptr,nullptr,L"file.ini"));eventually([&]{return store.idle();});
 auto s=store.status();assert(s.failed&&s.errorCode==5&&s.errorPath==L"file.ini");
 assert(store.write(L"Section",L"New",L"keep-after-delete",L"file.ini"));
 assert(store.write(L"Other",L"Unrelated",L"success",L"other.ini"));eventually([&]{return store.idle();});
 assert(store.status().failed);auto kept=store.lookup(L"Section",L"New",L"file.ini");assert(kept.known&&kept.value==L"keep-after-delete");
 assert(!store.lookup(L"Section",L"Unknown",L"file.ini").present);
 failDelete=false;assert(store.retryFailed(L"FILE.INI"));eventually([&]{return store.idle();});
 assert(!store.status().failed);
 assert(disk.get(L"Section",L"Unknown",L"file.ini")==L"<missing>");
 assert(disk.get(L"Section",L"New",L"file.ini")==L"keep-after-delete");
 assert(disk.get(L"Other",L"Unrelated",L"other.ini")==L"success");
 assert(!store.lookup(L"Section",L"New",L"file.ini").known);
}

void multiple_producers_and_notifications(){
 Disk disk;Store store([&](const Operation&o){disk.apply(o);return Result{};},4096);
 std::atomic<unsigned>notifications{0};store.setNotify([&]{++notifications;});
 std::vector<std::thread> producers;
 for(unsigned n=0;n<6;++n)producers.emplace_back([&,n]{
  const auto section=L"Producer"+std::to_wstring(n);
  for(unsigned i=0;i<250;++i){const auto value=std::to_wstring(i);assert(store.write(section.c_str(),L"Value",value.c_str(),L"settings.ini"));}
 });
 for(auto&thread:producers)thread.join();
 assert(store.requestFlush());
 eventually([&]{return store.idle();});assert(notifications>0);
 for(unsigned n=0;n<6;++n)assert(disk.get((L"Producer"+std::to_wstring(n)).c_str(),L"Value",L"settings.ini")==L"249");
 assert(store.status().completedRevision==1501);
 store.setNotify({});const auto before=notifications.load();
 assert(store.write(L"After",L"Unsubscribe",L"1",L"settings.ini"));eventually([&]{return store.idle();});assert(notifications==before);
}

void notification_edges_do_not_flood_ui(){
 Gate gate;std::atomic<unsigned>calls{0},notices{0};
 Store store([&](const Operation&){if(calls++==0)gate.wait();return Result{};},256);
 store.setNotify([&]{++notices;}); // One initial status, then one saving edge.
 assert(store.write(L"Hold",L"Key",L"1",L"file.ini"));gate.await();
 for(unsigned i=0;i<120;++i)assert(store.write(L"Values",std::to_wstring(i).c_str(),L"1",L"file.ini"));
 assert(notices==2);
 gate.release();eventually([&]{return store.idle();});eventually([&]{return notices==3;});
 store.setNotify({});
}

void cache_barrier_keeps_real_write_failure(){
 std::atomic<bool>failWrite{true};std::atomic<unsigned>barriers{0};Disk disk;
 Store store([&](const Operation&o){
  if(o.kind==Kind::Flush){++barriers;return asyncini::core::nativeResult(o.kind,false,2);}
  if(failWrite.load())return asyncini::core::nativeResult(o.kind,false,5);
  disk.apply(o);return Result{};
 });
 assert(store.write(L"Section",L"Value",L"new",L"file.ini"));
 assert(store.requestFlush(L"file.ini"));eventually([&]{return store.idle();});
 assert(barriers==1);assert(store.status().failed&&store.status().errorCode==5);
 assert(disk.get(L"Section",L"Value",L"file.ini")==L"<missing>");
 failWrite=false;assert(store.retryFailed(L"file.ini"));eventually([&]{return store.idle();});
 assert(!store.status().failed);assert(disk.get(L"Section",L"Value",L"file.ini")==L"new");
}

void decoding(){
 using namespace asyncini::core;
 assert(readValue(L"\"quoted\"")==L"quoted");assert(readValue(L"'single'")==L"single");
 assert(readInteger(L"123",99)==123);assert(readInteger(L"010",99)==10);
 assert(readInteger(L"0xFF",99)==255);assert(readInteger(L"-1",99)==0xffffffffu);
 assert(readInteger(L"",99)==99);assert(readInteger(L"not-number",99)==0);
 assert(nativeResult(Kind::Flush,false,0).ok);
 assert(nativeResult(Kind::Flush,false,2).ok);
 assert(nativeResult(Kind::Flush,false,5).ok); // Management return has no write-result contract.
 assert(!nativeResult(Kind::Set,false,0).ok&&nativeResult(Kind::Set,false,0).error==WriteFault);
 assert(!nativeResult(Kind::DeleteKey,false,0).ok);
 assert(nativeResult(Kind::Set,true,5).ok);
}
#ifdef _WIN32
std::wstring raw_file_text(const wchar_t*path,bool requireUtf16=false){
 HANDLE handle=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 assert(handle!=INVALID_HANDLE_VALUE);LARGE_INTEGER size{};assert(GetFileSizeEx(handle,&size)&&size.QuadPart>=2&&size.QuadPart<=1024*1024);
 std::vector<unsigned char>data(size_t(size.QuadPart));DWORD read=0;
 assert(ReadFile(handle,data.data(),DWORD(data.size()),&read,nullptr)&&read==data.size());CloseHandle(handle);
 const bool utf16=data[0]==0xff&&data[1]==0xfe;assert(!requireUtf16||utf16);std::wstring text;
 if(utf16){assert(data.size()%2==0);for(size_t i=2;i+1<data.size();i+=2)text.push_back(wchar_t(unsigned(data[i])|(unsigned(data[i+1])<<8)));}
 else{
  // A newly created profile is ANSI unless an existing UTF16 BOM selects
  // Unicode. Read the actual bytes rather than the profile mapping cache.
  const auto count=MultiByteToWideChar(CP_ACP,0,reinterpret_cast<const char*>(data.data()),int(data.size()),nullptr,0);assert(count>0);
  text.resize(size_t(count));assert(MultiByteToWideChar(CP_ACP,0,reinterpret_cast<const char*>(data.data()),int(data.size()),text.data(),count)==count);
 }
 return text;
}
void native_win32_file_roundtrip(){
 wchar_t temporary[MAX_PATH+1]{},file[MAX_PATH+1]{};
 assert(GetTempPathW(MAX_PATH,temporary));assert(GetTempFileNameW(temporary,L"rai",0,file));
 HANDLE handle=CreateFileW(file,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 assert(handle!=INVALID_HANDLE_VALUE);const unsigned char bom[]={0xff,0xfe};DWORD bytes=0;
 assert(WriteFile(handle,bom,2,&bytes,nullptr)&&bytes==2);CloseHandle(handle);
 assert(asyncini::nativeWrite(L"Other",L"Unknown",L"保留",file));
 assert(asyncini::nativeWrite(L"Section",L"Value",L"before",file));
 SetLastError(ERROR_SUCCESS);const BOOL flushReturn=asyncini::nativeWrite(nullptr,nullptr,nullptr,file);const DWORD flushError=GetLastError();
 std::cout<<"native profile-cache refresh probe: return="<<flushReturn<<" lastError="<<flushError<<" (management call; not a file write result)"<<std::endl;
 assert(asyncini::write(L"Section",L"Value",L"\"quoted\"",file));
 wchar_t value[128]{};assert(asyncini::cachedReadString(L"section",L"value",L"",value,128,file)==6);assert(std::wstring(value)==L"quoted");
 assert(asyncini::write(L"Section",L"Number",L"010",file));assert(asyncini::cachedReadInt(L"SECTION",L"NUMBER",99,file)==10);
 assert(asyncini::requestFlush(file));eventually([]{return asyncini::idle();});
 if(asyncini::status().failed){const auto failed=asyncini::status();std::wcerr<<L"unexpected INI persistence failure: code="<<failed.errorCode<<L" revision="<<failed.errorRevision<<L" path="<<failed.errorPath<<std::endl;}
 assert(!asyncini::status().failed);
 const auto raw=raw_file_text(file,true);
 assert(raw.find(L"Unknown=保留")!=std::wstring::npos);
 assert(raw.find(L"Value=\"quoted\"")!=std::wstring::npos);
 assert(raw.find(L"Number=010")!=std::wstring::npos);
 assert(raw.find(L"Value=before")==std::wstring::npos);
 asyncini::nativeRead(L"Other",L"Unknown",L"",value,128,file);assert(std::wstring(value)==L"保留");
 asyncini::nativeRead(L"Section",L"Value",L"",value,128,file);assert(std::wstring(value)==L"quoted");
 assert(asyncini::nativeInt(L"Section",L"Number",99,file)==asyncini::cachedReadInt(L"Section",L"Number",99,file));

 const std::wstring directory=std::wstring(file)+L"-directory",badPath=directory+L"\\settings.ini";
 assert(asyncini::write(L"Section",L"Retry",L"saved",badPath.c_str()));eventually([]{return asyncini::idle();});
 assert(asyncini::status().failed&&asyncini::status().errorPath==badPath);
 assert(asyncini::requestFlush(badPath.c_str()));eventually([]{return asyncini::idle();});
 assert(asyncini::status().failed&&asyncini::status().errorPath==badPath);
 assert(CreateDirectoryW(directory.c_str(),nullptr));assert(asyncini::retryFailed(badPath.c_str()));
 eventually([]{return asyncini::idle();});assert(!asyncini::status().failed);
 asyncini::nativeRead(L"Section",L"Retry",L"",value,128,badPath.c_str());assert(std::wstring(value)==L"saved");
 assert(raw_file_text(badPath.c_str()).find(L"Retry=saved")!=std::wstring::npos);
 assert(asyncini::requestFlush());asyncini::beginShutdown();eventually([]{return asyncini::idle();});
 asyncini::setNotify({});assert(DeleteFileW(file));assert(DeleteFileW(badPath.c_str()));assert(RemoveDirectoryW(directory.c_str()));
}
#endif
int main(){
 coalescing_and_barriers();owned_values_and_bounded_admission();failure_and_ordered_retry();multiple_producers_and_notifications();notification_edges_do_not_flood_ui();cache_barrier_keeps_real_write_failure();decoding();
#ifdef _WIN32
 native_win32_file_roundtrip();
#endif
 std::cout<<"async INI store: owned values, barriers, retry, bounded queues, notifications and concurrent final state passed\n";
}
