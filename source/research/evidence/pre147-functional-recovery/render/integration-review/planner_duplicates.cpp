#define main existingPlannerDeliveryChecks
#include "/workspace/scratch/40121bc881b8/ra2-ui-146/source/assist/tests/planner_batch_delivery.cpp"
#undef main
int main(){
 Scene scene(1);auto&w=*scene.world;
 w.actors[w.count++]=w.actors[0];
 w.refineries[0].count=2;w.refineries[0].waiting[1]={40021,0,0};
 std::cout<<"validWorld="<<validWorld(w)<<'\n';
 int yields=0;
 for(int frame:{500,530,560}){
  auto batch=scene.step(frame);int n=count(batch,OreYield);yields+=n;
  std::cout<<"frame="<<frame<<" yields="<<n<<" attempts="<<scene.planner.oreYieldAttempts;
  for(const auto&o:batch)if(o.purpose==OreYield)std::cout<<" unit="<<o.unit<<" token="<<o.token<<" destination="<<o.destination;
  std::cout<<'\n';
 }
 std::cout<<"total_yields="<<yields<<" retry_limit="<<ore::MaxDockAttempts<<'\n';
 return 0;
}
