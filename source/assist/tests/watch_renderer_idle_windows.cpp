#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/world_scene_pipeline.h"
#include "../source/watch_scene.h"
#include <cassert>
#include <cstdio>
struct Reader {
 unsigned reads=0;
 bool read(uint32_t,void*,size_t){++reads;return false;}
 bool readFresh(uint32_t p,void*out,size_t n){return read(p,out,n);}
 bool operator()(uint32_t p,void*out,size_t n){return read(p,out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
};
int main(){
 Reader reader;external::cameraassets::Context<Reader> assets(reader);watchscene::Renderer<Reader,decltype(assets)> renderer(reader,assets);
 renderer.views[0].shown=true;renderer.views[0].scene=std::make_shared<external::radar::Scene>();
 assert(renderer.update(1)&&reader.reads==0&&!renderer.views[0].shown&&!renderer.views[0].scene&&!renderer.timings.prepared);
 renderer.danger=true;renderer.autoSuppressed=true;assert(renderer.update(2)&&reader.reads==0&&!renderer.timings.prepared);
 renderer.tracks[0].target={0x20000,1};assert(!renderer.update(3)&&reader.reads>0&&bool(renderer.tracks[0]));
 renderer.tracks[0]={};reader.reads=0;renderer.autoSuppressed=false;assert(!renderer.update(4)&&reader.reads>0);
 std::puts("PASS production watch Renderer: empty explicitly disabled/suppressed cameras issue zero game reads and clear old output; pinned/automatic demand still takes validated sampling path.");
}
