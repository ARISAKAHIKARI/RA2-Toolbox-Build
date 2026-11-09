#define main originalSharedSceneTests
#include "shared_scene_sampling.cpp"
#undef main
#include <cstdlib>
#include <new>
#include <chrono>
#include <iostream>
static bool tracking=false;static size_t allocated=0,calls=0,largeCalls=0;
void*operator new(size_t n){if(tracking){allocated+=n;++calls;if(n>=512*1024)++largeCalls;}void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
int main(){
 using namespace external;setup();radar::SceneBank bank;radar::SharedSceneProvider<Reader>provider(bank);
 auto first=token(1,10,100);Reader firstReader(first);auto initial=acquire(provider,firstReader,radar::SceneConsumer::Minimap,100,true,true);assert(initial);
 size_t populated=0;for(auto&cell:initial->terrain->art)populated+=cell.cell!=0;
 std::cout<<"actual_cells="<<populated<<" grid_slots="<<initial->terrain->art.size()<<" CellArt_bytes="<<sizeof(radar::CellArt)<<" terrain_vectors_bytes="<<(initial->terrain->colors.size()*4+initial->terrain->baseColors.size()*4+initial->terrain->shade.size()*sizeof(radar::ShroudFrames)+initial->terrain->art.size()*sizeof(radar::CellArt))<<'\n';
 for(unsigned i=2;i<=10;++i){auto next=token(i,9+i,100+16*i);Reader reader(next);allocated=calls=largeCalls=0;tracking=true;
  auto captured=acquire(provider,reader,radar::SceneConsumer::Minimap,100+16*i,true,true);tracking=false;assert(captured&&captured->terrain!=initial->terrain&&captured->terrain->art[Edge].cell==Cell);
  if(i>=4)assert(allocated<200000&&largeCalls==0);
  std::cout<<"publication="<<i<<" allocation_bytes="<<allocated<<" allocation_calls="<<calls<<" large_allocations="<<largeCalls<<" physical_cell="<<captured->terrain->art[Edge].cell<<'\n';
  auto count=provider.statistics().captures;allocated=calls=largeCalls=0;tracking=true;auto same=acquire(provider,reader,radar::SceneConsumer::Watch,100+16*i,true,true);tracking=false;assert(same==captured&&provider.statistics().captures==count);
  std::cout<<"same_publication allocation_bytes="<<allocated<<" calls="<<calls<<'\n';
 }
}
