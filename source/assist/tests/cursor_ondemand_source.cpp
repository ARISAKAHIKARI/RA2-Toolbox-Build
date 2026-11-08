// Actual source adoption, display rebuilding and immutable cache publication.
// Win32 resource delivery and filesystem calls are controlled boundaries.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../../cursor/source/core.h"
#include "../../cursor/source/ondemand.h"
#include "../../cursor/source/bridge.h"
#include "../../cursor/source/playback.h"
#include "../../cursor/source/super_hotspot.h"
#include "../../cursor/source/snapshot_latch.h"

static Asset asset,displayAsset;
static Bytes original,displaySourceColors,workOriginal,workColors,workGroups;
static DWORD palette[256]{},displayPalette[256]{};
static bool loaded=false,displayLoaded=false,running=false,gameResourceConfirmed=false;
static std::wstring root,gameDir,cacheKey,profilePath,profileId,workProfile,materializedProfile;
static std::set<unsigned> allowedPairs,playbackPendingPairs;
static std::map<unsigned,std::shared_ptr<playback::Timeline>> motionCache;
static std::vector<motion::Image> displayCanvases;
static std::vector<playback::Frame> displayFrames;
static playback::RasterCache rasterCache;
static std::shared_ptr<playback::Timeline> activeMotion;
static std::shared_ptr<std::atomic<bool>> playbackCancel;
static superhot::Centers superCenters;
static bool playbackReady=false,playbackPending=false,playbackFullPending=false;
static bool prepareAutomatic=false,previewQueued=false;
static HANDLE prepareProcess=nullptr;
static cursorondemand::Queue prepareQueue;
static unsigned motionEpoch=0,resourceTries=0;
static size_t motionBytes=0,lastPhase=0;
static int lastFrame=0;
static cursorsnap::Latch bridgeFrame;
static struct ResetCounter {unsigned calls=0;void reset(){++calls;}} streamShapeGate,fallbackTracker;
static Shared sharedStorage{};
static Shared* shared=&sharedStorage;
static uintptr_t hook=1;
static DWORD resourceRetryAt=0,clockNow=10000;
static bool tearAtBarrier=false;
static unsigned wakes=0,barriers=0,stops=0,cancels=0;
static std::wstring lastLog;
static std::map<std::wstring,Bytes> disk;
static std::set<std::wstring> directories;
struct Io {unsigned exists=0,reads=0,writes=0,moves=0,deletes=0,directories=0,pairs=0;
 unsigned total()const{return exists+reads+writes+moves+deletes+directories+pairs;}};
static Io io;
static unsigned checks=0,failures=0,scenarios=0;
static void check(bool condition,const char* reason){++checks;if(!condition){++failures;std::cerr<<"FAIL "<<reason<<'\n';}}
[[noreturn]] static void fail(const char* message){throw std::runtime_error(message);}
static bool exists(const std::wstring& name){++io.exists;return disk.count(name)!=0;}
static Bytes file(const std::wstring& name){++io.reads;auto i=disk.find(name);if(i==disk.end())fail("fixture missing file");return i->second;}
static void save(const std::wstring& name,const Bytes& bytes){++io.writes;disk[name]=bytes;}
static std::wstring widen(const std::string& value){return {value.begin(),value.end()};}
static void log(const std::wstring& value){lastLog=value;}
static bool manuallyBlankSHA(){return false;}
static DWORD GetTickCount(){return clockNow;}
static uint64_t GetTickCount64(){return clockNow;}
static DWORD GetCurrentProcessId(){return 42;}
static void MemoryBarrier(){++barriers;if(tearAtBarrier){shared->resourceSeq+=2;tearAtBarrier=false;}}
namespace gameinput {static void wake(uintptr_t){++wakes;}}
static void stop(){++stops;running=false;}
static bool cancelPrepare(){++cancels;return true;}
static void fixtureCreateDirectories(const std::wstring& path){++io.directories;directories.insert(path);}
static BOOL MoveFileExW(const wchar_t* from,const wchar_t* to,DWORD flags){
 ++io.moves;check(flags==MOVEFILE_WRITE_THROUGH,"immutable inputs use write-through without replace-existing");
 auto i=disk.find(from);if(i==disk.end()||disk.count(to))return FALSE;
 disk[to]=i->second;disk.erase(i);return TRUE;
}
static BOOL DeleteFileW(const wchar_t* path){++io.deletes;return disk.erase(path)?TRUE:FALSE;}
static std::istringstream fixturePreparedPairStream(){
 ++io.pairs;auto i=disk.find(workProfile+L"\\pairs.txt");
 if(i==disk.end()){std::istringstream out;out.setstate(std::ios::failbit);return out;}
 return std::istringstream(std::string(i->second.begin(),i->second.end()));
}
#include SOURCE_PRODUCTION

