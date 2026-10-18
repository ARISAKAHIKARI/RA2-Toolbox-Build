#include "../source/icon_cache.h"
#include <cassert>
#include <iostream>
int main(){
 namespace fs=std::filesystem;auto root=fs::temp_directory_path()/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(root);
 auto file=root/"icons.bin";um::iconcache::Record r{};r.category=1;strcpy(r.id,"HARV");strcpy(r.cameo,"harvicon.shp");r.icon[44]=12;r.palette[12]=0xff123456;
 assert(um::iconcache::write(file,42,{r}));auto records=um::iconcache::read(file,42);assert(records.size()==1&&records[0].icon[44]==12&&records[0].palette[12]==0xff123456);
 assert(um::iconcache::read(file,43).empty());r.icon[44]=19;assert(um::iconcache::write(file,42,{r}));assert(um::iconcache::read(file,42)[0].icon[44]==19);
 {std::fstream f(file,std::ios::in|std::ios::out|std::ios::binary);f.seekp(sizeof(um::iconcache::Header)+50);f.put(77);}assert(um::iconcache::read(file,42).empty());
 {std::ofstream f(file,std::ios::binary|std::ios::trunc);um::iconcache::Header h;h.scope=42;h.count=~0u;f.write((char*)&h,sizeof h);}assert(um::iconcache::read(file,42).empty());
 auto exe=root/"gamemd.exe";{std::ofstream f(exe);f<<"same executable";}auto first=um::iconcache::scope(exe);assert(first);
 {std::ofstream f(root/"rulesmd.ini");f<<"[HARV]\nImage=OTHER";}auto second=um::iconcache::scope(exe);assert(first!=second);
 {std::ofstream f(root/"spawn.ini");f<<"unrelated player name";}assert(um::iconcache::scope(exe)==second);
 {std::ofstream f(root/"spawnmap.ini");f<<"per-match generated rules";}
 {std::ofstream f(root/"spawnmap.yrm");f<<"temporary map";}
 {std::ofstream f(root/"Assist.ini");f<<"user changed a hotkey";}
 assert(um::iconcache::scope(exe)==second);
 {std::ofstream f(root/"rulesmd.ini");f<<"[HARV]\nImage=CHANG";}assert(um::iconcache::scope(exe)!=second);
 assert(um::iconcache::scope(root/"different.exe")!=um::iconcache::scope(exe));
 auto scope=um::iconcache::scope(exe);auto folder=root/"cache";assert(um::iconcache::write(folder/(std::to_string(scope)+".bin"),scope,{r}));
 {um::IconCache cache;cache.open(exe,folder);for(int i=0;i<200&&!cache.loaded;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));assert(cache.loaded==1);
  um::Type t{};t.category=1;strcpy(t.id,"HARV");strcpy(t.cameo,"harvicon.shp");auto snap=std::make_unique<um::Snapshot>();
  assert(cache.restore(t,*snap)&&t.iconReady==2&&snap->paletteReady&&t.icon[44]==19);
  t.iconReady=1;t.icon[44]=33;snap->paletteReady=0;assert(cache.restorePalette(t,*snap)&&t.icon[44]==33&&snap->paletteReady);
  assert(cache.remember(t,*snap)); // destructor flushes pending data before joining the worker
 }
 assert(um::iconcache::read(folder/(std::to_string(scope)+".bin"),scope)[0].icon[44]==33);

 // A learned icon is retained even while the initial resource scan is running.
 auto shared=root/"shared";
 {um::IconCache cache;cache.open(exe,shared);um::Type t{};t.category=3;strcpy(t.id,"GAPOWR");strcpy(t.cameo,"POWRICON.SHP");t.iconReady=1;t.icon[10]=55;
  auto snap=std::make_unique<um::Snapshot>();snap->paletteReady=1;snap->palette[55]=0xff445566;assert(cache.remember(t,*snap));
  // Monitor and reservation attachment are idempotent; neither resets learning.
  cache.open(exe,shared);cache.open(exe,shared);
 }
 {um::IconCache cache;cache.open(exe,shared);for(int i=0;i<200&&!cache.loaded;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
  um::Type t{};t.category=3;strcpy(t.id,"gapowr");strcpy(t.cameo,"powricon.shp");auto snap=std::make_unique<um::Snapshot>();assert(cache.restore(t,*snap)&&t.icon[10]==55&&snap->palette[55]==0xff445566);
  // Across a match, all game pointers may change while the art key is stable.
  t={};t.ptr=0x76543210;t.category=3;strcpy(t.id,"GAPOWR");strcpy(t.cameo,"POWRICON.SHP");assert(cache.restore(t,*snap));
 }
 auto beforePlugin=um::iconcache::scope(exe);{std::ofstream f(root/"GameRuntime.dll");f<<"new plugin build";}assert(um::iconcache::scope(exe)==beforePlugin);
 // Migrate both former consumers, with identical-key records deduplicated.
 auto legacy=um::iconcache::scope(exe,true);auto oldRoot=root/"migration";
 assert(um::iconcache::write(oldRoot/"unit-icons"/(std::to_string(legacy)+".bin"),legacy,{r}));
 auto r2=r;r2.category=3;strcpy(r2.id,"GAPOWR");strcpy(r2.cameo,"powricon.shp");assert(um::iconcache::write(oldRoot/"production-icons"/(std::to_string(legacy)+".bin"),legacy,{r2}));
 {um::IconCache cache;cache.open(exe,oldRoot/"icons");for(int i=0;i<200&&cache.loaded<2;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));assert(cache.loaded==2);}
 assert(um::iconcache::read(oldRoot/"icons"/(std::to_string(beforePlugin)+".bin"),beforePlugin).size()==2);
 assert(&um::sharedIconCache()==&um::sharedIconCache());
 std::cout<<"PASS: pending learn, process restart, case-normalized keys, cross-match pointers, plugin-independent scope, legacy consumer merge\n";
 fs::remove_all(root);std::cout<<"PASS: icon roundtrip, atomic replacement, scope isolation, corruption/bounds, resource invalidation\n";
}

