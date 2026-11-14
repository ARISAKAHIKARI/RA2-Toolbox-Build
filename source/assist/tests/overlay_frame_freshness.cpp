// Actual TacticalSampler/Motion and verbatim production paint-count gates.
// No Windows/GDI or game-process execution is claimed.
#define main existingEnemyMarkerMain
#include ENEMY_FIXTURE
#undef main
#include DISPLAY_POLICY
#include <array>
using namespace assist;
static uint32_t options=3;
static uint32_t statusOptions(){return options;}
int main(int argc,char**argv){
 const bool fixed=argc>1&&std::string(argv[1])=="--fixed";
 EnemyFixture f;f.now=1000;f.mem.put<double>(EnemyFixture::unit+0x160,1.5);
 Snapshot source=f.sampler.sample(f.now,Capacity,true,3);
 const Snapshot*externalData=&source;
 auto statusFrame=f.sampler.unitStatuses();auto rawEnemyFrame=f.sampler.enemyMarkers();
 struct{bool enemyMarkers=true,enemySpecialOnly=false;int enemySpecialMode=0;}extra;bool externalSourceConfirmed=true;
 visual::Motion statusMotion,enemyMotion;
 visual::LabelView statusViews[MaxLabels],enemyViews[MaxLabels];
 Snapshot input{};input.match=source.match;input.frame=source.frame;input.sampledTick=source.sampledTick;input.cameraX=source.cameraX;input.cameraY=source.cameraY;
 input.labels=statusFrame.count;for(int i=0;i<input.labels;++i)input.label[i]={statusFrame.items[i].id,statusFrame.items[i].x,statusFrame.items[i].y};statusMotion.update(input);
 input.labels=rawEnemyFrame.count;for(int i=0;i<input.labels;++i)input.label[i]={rawEnemyFrame.items[i].id,rawEnemyFrame.items[i].x,rawEnemyFrame.items[i].y};enemyMotion.update(input);
 assert(source.status==1&&statusFrame.count==1&&rawEnemyFrame.count==4);
 auto paintCounts=[&](uint32_t renderNow){
  auto GetTickCount=[&](){return renderNow;};
#include PAINT_GATES
  return std::array<int,2>{statusCount,rawEnemyCount};
 };
 unsigned checks=0;auto expect=[&](uint32_t now,int status,int enemies){auto got=paintCounts(now);assert(got[0]==status&&got[1]==enemies);++checks;};
 expect(1000,1,4);expect(1249,1,4);expect(1250,fixed?1:0,fixed?4:0);expect(2000,fixed?1:0,fixed?4:0);expect(6000,fixed?1:0,fixed?4:0);
 // Confirmation never makes a different world or a previous logic frame current.
 ++source.frame;expect(2000,0,0);--source.frame;
 ++source.match;expect(1000,0,0);expect(2000,0,0);--source.match;
 externalSourceConfirmed=false;expect(1000,1,4);expect(2000,0,0);externalSourceConfirmed=true;
 options=0;extra.enemyMarkers=false;expect(2000,0,0);options=3;extra.enemyMarkers=true;
 externalData=nullptr;expect(1000,0,0);externalData=&source;
 // Both frame types remain bounded even on a confirmed frame.
 statusFrame.count=-1;rawEnemyFrame.count=-1;expect(2000,0,0);
 statusFrame.count=external::status::Maximum+1;rawEnemyFrame.count=external::enemies::Maximum+1;expect(2000,0,0);
 statusFrame.count=1;rawEnemyFrame.count=4;statusFrame.tick=rawEnemyFrame.tick=0;expect(2000,0,0);
 // Unsigned tick age preserves the existing wraparound behavior.
 statusFrame.tick=rawEnemyFrame.tick=0xFFFFFFF0u;externalSourceConfirmed=false;expect(0x20,1,4);expect(0x100,0,0);
 externalSourceConfirmed=true;expect(0x100,fixed?1:0,fixed?4:0);
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":0,\"fixed\":"<<(fixed?"true":"false")<<",\"paused_status_after_250ms\":"<<(fixed?1:0)<<",\"paused_enemies_after_250ms\":"<<(fixed?4:0)<<"}"<<std::endl;
}