static Bytes text(const char* value){return Bytes(value,value+std::strlen(value));}
static Asset artwork(unsigned frames,uint8_t ink=1){
 Asset out;out.w=55;out.h=43;out.frames.resize(frames);
 for(unsigned i=0;i<frames;++i)out.frames[i]={uint16_t(4+i%3),uint16_t(6+i%2),2,2,{ink,0,ink,ink}};
 return out;
}
static Bytes colors(uint32_t ink=0x80804020){
 Bytes out(1024);w32(out,0,0xffffffff);w32(out,4,ink);w32(out,8,0xff4060a0);return out;
}
static Bytes normalized(Bytes out){std::fill_n(out.begin(),4,uint8_t(0));return out;}
static void offer(const Bytes& bytes,const Bytes& colors,LONG seq=2){
 check(bytes.size()<=RESOURCE_MAX_BYTES&&colors.size()==1024,"fixture fits actual shared resource bounds");
 shared->resourceBytes=DWORD(bytes.size());shared->resourceSeq=seq;shared->resourceState=2;
 std::memcpy(shared->resourcePixels,bytes.data(),bytes.size());std::memcpy(shared->resourcePalette,colors.data(),1024);
}
static void reset(){
 ++scenarios;disk.clear();directories.clear();std::memset(&sharedStorage,0,sizeof(sharedStorage));shared=&sharedStorage;
 root=L"C:\\RA2 Toolbox 中文";gameDir=L"C:\\Games\\YR";cacheKey=root+L"\\cache\\game";profilePath=root+L"\\assets";profileId.clear();
 disk[profilePath+L"\\groups.json"]=text("[{\"start\":0,\"count\":3,\"loop\":false}]");
 asset=artwork(450,2);original=encode(asset,false);const auto pal=normalized(colors());std::memcpy(palette,pal.data(),1024);
 loaded=true;displayLoaded=false;running=true;gameResourceConfirmed=false;motionEpoch=0;
 prepareProcess=nullptr;prepareAutomatic=previewQueued=false;playbackCancel.reset();prepareQueue.reset();allowedPairs={pairKey(0,1),pairKey(1,2)};
 resourceRetryAt=resourceTries=0;clockNow=10000;tearAtBarrier=false;wakes=barriers=stops=cancels=0;lastLog.clear();
 streamShapeGate.calls=fallbackTracker.calls=0;bridgeFrame.clear();syncDefaultDisplay();resetPrepared(false);io={};
}
static void displayMatches(const Asset& expected,const Bytes& colors){
 check(encode(displayAsset,false)==encode(expected,false),"default display adopts actual decoded source frames");
 check(displaySourceColors==normalized(colors),"display keeps source BGRA bytes with transparent index zero");
 check(displayLoaded&&displayCanvases.size()==expected.frames.size()&&displayFrames.size()==expected.frames.size(),"actual rebuilt display has every captured source frame");
 const auto& first=expected.frames.front();const unsigned at=first.y*expected.w+first.x;
 const auto raw=u32(colors,size_t(first.pixels[0])*4);const auto want=motion::premultiplyRGB(raw&0xffffff,int(raw>>24));
 check(displayCanvases.front()[at]==want&&displayFrames.front().at(first.x,first.y)==want,"real canvas and packed display use captured premultiplied palette");
 check(displayPalette[0]==0,"transparent palette index zero stays transparent");
}
static bool materializeFails(){try{materializeWorkProfile();return false;}catch(const std::exception&){return true;}}
int main(){try{
 for(unsigned count:{450u,517u}){
  reset();const Asset captured=artwork(count);const auto bytes=encode(captured,false),pal=colors();const auto previousEpoch=motionEpoch;
  auto cancelled=std::make_shared<std::atomic<bool>>(false);playbackCancel=cancelled;prepareQueue.request(pairKey(0,1),450);
  activeMotion=std::make_shared<playback::Timeline>();motionCache[pairKey(0,1)]=activeMotion;motionBytes=123;playbackPendingPairs.insert(pairKey(0,1));
  offer(bytes,pal);pollOriginalResource();
  check(gameResourceConfirmed&&shared->resourceState==3,"committed visible game source is confirmed and consumed");
  check(original==bytes&&encode(asset,false)==bytes,"actual decode/encode canonical source is adopted");
  check(shared->cursorFrameCount==LONG(count)&&shared->cursorCanvasW==55&&shared->cursorCanvasH==43,"bridge acknowledgement reports captured dimensions and count");
  check(workOriginal==bytes&&workColors==normalized(pal),"work source matches captured original and palette");
  check(workProfile==root+L"\\cache\\cursor-dis-v1\\"+cursorondemand::sourceKey(bytes,normalized(pal),workGroups),"actual reset chooses source-content identity");
  check(motionEpoch==previousEpoch+1&&cancelled->load()&&prepareQueue.empty()&&motionCache.empty()&&motionBytes==0&&!activeMotion,"changed source invalidates resident and pending old-source work");
  check(playbackReady&&playbackPending&&playbackFullPending&&playbackPendingPairs.empty()&&running&&stops==0,"new source displays original frames without stopping running cursor");
  displayMatches(captured,pal);
 }

 reset();const auto captured=artwork(517);const auto bytes=encode(captured,false),pal=colors();offer(bytes,pal);pollOriginalResource();
 const auto firstKey=workProfile;auto changedPal=pal;w32(changedPal,4,0xc04070a0);const auto oldEpoch=motionEpoch;
 offer(bytes,changedPal,4);pollOriginalResource();
 check(workOriginal==bytes&&workProfile!=firstKey&&motionEpoch==oldEpoch+1,"same SHA with a changed palette creates a new cache identity");displayMatches(captured,changedPal);
 const auto sameKey=workProfile;const auto sameOriginal=original;const auto sameEpoch=motionEpoch;io={};offer(bytes,changedPal,6);io={};pollOriginalResource();
 check(workProfile==sameKey&&original==sameOriginal&&motionEpoch==sameEpoch&&io.total()==0,"identical captured bytes reuse identity without resetting or touching disk");
 auto indexZeroOnly=changedPal;w32(indexZeroOnly,0,0x76543210);offer(bytes,indexZeroOnly,8);io={};pollOriginalResource();
 check(workProfile==sameKey&&motionEpoch==sameEpoch&&io.total()==0,"unused transparent-index color changes do not create spurious work");
 ++scenarios;disk[profilePath+L"\\groups.json"]=text("[{\"start\":0,\"count\":4}]");resetPrepared(false);
 check(workProfile!=sameKey&&workOriginal==bytes&&workColors==normalized(changedPal),"groups-only change selects a new cache identity without changing source art");

 reset();profileId=L"custom-theme";profilePath=root+L"\\profiles\\custom-theme";disk[profilePath+L"\\groups.json"]=text("[{\"start\":0,\"count\":2}]");
 displayAsset=artwork(450,2);displaySourceColors=normalized(colors(0xff103050));std::memcpy(displayPalette,displaySourceColors.data(),1024);
 for(auto& c:displayPalette)c=motion::premultiplyRGB(c&0xffffff,int(c>>24));displayLoaded=true;resetPrepared(false);
 const auto customOriginal=workOriginal,customColors=workColors;const auto customKey=workProfile;const auto customPixel=displayCanvases.front()[6*55+4];
 offer(bytes,pal);pollOriginalResource();
 check(gameResourceConfirmed&&asset.frames.size()==517,"custom theme still allows current game source confirmation");
 check(profileId==L"custom-theme"&&workOriginal==customOriginal&&workColors==customColors&&workProfile==customKey,"game source cannot replace explicitly chosen theme inputs or identity");
 check(displayAsset.frames.size()==450&&displayCanvases.front()[6*55+4]==customPixel,"custom theme remains the display source for its own frames");
 check(displayCanvases.size()==517&&displayCanvases.back()==frameCanvas(516),"extended game-only frames retain the existing fallback route");

 const auto good=encode(artwork(450),false);std::vector<Bytes> invalid;
 invalid.push_back(Bytes{0,1,2,3,4,5,6});auto broken=good;broken.pop_back();invalid.push_back(broken);
 broken=good;w16(broken,2,54);w16(broken,8,53);invalid.push_back(broken);
 invalid.push_back(encode(artwork(449),false));invalid.push_back(encode(artwork(450),true));
 broken=good;w16(broken,8,55);invalid.push_back(broken);
 for(const auto& bad:invalid){
  reset();const auto previous=original;const auto key=workProfile;const auto previousEpoch=motionEpoch;offer(bad,pal);io={};pollOriginalResource();
  check(!gameResourceConfirmed&&original==previous&&workProfile==key&&motionEpoch==previousEpoch,"bad, unsupported or transparent SHA cannot confirm or replace source");
  check(shared->resourceState==3&&io.writes==0&&io.moves==0,"rejected resource is consumed without disk publication");
 }
 for(DWORD count:{0u,RESOURCE_MAX_BYTES+1u}){
  reset();offer(good,pal);shared->resourceBytes=count;io={};pollOriginalResource();
  check(!gameResourceConfirmed&&shared->resourceState==3&&barriers==0&&io.total()==0,"out-of-bounds resource byte count is rejected before copying or disk work");
 }
 for(LONG seq:{0,1,3}){
  reset();const auto previous=original;const auto key=workProfile;offer(good,pal,seq);io={};pollOriginalResource();
  check(!gameResourceConfirmed&&original==previous&&workProfile==key&&shared->resourceState==2&&barriers==0&&io.total()==0,"uncommitted resource never becomes a confirmed source");
 }
 reset();const auto beforeTear=original;const auto beforeTearKey=workProfile;offer(good,pal);tearAtBarrier=true;io={};pollOriginalResource();
 check(!gameResourceConfirmed&&original==beforeTear&&workProfile==beforeTearKey&&shared->resourceState==2&&io.total()==0,"sequence change across copied payload rejects torn source without publication");
 pollOriginalResource();check(gameResourceConfirmed&&original==good&&shared->resourceState==3,"a subsequent coherent resource can confirm after a torn attempt");

 reset();offer(bytes,pal);pollOriginalResource();io={};materializeWorkProfile();
 check(io.directories==1&&io.writes==3&&io.moves==3&&materializedProfile==workProfile,"first materialization atomically publishes exactly three immutable inputs");
 check(disk.at(workProfile+L"\\original.sha")==workOriginal&&disk.at(workProfile+L"\\palette.bgra")==workColors&&disk.at(workProfile+L"\\groups.json")==workGroups,"full source, palette and groups bytes are materialized unchanged");
 check(!disk.count(workProfile+L"\\pairs.txt"),"derived pair index is not staged as an immutable input");
 io={};for(unsigned i=0;i<100;++i)materializeWorkProfile();const unsigned hotIo=io.total();check(hotIo==0,"one hundred hot-epoch materializations perform no filesystem operations");
 const auto sourceKey=workProfile;const auto generatedPairs=text("0 1\n1 2\n450 451\n516 515\n");disk[workProfile+L"\\pairs.txt"]=generatedPairs;
 readPreparedPairs();check(allowedPairs==std::set<unsigned>{pairKey(0,1),pairKey(1,2),pairKey(450,451),pairKey(516,515)},"generated pair index admits valid extended game frames");
 io={};materializeWorkProfile();check(io.total()==0&&workProfile==sourceKey,"engine pair-index rewrite preserves hot cache identity and causes no restaging");
 resetPrepared(false);io={};materializeWorkProfile();
 check(workProfile==sourceKey&&io.reads==3&&io.writes==0&&io.moves==0&&disk.at(workProfile+L"\\pairs.txt")==generatedPairs,"new epoch reuses three byte-equal inputs and preserves derived pair metadata");
 for(const auto name:{L"original.sha",L"palette.bgra",L"groups.json"}){
  ++scenarios;const auto target=workProfile+L"\\"+name;const auto saved=disk.at(target);disk[target].back()^=0x5a;const auto conflicting=disk.at(target);materializedProfile.clear();io={};
  check(materializeFails(),"conflicting immutable input rejects claimed cache identity");
  check(materializedProfile.empty()&&disk.at(target)==conflicting&&io.writes==0&&io.moves==0,"identity mismatch neither overwrites existing input nor marks cache ready");disk[target]=saved;
 }
 materializedProfile.clear();materializeWorkProfile();check(materializedProfile==workProfile,"restoring exact source bytes makes cache reusable");

 ++scenarios;const auto keep=allowedPairs;disk.erase(workProfile+L"\\pairs.txt");readPreparedPairs();check(allowedPairs==keep,"absent derived pair index retains fallback eligibility");
 disk[workProfile+L"\\pairs.txt"]=text("0 1\nnot-a-pair\n");readPreparedPairs();check(allowedPairs==keep,"non-numeric derived pair corruption cannot replace eligibility");
 std::ostringstream tooMany;for(unsigned i=0;i<513;++i)tooMany<<i<<' '<<i+1<<'\n';const auto excessive=tooMany.str();disk[workProfile+L"\\pairs.txt"]=Bytes(excessive.begin(),excessive.end());readPreparedPairs();
 check(allowedPairs==keep,"derived eligibility remains bounded to 512 pairs");
 for(const auto invalidPairs:{"0 1\n1", "0 1\n999999999999999999999", "0 1\n-1 2\n", "0 1\n517 1\n", "0 1\n2 2\n"}){
  disk[workProfile+L"\\pairs.txt"]=text(invalidPairs);readPreparedPairs();
  check(allowedPairs==keep,"incomplete/overflow/self/out-of-range derived index cannot replace eligibility");
 }
 disk[workProfile+L"\\pairs.txt"]=text("0 1\n0 1\n516 515\n");readPreparedPairs();
 check(allowedPairs==std::set<unsigned>{pairKey(0,1),pairKey(516,515)},"derived parser deduplicates valid entries and admits the final supported frame");
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"scenarios\":"<<scenarios<<",\"hot_materialize_calls\":100,\"hot_filesystem_calls\":"<<hotIo<<"}\n";
 return failures?1:0;
}catch(const std::exception& e){std::cerr<<"Unexpected fixture exception: "<<e.what()<<'\n';return 2;}}
